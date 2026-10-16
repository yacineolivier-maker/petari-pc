#pragma once

#include <aurora/aurora.h>

// Reads <user dir>/display.ini (creating it with the defaults) into the Aurora configuration.
void pc_display_configure(AuroraConfig* config, bool forceFullscreen);

// Settings that are applied once Aurora is initialized (alpha to coverage, texture mipmaps).
void pc_display_apply_after_init();
