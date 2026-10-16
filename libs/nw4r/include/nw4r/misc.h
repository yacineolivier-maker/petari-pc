#pragma once

namespace nw4r {
#if TARGET_PC
    typedef uintptr_t IntPtr;
#else
    typedef unsigned long IntPtr;
#endif
    typedef signed long PtrDiff;
};  // namespace nw4r
