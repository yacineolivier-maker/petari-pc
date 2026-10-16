// OSAlarm.cpp - PC implementation of the GameCube OSAlarm API.
// On GameCube alarms fire from the decrementer interrupt. Here one timer thread keeps the pending
// alarms sorted by fire time and calls the handlers. Handlers run on the timer thread, so they may
// only do what the game does from interrupt context (resume/wake threads, set flags).

#include <dolphin/dolphin.h>
#include <dolphin/os.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "pc/main.h"
#include "pc/os.h"

namespace {

struct AlarmQueue {
    std::mutex mtx;
    std::condition_variable cv;      // wakes the timer thread (queue changed)
    std::condition_variable doneCv;  // signals the end of a handler call
    std::vector<OSAlarm*> pending;   // sorted by fire time, earliest first
    OSAlarm* firing = nullptr;
    std::thread::id timerThreadId;
    bool started = false;
};

AlarmQueue& GetQueue() {
    // Lazy-initialized to stay safe during static init.
    static AlarmQueue* queue = new AlarmQueue();
    return *queue;
}

// Must be called with the queue locked.
void RemoveLocked(AlarmQueue& q, OSAlarm* alarm) {
    for (auto it = q.pending.begin(); it != q.pending.end(); ++it) {
        if (*it == alarm) {
            q.pending.erase(it);
            return;
        }
    }
}

// Must be called with the queue locked.
void InsertLocked(AlarmQueue& q, OSAlarm* alarm) {
    auto it = q.pending.begin();
    while (it != q.pending.end() && (*it)->fire <= alarm->fire) {
        ++it;
    }
    q.pending.insert(it, alarm);
}

void TimerThreadMain() {
    AlarmQueue& q = GetQueue();
    OSSetCurrentThreadName("OSAlarm timer");
    static OSContext sDummyContext;

    std::unique_lock lock(q.mtx);
    while (!pc::IsShuttingDown) {
        if (q.pending.empty()) {
            // Poll the shutdown flag now and then; no alarm can be due while the queue is empty.
            q.cv.wait_for(lock, std::chrono::milliseconds(100));
            continue;
        }

        OSAlarm* alarm = q.pending.front();
        OSTime now = OSGetTime();
        if (now < alarm->fire) {
            OSTime ticks = alarm->fire - now;
            s64 us = OSTicksToMicroseconds(ticks);
            if (us > 100000) {
                us = 100000;
            }
            q.cv.wait_for(lock, std::chrono::microseconds(us < 1 ? 1 : us));
            continue;
        }

        q.pending.erase(q.pending.begin());
        OSAlarmHandler handler = alarm->handler;
        if (alarm->period > 0) {
            // Reschedule before calling the handler so the handler (or another thread) can cancel it.
            OSTime next = alarm->fire + alarm->period;
            if (next <= now) {
                // Fell behind: skip the missed periods, like the hardware alarm would.
                next = alarm->fire + alarm->period * ((now - alarm->fire) / alarm->period + 1);
            }
            alarm->fire = next;
            InsertLocked(q, alarm);
        } else {
            alarm->handler = nullptr;
        }

        if (handler == nullptr) {
            continue;
        }

        // Don't touch a one-shot alarm after its handler runs: it may live on a stack frame that
        // the woken thread is about to leave (JFWDisplay::threadSleep).
        q.firing = alarm;
        lock.unlock();
        std::memset(&sDummyContext, 0, sizeof(sDummyContext));
        handler(alarm, &sDummyContext);
        lock.lock();
        q.firing = nullptr;
        q.doneCv.notify_all();
    }
}

void EnsureTimerThreadLocked(AlarmQueue& q) {
    if (q.started) {
        return;
    }
    q.started = true;
    std::thread thread(TimerThreadMain);
    q.timerThreadId = thread.get_id();
    thread.detach();
}

void ScheduleLocked(AlarmQueue& q, OSAlarm* alarm, OSAlarmHandler handler) {
    RemoveLocked(q, alarm);
    alarm->handler = handler;
    InsertLocked(q, alarm);
    EnsureTimerThreadLocked(q);
    q.cv.notify_all();
}

}  // namespace

extern "C" {

void OSInitAlarm(void) {}

BOOL OSCheckAlarmQueue(void) {
    return TRUE;
}

void OSCreateAlarm(OSAlarm* alarm) {
    if (alarm == nullptr) {
        return;
    }
    AlarmQueue& q = GetQueue();
    std::lock_guard lock(q.mtx);
    RemoveLocked(q, alarm);
    alarm->handler = nullptr;
    alarm->tag = 0;
    alarm->fire = 0;
    alarm->prev = nullptr;
    alarm->next = nullptr;
    alarm->period = 0;
    alarm->start = 0;
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    if (alarm == nullptr) {
        return;
    }
    AlarmQueue& q = GetQueue();
    std::lock_guard lock(q.mtx);
    alarm->period = 0;
    alarm->fire = OSGetTime() + tick;
    ScheduleLocked(q, alarm, handler);
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) {
    if (alarm == nullptr) {
        return;
    }
    AlarmQueue& q = GetQueue();
    std::lock_guard lock(q.mtx);
    alarm->period = 0;
    alarm->fire = time;
    ScheduleLocked(q, alarm, handler);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    if (alarm == nullptr || period <= 0) {
        return;
    }
    AlarmQueue& q = GetQueue();
    std::lock_guard lock(q.mtx);
    alarm->period = period;
    alarm->start = start;
    OSTime now = OSGetTime();
    if (start < now) {
        // Same rule as the SDK: the first fire is the next multiple of the period after now.
        alarm->fire = start + period * ((now - start) / period + 1);
    } else {
        alarm->fire = start;
    }
    ScheduleLocked(q, alarm, handler);
}

void OSCancelAlarm(OSAlarm* alarm) {
    if (alarm == nullptr) {
        return;
    }
    AlarmQueue& q = GetQueue();
    std::unique_lock lock(q.mtx);
    RemoveLocked(q, alarm);
    alarm->handler = nullptr;
    alarm->period = 0;
    // If its handler is running on the timer thread right now, wait for it to finish so the caller
    // can free the alarm afterwards. (A handler cancelling its own alarm doesn't wait.)
    if (std::this_thread::get_id() != q.timerThreadId) {
        q.doneCv.wait(lock, [&] { return q.firing != alarm || pc::IsShuttingDown; });
    }
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag) {
    if (alarm != nullptr) {
        alarm->tag = tag;
    }
}

void OSCancelAlarms(u32 tag) {
    if (tag == 0) {
        return;
    }
    std::vector<OSAlarm*> toCancel;
    {
        AlarmQueue& q = GetQueue();
        std::lock_guard lock(q.mtx);
        for (OSAlarm* alarm : q.pending) {
            if (alarm->tag == tag) {
                toCancel.push_back(alarm);
            }
        }
    }
    for (OSAlarm* alarm : toCancel) {
        OSCancelAlarm(alarm);
    }
}

}  // extern "C"
