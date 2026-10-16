#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void OSSetCurrentThreadName(const char* name);

// Called by blocking OS primitives: lets a creator that resumed this (higher-priority) thread
// continue, like the Wii scheduler would (see OSResumeThread).
void PCThreadBlockingPoint(void);

// Makes `thread` the OSThread of the calling native thread (call once, early, from the main thread).
struct OSThread;
void OSPCAdoptCurrentThread(struct OSThread* thread);

#ifdef __cplusplus
}
#endif
