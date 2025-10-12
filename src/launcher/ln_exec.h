
#ifndef __LN_EXEC_H__
#define __LN_EXEC_H__

#include "doomtype.h"

#include "apdoom.h"

typedef struct {
	char slot_name[16 + 1]; // -applayerhex <s>
	char address[128 + 1]; // -apserver <s>
	char password[128 + 1]; // -password <s>

	int practice_mode; // -practice

	// Overrides of server options
	int monster_rando; // -apmonsterrando <n>
	int item_rando; // -apitemrando <n>
	int music_rando; // -apmusicrando <n>
	int flip_levels; // -apfliplevels <n>
	int reset_level; // -apresetlevelondeath <n>
	int no_deathlink; // -apdeathlinkoff
} gamesettings_t;

extern gamesettings_t exec_settings;

// Executes a game, hiding the GUI until it returns.
void LN_ExecuteWorld(const ap_worldinfo_t *world);

// Executes the setup program, hiding the GUI until it returns.
void LN_ExecuteSetup(void);

// Executes a game using the same command line arguments passed to the launcher.
// Used for quick passthrough, without starting the GUI. As such, never returns.
void NORETURN LN_ImmediateExecute(const ap_worldinfo_t *world);

#endif // __LN_EXEC_H__
