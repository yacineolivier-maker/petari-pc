#pragma once

// Installs a handler that prints a symbolized stack trace on crashes.
void pc_install_crash_handler();

// Call once per frame; with SMGPC_WATCHDOG=N set, a stall of N seconds prints the stack.
void pc_watchdog_tick();
