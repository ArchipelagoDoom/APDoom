@ECHO off

SET "DEP_MISS=0"
if not exist "Release\libfluidsynth-3.dll"  ECHO Missing dependency: libfluidsynth-3.dll & SET "DEP_MISS=1"
if not exist "Release\libgcc_s_sjlj-1.dll"  ECHO Missing dependency: libgcc-s_sjlj-1.dll & SET "DEP_MISS=1"
if not exist "Release\libglib-2.0-0.dll"    ECHO Missing dependency: libglib-2.0-0.dll & SET "DEP_MISS=1"
if not exist "Release\libgobject-2.0-0.dll" ECHO Missing dependency: libgobject-2.0-0.dll & SET "DEP_MISS=1"
if not exist "Release\libgomp-1.dll"        ECHO Missing dependency: libgomp-1.dll & SET "DEP_MISS=1"
if not exist "Release\libgthread-2.0-0.dll" ECHO Missing dependency: libgthread-2.0-0.dll & SET "DEP_MISS=1"
if not exist "Release\libinstpatch-2.dll"   ECHO Missing dependency: libinstpatch-2.dll & SET "DEP_MISS=1"
if not exist "Release\libintl-8.dll"        ECHO Missing dependency: libintl-8.dll & SET "DEP_MISS=1"
if not exist "Release\libsndfile-1.dll"     ECHO Missing dependency: libsndfile-1.dll & SET "DEP_MISS=1"
if not exist "Release\libstdc++-6.dll"      ECHO Missing dependency: libstdc++-6.dll & SET "DEP_MISS=1"
if not exist "Release\libwinpthread-1.dll"  ECHO Missing dependency: libwinpthread-1.dll & SET "DEP_MISS=1"
if not exist "Release\samplerate.dll"       ECHO Missing dependency: samplerate.dll & SET "DEP_MISS=1"
if not exist "Release\SDL2.dll"             ECHO Missing dependency: SDL2.dll & SET "DEP_MISS=1"
if not exist "Release\SDL2_mixer.dll"       ECHO Missing dependency: SDL2_mixer.dll & SET "DEP_MISS=1"
if not exist "Release\zlib1.dll"            ECHO Missing dependency: zlib1.dll & SET "DEP_MISS=1"

if %DEP_MISS% neq 0 ECHO Please place the above missing dependencies into "Release\" and then retry. & EXIT /b 1

@ECHO Updating...
git pull
git submodule update --recursive

@ECHO Compiling archipelago-doom...
MSBUILD /v:m "build\Archipelago Doom.sln" /t:archipelago-doom /p:Configuration="Release" /p:Platform="x64" || EXIT /b %errorlevel%
COPY "build\bin\Release\archipelago-doom.exe" "Release\"

@ECHO Compiling archipelago-heretic...
MSBUILD /v:m "build\Archipelago Doom.sln" /t:archipelago-heretic /p:Configuration="Release" /p:Platform="x64" || EXIT /b %errorlevel%
COPY "build\bin\Release\archipelago-heretic.exe" "Release\"

@ECHO Compiling apdoom-setup...
MSBUILD /v:m "build\Archipelago Doom.sln" /t:apdoom-setup /p:Configuration="Release" /p:Platform="x64" || EXIT /b %errorlevel%
COPY "build\bin\Release\apdoom-setup.exe" "Release\"

@ECHO Compiling apdoom-launcher...
MSBUILD /v:m "build\Archipelago Doom.sln" /t:apdoom-launcher /p:Configuration="Release" /p:Platform="x64" || EXIT /b %errorlevel%
COPY "build\bin\Release\apdoom-launcher.exe" "Release\"

@ECHO Copying other files...
COPY "build\bin\Release\APCpp.dll" "Release\"
COPY "CREDITS" "Release\credits.txt"
COPY "COPYING.md" "Release\license.txt"

@ECHO Archiving...
CD "Release"
7z a "../apdoom-Win-x64.zip" *.* || EXIT /b %errorlevel%
CD ".."

MOVE "apdoom-Win-x64.zip" "K:\"
@ECHO apdoom-Win-x64.zip placed in shared folder drive_k.