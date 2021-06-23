#!/bin/bash

set -e

# Detect OS
# from: http://stackoverflow.com/a/8597411/142317
if   [[ "$OSTYPE" == "linux-gnu" ]]; then OS="unix"
elif [[ "$OSTYPE" == "darwin"* ]];   then OS="unix"
elif [[ "$OSTYPE" == "cygwin" ]];    then OS="win"
elif [[ "$OSTYPE" == "msys" ]];      then OS="win"
elif [[ "$OSTYPE" == "win32" ]];     then OS="win"
elif [[ "$OSTYPE" == "freebsd"* ]];  then OS="unix"
else
  OS="unix"
  echo "Unable to detect OS, assuming unix..."
fi

# Configure OS vars
if [[ "$OS" == "unix" ]]; then
  EXT=
else
  EXT=.exe
fi

type python3;
if [[ $? -ne 0 ]]; then
	echo "python3 not found; python is required for the meson build system"
	exit 1;
fi

type meson;
if [[ $? -ne 0 ]]; then
	echo "meson not found; attempting to install..."
	type pip3;
	if [[ $? -ne 0 ]]; then
		echo "pip3 not found; unable to automatically install meson"
		exit 1;
	fi;
	pip3 install --user meson;
	path=$path;'~/.local/bin';
fi;

echo
echo "Building engine..."
pushd engine
	if [[ -d "build" ]]; then
		meson --reconfigure build
	else
		meson build
	fi;
	pushd build
		ninja
	popd
popd

echo
echo "Copying engine to game directory..."
mkdir -p game
rm -f game/blinky$EXT
cp engine/build/blinky$EXT game/blinky$EXT

echo
echo -e "\033[32mSuccessfully built!\033[0m"
echo -e "  Run \033[36mplay.sh\033[0m to starting playing!"
echo
