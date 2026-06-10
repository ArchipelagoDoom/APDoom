# Archipelago Doom 2.0

![Upstream](https://img.shields.io/badge/upstream-Crispy_Doom_7.1.0-3b849c)
![Release](https://img.shields.io/github/v/release/archipelagodoom/apdoom?include_prereleases)
![Commits Since Release](https://img.shields.io/github/commits-since/archipelagodoom/apdoom/latest?include_prereleases)

Archipelago Doom is a fork of [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom), designed to play randomized MultiWorld games through [Archipelago](archipelago.gg).

## Running

### Windows

1. Download Archipelago Doom from the [release page](https://github.com/ArchipelagoDoom/APDoom/releases/tag/v2.0.0-beta3).
   Windows users will want to download `apdoom-Windows-x64.zip`.
2. Extract the contents of the zip file into a new folder.
3. Run `apdoom-launcher.exe`.
4. Rip and tear!

### Linux

We distribute an AppImage for Linux users which should function regardless of the distro being used.

1. Download Archipelago Doom from the [release page](https://github.com/ArchipelagoDoom/APDoom/releases/tag/v2.0.0-beta3).
   Linux users will want to download `APDoom-Linux-x86_64.AppImage`.
2. Make the AppImage executable if it isn't already. (`chmod +x APDoom-Linux-x86_64.AppImage`)
3. Run the AppImage.
4. Rip and tear!

### WAD files

Archipelago Doom should automatically detect the WAD files from the Steam releases of DOOM + DOOM II and Heretic + Hexen, if they are installed. Otherwise, copy any additional WAD files into the same directory as Archipelago Doom.

Archipelago Doom works best with the original DOS versions of the IWADs, not the rerelease versions. This means if you're copying from, say, the Steam release of DOOM + DOOM II, you want to copy the WAD files from the `/dos/base/` folder. While Archipelago Doom should still function with other WAD files, there may be visual glitches and support is not guaranteed.

## Compiling

**You should not need to compile the executable yourself just to play Archipelago Doom.**
Compiling is for those that wish to work on the game, fix bugs, etc.

**Do not attempt to use automake; the build files are left over from Crispy Doom and are nonfunctional.**
CMake is the only supported method of generating project files.

- Clone the repository: `git clone https://github.com/ArchipelagoDoom/APDoom`
- Initialize submodules: `git submodule update --init --recursive`
- Place any APWorlds that you want to be available by default in the `embed` directory.
  - **Beta note:** No APWorlds are currently included by default; ideally, you should generate some using ap_gen_tool, but you can take the officially distributed Beta APWorlds and place them in there too.
- Create a `build` directory inside the repository directory, and change directories into it.
- Use CMake to generate project files: `cmake ..`

### Libraries

I'm sorry in advance for how frustrating CMake can be to set up on Windows.
You will likely need to set the include and library directories up manually; using the CMake GUI may make things easier.

|Library|Required|Version|
|-|-|-|
|SDL2|**required**|>= 2.0.18|
|SDL2_mixer|**required**|>= 2.6.3|
|zlib|**required**|any recent version|
|libpng|_optional_|any recent version|
|libfluidsynth|_optional_|>= 2.3.2|
|libsamplerate|_optional_|0.2.2|

Once libraries have been set up, use CMake to generate project files for your platform.
For Windows you probably want to generate VS2022 project files, and for Linux you'll probably just want to generate a Makefile.

## ap_gen_tool

Archipelago Doom reads worlds generated with [ap_gen_tool](https://github.com/ArchipelagoDoom/ap_gen_tool) to determine which locations are available, what tweaks to maps need to be applied, etc. See the ap_gen_tool repository for more information.

## Legalese

Doom is © 1993-1996 Id Software, Inc.; 
Boom 2.02 is © 1999 id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman;
PrBoom+ is © 1999 id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman,
© 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze,
© 2005-2006 Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko;
Chocolate Doom is © 1993-1996 Id Software, Inc., © 2005 Simon Howard; 
Chocolate Hexen is © 1993-1996 Id Software, Inc., © 1993-2008 Raven Software, © 2008 Simon Howard;
Strawberry Doom is © 1993-1996 Id Software, Inc., © 2005 Simon Howard, © 2008-2010 GhostlyDeath; 
Crispy Doom is additionally © 2014-2019 Fabian Greffrath;
Archipelago Doom is additionally © 2023-2024 David "Daivuk" St-Louis, © 2024-2026 Kay "Kaito" Sinclaire;
all of the above are released under the [GPL-2+](https://www.gnu.org/licenses/gpl-2.0.html).

SDL 2.0, SDL_mixer 2.0 and SDL_net 2.0 are © 1997-2016 Sam Lantinga and are released under the [zlib license](http://www.gzip.org/zlib/zlib_license.html).

Secret Rabbit Code (libsamplerate) is © 2002-2011 Erik de Castro Lopo and is released under the [GPL-2+](http://www.gnu.org/licenses/gpl-2.0.html).
Libpng is © 1998-2014 Glenn Randers-Pehrson, © 1996-1997 Andreas Dilger, © 1995-1996 Guy Eric Schalnat, Group 42, Inc. and is released under the [libpng license](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt).
Zlib is © 1995-2013 Jean-loup Gailly and Mark Adler and is released under the [zlib license](http://www.zlib.net/zlib_license.html).

The Archipelago logo is © 2022 Krista Corkos and Christopher Wilson and is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International license](http://creativecommons.org/licenses/by-nc/4.0/).
