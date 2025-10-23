#!/bin/bash

usage() {
  echo -e "-h,--help            Shows help and exits"
  echo -e "-l,--linuxdeploy     Sets path to linuxdeploy, optional (download if not set)"
  echo -e "-o,--output          Sets output file name, optional"
  exit 1
}

GETOPT=$(getopt -s 'bash' -o 'h,l:,o:' -l 'help,linuxdeploy:,output:' -n 'appimage.sh' -- "$@")
if [ $? -ne 0 ]; then exit 1; fi
eval set -- "$GETOPT"

LINUXDEPLOY_PATH="download"
export LDAI_OUTPUT="bin/apdoom-x86_64.AppImage"

while true; do
  case "$1" in
    -h|--help)
      usage
    ;;
    -l|--linuxdeploy)
      LINUXDEPLOY_PATH=$2
      shift 2;
    ;;
    -o|--output)
      export LDAI_OUTPUT=$2
      shift 2;
    ;;
    --)
      shift;
      break;
    ;;
  esac
done

cd build || exit 1

BUILD_READY=1
if [ ! -f bin/archipelago-doom ]; then BUILD_READY=0; fi
if [ ! -f bin/archipelago-heretic ]; then BUILD_READY=0; fi
if [ ! -f bin/apdoom-setup ]; then BUILD_READY=0; fi
if [ ! -f bin/apdoom-launcher ]; then BUILD_READY=0; fi
if [ ! -f lib/libAPCpp.so ]; then BUILD_READY=0; fi

if [ "$BUILD_READY" -eq 0 ]; then
  echo "Please build APDoom first."
  exit 1
fi

if [ "$LINUXDEPLOY_PATH" == "download" ]; then
  wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" -nc || exit 1
  chmod +x linuxdeploy-x86_64.AppImage
  LINUXDEPLOY_PATH="./linuxdeploy-x86_64.AppImage"
fi

rm -rf ./AppDir
mkdir -p ./AppDir/usr/bin
mkdir -p ./AppDir/usr/lib

cp bin/archipelago-doom ./AppDir/usr/bin || exit 1
cp bin/archipelago-heretic ./AppDir/usr/bin || exit 1
cp bin/apdoom-setup ./AppDir/usr/bin || exit 1
cp bin/apdoom-launcher ./AppDir/usr/bin || exit 1
cp lib/libAPCpp.so ./AppDir/usr/lib || exit 1
cp ../data/launcher_icon.png ./AppDir/apdoom.png || exit 1
cp ../data/AppImage/apdoom.desktop ./AppDir/apdoom.desktop || exit 1
cp ../data/AppImage/run_launcher.sh ./AppDir/usr/bin || exit 1
cp ../CREDITS ./AppDir || exit 1

"$LINUXDEPLOY_PATH" --appdir=AppDir --desktop-file=AppDir/apdoom.desktop --icon-file=AppDir/apdoom.png --custom-apprun=AppDir/usr/bin/run_launcher.sh --output=appimage || exit 1
