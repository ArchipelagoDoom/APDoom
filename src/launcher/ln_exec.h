
#ifndef __LN_EXEC_H__
#define __LN_EXEC_H__

#include "doomtype.h"

#include "apdoom.h"

// Executes a game, hiding the GUI until it returns.
void LN_ExecuteGame(const ap_savesettings_t *settings);

// Executes the setup program, hiding the GUI until it returns.
void LN_ExecuteSetup(void);

// Executes a game using the same command line arguments passed to the launcher.
// Used for quick passthrough, without starting the GUI. As such, never returns.
void NORETURN LN_ImmediateExecute(const ap_worldinfo_t *world);

#endif // __LN_EXEC_H__
