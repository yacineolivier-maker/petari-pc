// Prints a symbolized stack trace on an unhandled exception (Windows), so crashes in the port can
// be located without a debugger attached.
#include "pc/crash_handler.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstdlib>

// Look for the .pdb next to the executable first: the path embedded in the exe points at the
// build directory, which may hold a newer build than a copied executable.
static const char* pc_symbol_path() {
    static char path[MAX_PATH * 2];
    if (path[0] == '\0') {
        char exe[MAX_PATH];
        DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
        while (n > 0 && exe[n - 1] != '\\' && exe[n - 1] != '/') {
            n--;
        }
        exe[n] = '\0';
        std::snprintf(path, sizeof(path), "%s;.", exe);
    }
    return path;
}

static void pc_print_stack(HANDLE process, HANDLE thread, CONTEXT ctx) {
    if (ctx.Rip == 0 && ctx.Rsp != 0) {
        // A call through a NULL function pointer: unwind from the return address it pushed.
        ctx.Rip = *(DWORD64*)ctx.Rsp;
        ctx.Rsp += 8;
        std::fprintf(stderr, "  (called a NULL function pointer from below)\n");
    }
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbuf[sizeof(SYMBOL_INFO) + 512];
    for (int i = 0; i < 48; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }
        DWORD64 addr = frame.AddrPC.Offset;
        if (addr == 0) {
            break;
        }
        auto* sym = (SYMBOL_INFO*)symbuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        const char* name = SymFromAddr(process, addr, &disp, sym) ? sym->Name : "?";
        if (SymGetLineFromAddr64(process, addr, &lineDisp, &line)) {
            std::fprintf(stderr, "  #%02d %s  (%s:%lu)\n", i, name, line.FileName, line.LineNumber);
        } else {
            std::fprintf(stderr, "  #%02d %s +0x%llx  [%p]\n", i, name, (unsigned long long)disp,
                         (void*)addr);
        }
    }
}

// Raw frames as offsets into smgpc.exe, printed before anything that allocates (symbolization
// can itself crash when the heap is corrupted). Resolve them offline from smgpc.pdb.
static void pc_print_raw_frames(const CONTEXT& context) {
    const DWORD64 base = (DWORD64)GetModuleHandleW(nullptr);
    CONTEXT ctx = context;
    std::fprintf(stderr, "  raw frames (smgpc.exe RVA):");
    for (int i = 0; i < 24 && ctx.Rip != 0; i++) {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (imageBase == base) {
            std::fprintf(stderr, " %llx", (unsigned long long)(ctx.Rip - base));
        } else {
            std::fprintf(stderr, " ext:%llx", (unsigned long long)ctx.Rip);
        }
        if (fn == nullptr) {
            ctx.Rip = *(DWORD64*)ctx.Rsp;
            ctx.Rsp += 8;
        } else {
            void* handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
        }
    }
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

static LONG WINAPI pc_unhandled_exception(EXCEPTION_POINTERS* info) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    EXCEPTION_RECORD* rec = info->ExceptionRecord;
    std::fflush(stdout);
    std::fprintf(stderr, "\n=== Unhandled exception 0x%08lX at %p", rec->ExceptionCode,
                 rec->ExceptionAddress);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        std::fprintf(stderr, " (%s address %p)", rec->ExceptionInformation[0] ? "writing" : "reading",
                     (void*)rec->ExceptionInformation[1]);
    }
    std::fprintf(stderr, " ===\n");
    pc_print_raw_frames(*info->ContextRecord);

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, pc_symbol_path(), TRUE);

    pc_print_stack(process, thread, *info->ContextRecord);
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

// abort() (Aurora's FATAL, failed CRT checks) doesn't raise an SEH exception: print the stack too.
static void pc_abort_handler(int) {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, pc_symbol_path(), TRUE);
    CONTEXT ctx{};
    RtlCaptureContext(&ctx);
    std::fflush(stdout);
    std::fprintf(stderr, "\n=== abort() ===\n");
    pc_print_stack(process, GetCurrentThread(), ctx);
    std::fflush(stderr);
    TerminateProcess(process, 3);
}

// Prints the calling thread's stack (for diagnostics, e.g. when the game asks to exit).
void pc_print_current_stack(const char* why) {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, pc_symbol_path(), TRUE);
    CONTEXT ctx{};
    RtlCaptureContext(&ctx);
    std::fflush(stdout);
    std::fprintf(stderr, "\n=== %s ===\n", why);
    pc_print_stack(process, GetCurrentThread(), ctx);
    std::fflush(stderr);
}

// Integer division by zero: the Wii's divw/divwu don't trap (the quotient is 0, or -1 for a
// negative dividend with divw, so the remainder the code then computes is the dividend). Some game
// code relies on it (e.g. FallOutFieldDraw). Emulate that for the faulting x86 div/idiv and skip it.
static int pc_div_insn_length(const uint8_t* p, bool* is64, bool* isSigned, bool* is8) {
    int len = 0;
    *is64 = false;
    if (p[len] == 0x66) {
        len++;
    }
    if ((p[len] & 0xF0) == 0x40) {  // REX
        *is64 = (p[len] & 0x08) != 0;
        len++;
    }
    if (p[len] != 0xF7 && p[len] != 0xF6) {
        return 0;
    }
    *is8 = p[len] == 0xF6;
    len++;
    const uint8_t modrm = p[len++];
    const int mod = modrm >> 6, reg = (modrm >> 3) & 7, rm = modrm & 7;
    if (reg != 6 && reg != 7) {  // /6 div, /7 idiv
        return 0;
    }
    *isSigned = reg == 7;
    if (mod != 3 && rm == 4) {
        const uint8_t sib = p[len++];
        if (mod == 0 && (sib & 7) == 5) {
            len += 4;
        }
    }
    if (mod == 1) {
        len += 1;
    } else if (mod == 2 || (mod == 0 && rm == 5)) {
        len += 4;
    }
    return len;
}

static LONG WINAPI pc_divide_by_zero_handler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    CONTEXT* ctx = info->ContextRecord;
    bool is64 = false, isSigned = false, is8 = false;
    const int len = pc_div_insn_length(reinterpret_cast<const uint8_t*>(ctx->Rip), &is64, &isSigned, &is8);
    if (len == 0 || is8) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static volatile LONG sReports = 0;
    if (InterlockedIncrement(&sReports) <= 5) {
        std::fprintf(stderr, "[pc] integer division by zero at smgpc.exe+%llx: Wii result emulated\n",
                     (unsigned long long)(ctx->Rip - (DWORD64)GetModuleHandleW(nullptr)));
    }
    if (is64) {
        const int64_t dividend = (int64_t)ctx->Rax;
        ctx->Rdx = ctx->Rax;
        ctx->Rax = (isSigned && dividend < 0) ? (DWORD64)-1 : 0;
    } else {
        const int32_t dividend = (int32_t)(uint32_t)ctx->Rax;
        ctx->Rdx = (uint32_t)ctx->Rax;
        ctx->Rax = (isSigned && dividend < 0) ? 0xFFFFFFFFull : 0;
    }
    ctx->Rip += len;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void pc_install_crash_handler() {
    AddVectoredExceptionHandler(1, pc_divide_by_zero_handler);
    SetUnhandledExceptionFilter(pc_unhandled_exception);
    std::signal(SIGABRT, pc_abort_handler);
}

// SMGPC_WATCHDOG=N: if the thread calling pc_watchdog_tick() stops ticking for N seconds, print its
// stack (to find where the game hangs) and exit.
static HANDLE s_watchThread;
static volatile LONG s_watchTicks;

static DWORD WINAPI pc_watchdog_main(void* param) {
    DWORD seconds = (DWORD)(uintptr_t)param;
    LONG last = -1;
    DWORD still = 0;
    for (;;) {
        Sleep(1000);
        LONG now = s_watchTicks;
        still = now == last ? still + 1 : 0;
        last = now;
        if (still < seconds) {
            continue;
        }
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, pc_symbol_path(), TRUE);
        SuspendThread(s_watchThread);
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_FULL;
        GetThreadContext(s_watchThread, &ctx);
        std::fflush(stdout);
        std::fprintf(stderr, "\n=== Watchdog: no frame for %lu s (tick %ld) ===\n", seconds, now);
        pc_print_stack(process, s_watchThread, ctx);
        std::fflush(stderr);
        TerminateProcess(process, 3);
    }
}

// SMGPC_DUMP_THREADS=N: N seconds after start, print the stack of every thread (to find which
// thread is stuck where), then keep running.
static DWORD WINAPI pc_dump_threads_main(void* param) {
    Sleep((DWORD)(uintptr_t)param * 1000);
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, pc_symbol_path(), TRUE);
    DWORD self = GetCurrentThreadId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    std::fflush(stdout);
    for (BOOL ok = Thread32First(snap, &entry); ok; ok = Thread32Next(snap, &entry)) {
        if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == self) {
            continue;
        }
        HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
        if (thread == nullptr) {
            continue;
        }
        SuspendThread(thread);
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_FULL;
        GetThreadContext(thread, &ctx);
        std::fprintf(stderr, "\n=== Thread %lu ===\n", entry.th32ThreadID);
        pc_print_stack(process, thread, ctx);
        ResumeThread(thread);
        CloseHandle(thread);
    }
    CloseHandle(snap);
    std::fflush(stderr);
    return 0;
}

static void pc_start_thread_dump() {
    static bool started = false;
    const char* env = std::getenv("SMGPC_DUMP_THREADS");
    if (started || env == nullptr) {
        return;
    }
    started = true;
    CreateThread(nullptr, 0, pc_dump_threads_main, (void*)(uintptr_t)std::strtoul(env, nullptr, 10), 0, nullptr);
}

void pc_watchdog_tick() {
    pc_start_thread_dump();
    if (s_watchThread == nullptr) {
        const char* env = std::getenv("SMGPC_WATCHDOG");
        DWORD seconds = env != nullptr ? (DWORD)std::strtoul(env, nullptr, 10) : 0;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_watchThread,
                        0, FALSE, DUPLICATE_SAME_ACCESS);
        if (seconds != 0) {
            CreateThread(nullptr, 0, pc_watchdog_main, (void*)(uintptr_t)seconds, 0, nullptr);
        }
    }
    InterlockedIncrement(&s_watchTicks);
}

// Install the handler before the game's own static constructors run (they can crash too).
#pragma warning(disable : 4073)
#pragma init_seg(lib)
static struct PcCrashHandlerInstaller {
    PcCrashHandlerInstaller() {
        pc_install_crash_handler();
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }
} s_pcCrashHandlerInstaller;
#else
void pc_install_crash_handler() {}
void pc_print_current_stack(const char*) {}
void pc_watchdog_tick() {}
#endif
