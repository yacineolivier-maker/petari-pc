#pragma once
// Files in the player's user directory, %APPDATA%\Super Mario Galaxy (see pc/src/os/user_files.cpp).

#ifdef __cplusplus
extern "C" {
#endif

// The user directory (UTF-8, with a trailing separator). Created on first use.
const char* pc_user_dir(void);

// Call first thing in main: when the game has no console (started from a shortcut), send
// stdout/stderr to logs\smgpc-<date>.log.
void pc_log_file_init(void);

#ifdef __cplusplus
}
#endif
