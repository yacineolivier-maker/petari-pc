#pragma once

// Installs a handler that prints a symbolized stack trace on crashes.
void pc_install_crash_handler();

// Prints the calling thread's stack with a title.
void pc_print_current_stack(const char* why);

// Call once per frame; with SMGPC_WATCHDOG=N set, a stall of N seconds prints the stack.
void pc_watchdog_tick();
