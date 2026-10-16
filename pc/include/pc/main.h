#pragma once
// Process-wide state of the PC port.

namespace pc {
// Set when the game is exiting; blocking OS primitives wake up and return.
extern bool IsShuttingDown;
}  // namespace pc
