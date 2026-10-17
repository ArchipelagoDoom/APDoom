**General Fixes and Improvements**

- Expand extended BSP format support and blockmap building to all games
  (@elf-alchemist).
- Fix nodes decompression buffer reallocation in all games.
- Allocate texture composites on demand with PU_LEVEL tag, fixing a crash in
  R_GenerateComposite in all games (@Noseey).
- Rework clean screenshots: always refresh the SDL backbuffer before taking
  a screenshot (@Noseey).
- Only suppress fast polling when automap follow mode is off (@mikeday0).
- Fully remove the visplanes and drawseg limits in Hexen and Strife
  (@elf-alchemist).
- Add overflow guards for flattranslation when loading savegames in Heretic,
  Hexen and Strife (@Noseey).

**Crispy Doom**

- Add support for the 200 dummy SFX added by DEHEXTRA.
- Improve stat counting: stats are now identical for all players in netgames,
  and monsters resurrected by Arch-Viles or spawned by the Icon of Sin
  (Nightmare/`-respawn`) are counted (@gera2342).
- Include `-coop_spawns` in the demo footer (@gera2342).
- Fix lowres_turn not being updated when joining demos recorded with shorttics
  (@Noseey).
- Fix GotoNextLevel storing the wrong map in the demo when recording
  (@Noseey).
- Use the new naming scheme for Masterlevels KEX music, and make the
  Masterlevels KEX detection more robust (@Noseey).
- Don't preserve palette index 109 for CR_DARK (@KScl).
- Fix out-of-bounds read when resizing the status bar with a ready weapon that
  uses no ammo (@KScl).
- Make sure the patch name buffer is terminated in R_InitTextures().

**Crispy Heretic**

- Fix demo desync in P_ExplodeMissileSafe().
- Check for a NULL thing->state in R_AddSprites() (@Noseey).
- Move demo recording to the actual map init and prepare the frame in
  P_SetupLevel (@Noseey).
- Fix key_lookcenter being blocked by joystick_look_sensitivity when using
  spring-centered freelook (@Noseey).
- Add clean screenshot support (@Noseey).
- Add `-coop_spawns` to the demo footer (@gera2342).

**Crispy Hexen**

- Add support for sideloading Deathkings of the Dark Citadel (DKDC),
  including SNDINFO, CLUSxMSG and WINxMSG (@Noseey).
- Optimize sideloading, fix the wrong demo reel in the episode selection and
  detect HEXDD.WAD differently (@Noseey).
- Fix demo issues: re-init episode 1 on Endgame and use the start map for new
  games during recording (@Noseey).
- Initialize S_InitScript and InitMapInfo in SV_LoadGame (@Noseey).
- Move the world timer when powers are active to avoid overlapping (@Noseey).
- Add GotoNextLevel and ReloadLevel with demo suffix (@Noseey).
- Move demo recording to the actual map init and prepare the frame in
  P_SetupLevel (@Noseey).
- Fix key_lookcenter being blocked by joystick_look_sensitivity (@Noseey).
- Add clean screenshot support (@Noseey).

**Crispy Strife**

- Add translucency option (@Noseey).
- Add accessibility features: weapon flash lighting, palette changes,
  ultimate weapon palette, extra lighting, and the startup scream with the
  graphical startup (@Noseey).
- Add fullscreen HUD variations, including translucent HUDs (@Noseey).
- Add demo fast-forward (@Noseey).
- Add GotoNextLevel and ReloadLevel with demo suffix (@Noseey).
- Apply the joystick fire delay to dialogs and menus (@mgorny).
