## Windows

1. Get [MinGW installer](http://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download), and install the following with the package manager:
  - gcc
  - lua
  - sdl2
  - sdl2image
  - cmocka
  - meson
1. Install msys
1. Add mingw/bin and msys/bin to system path
1. Open msys/bin/msys to open command line

```sh
./build.sh
./play.sh
```

## Mac OS X

- Install [Command Line Tools for XCode](https://developer.apple.com/downloads/) (you need an apple developer account, free)
- Install [homebrew](https://brew.sh)

  ```sh
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  ```

- Install Lua

  ```sh
  brew install lua
  ```

- Install SDL2
  
  ```sh
  brew install sdl2 sdl2_image
  ```

- Install cmocka

  ```sh
  brew install cmocka
  ```

- Install meson

  ```sh
  brew install meson
  ```

```sh
./build.sh
./play.sh
```

### Creating the Mac Bundle

[**edit:** _This is broken. Makefile has not been updated for this to still work, and the makefile is in the process of being deprecated. Furthermore, macOS Big Sur does not support 32bit. At all._ ]

I don't actually understand how Mac builds work currently.  I am piggiebacking
off TyrQuake's Makefile "bundles" task with some workarounds to compensate for
two things:

- The makefile expects 32bit and 64bit Lua installations to be available (I only know how to install one at a time)
- The Launcher isn't trying to use its bundled SDL2.Framework folder

So these are the wonky, unautomated steps to build the mac bundle:

1. go to lua 5.2 folder (from previous section) and
   modify src/Makefile lines 22-23 to force a 32bit build.

    ```
    MYCFLAGS= -arch i386
    MYLDFLAGS= -arch i386
    ```

1. build and install lua (from previous section)
1. go back to this project's engine/ directory, and run `make bundles`
1. it should succeed with the 32bit build, but fail when linking the 64bit build.
1. Rebuild/install lua with 64bit, with the following edits:

    ```
    MYCFLAGS= -arch x86_64
    MYLDFLAGS= -arch x86_64
    ```

1. run `make bundles` again.
1. It should succeed with the 64bit, and fail when reaching the opengl build (that's okay)
1. The Bundle will be in dist/osx/Tyr-Quake.app
1. Run the following to allow Launcher to find its bundled SDL2 framework

    ```
    install_name_tool -add_rpath @executable_path/../Frameworks dist/osx/Tyr-Quake.app/Contents/MacOS/Launcher
    ```

1. Rename Tyr-Quake.app to Blinky.app

## Linux

- Install Lua 5.2

  - On Debian or Ubuntu:

    ```sh
    sudo apt-get install liblua5.2-dev
    ```

  - Otherwise install from source:

    ```sh
    curl -R -O http://www.lua.org/ftp/lua-5.2.0.tar.gz
    tar zxf lua-5.2.0.tar.gz
    cd lua-5.2.0
    make linux test
    sudo make install
    ```

- Install SDL 2

  ```sh
  sudo apt-get install libsdl2-dev libsdl2-image-dev
  ```

- Install Xxf86dga

  ```sh
  sudo apt-get install libxxf86dga-dev
  ```

- Install python and pip3 (for meson) and cmocka

  ```
  sudo apt-get python3 pip3 cmocka
  ```

```sh
./build.sh
./play.sh
```
