#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void OSSetCurrentThreadName(const char* name);

// Makes `thread` the OSThread of the calling native thread (call once, early, from the main thread).
struct OSThread;
void OSPCAdoptCurrentThread(struct OSThread* thread);

#ifdef __cplusplus
}
#endif
