# DobieStation
A young PS2 emulator with plans for an optimized Android port, as well as a fast, accurate, and easy-to-use PC port.

A large portion of the PS2's library can boot or get to menus. Some titles can even go in-game, including high-profile ones such as Final Fantasy X and Shadow of the Colossus. Not intended for general use.

Discord: https://discord.gg/zbEXKfN

## Compiling
DobieStation uses Qt 5 and supports qmake and CMake.

### Building with qmake
```
cd DobieStation/DobieStation
qmake DobieStation.pro
make
```

### Building with CMake
```
mkdir build
cd build
cmake ..
make
```

### Note for Ubuntu Xenial
While 16.04 is still officially supported they don't provide a recent enough version of Qt.
As such if you wish to build Dobie on Xenial you will need to update Qt through a PPA
```
add-apt-repository ppa:beineri/opt-qt-5.11.1-xenial
apt-get update
apt-get install qt511-meta-minimal qt511multimedia libglu1-mesa-dev
source /opt/qt511/bin/qt511-env.sh
```
Alternatively, you may upgrade to the most recent Ubuntu LTS, 18.04.

### Building with Visual Studio
Dobiestation is known to compile on 2015 and 2017.

Before opening Visual Studio, you must set the `QTDIR` environment variable to your 64 bit QT install.
For example on Visual Studio 2017: `C:\path\to\qt\5.12.1\msvc2017_64`

Once the variable is set open `DobieStation\DobieStation.sln` in Visual Studio.

## Using the Emulator
DobieStation requires a copy of the PS2 BIOS, which must be dumped from your PS2.

The various command line options are as follows:
```
-b [BIOS file] - Takes the form of /path/to/bios.bin. Required for booting DobieStation.
-f [file] (optional) - The ELF or ISO that DobieStation loads. Takes the form of /path/to/game.iso or /path/to/homebrew.elf.
-s (optional) - Skip the BIOS boot animation when starting DobieStation with an ISO/ELF loaded.
```

The key bindings are as follows:

| Keyboard         | DualShock 2       |
| ---------------- | ----------------- |
| <kbd>A</kbd>     | Triangle          |
| <kbd>S</kbd>     | Square            |
| <kbd>Z</kbd>     | Circle            |
| <kbd>X</kbd>     | Cross             |
| <kbd>Enter</kbd> | Start             |
| <kbd>Shift</kbd> | Select            |
| <kbd>Q</kbd>     | L1                |
| N/A              | L2                |
| N/A              | L3                |
| <kbd>W</kbd>     | R1                |
| N/A              | R2                |
| N/A              | R3                |
| <kbd>↑</kbd>     | D-pad up          |
| <kbd>↓</kbd>     | D-pad down        |
| <kbd>←</kbd>     | D-pad left        |
| <kbd>→</kbd>     | D-pad right       |
| <kbd>I</kbd>     | Left Analog up    |
| <kbd>K</kbd>     | Left Analog down  |
| <kbd>J</kbd>     | Left Analog left  |
| <kbd>L</kbd>     | Left Analog right |

| Keyboard      | DobieStation               |
| ------------- | -------------------------- |
| <kbd>F1</kbd> | Dump current frame from GS |
| <kbd>F8</kbd> | Take a screenshot          |
| <kbd>.</kbd>  | Advance a single frame     |

### PS2 Homebrew
Want to test DobieStation? Check out this repository: https://github.com/PSI-Rockin/ps2demos

Compatibility not guaranteed, but some demos do work.

## Contributing
First, review the [Contribution Guide](../master/CONTRIBUTING.md). Once you have done so, take a look at the issue tracker to look for tasks you may be interested in.

## Legal
DobieStation uses `arg.h` by [Christoph "20h" Lohmann](http://www.r-36.net).
