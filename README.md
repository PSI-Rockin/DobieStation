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

## Using the Emulator
DobieStation requires a copy of the PS2 BIOS, which must be dumped from your PS2.

The various command line options are as follows:
```
-b [BIOS file] - Takes the form of /path/to/bios.bin. Required for booting DobieStation.
-f [file] (optional) - The ELF or ISO that DobieStation loads. Takes the form of /path/to/game.iso or /path/to/homebrew.elf.
-s (optional) - Skip the BIOS boot animation when starting DobieStation with an ISO/ELF loaded.
```

For now, the `-s` flag is needed for a successful boot to occur, so use it when testing.

The key bindings are as follows:

| Keyboard         | DualShock 2 |
| ---------------- | ----------- |
| <kbd>A</kbd>     | Triangle    |
| <kbd>S</kbd>     | Square      |
| <kbd>Z</kbd>     | Circle      |
| <kbd>X</kbd>     | Cross       |
| <kbd>Enter</kbd> | Start       |
| <kbd>Shift</kbd> | Select      |
| <kbd>Q</kbd>     | L1          |
| N/A              | L2          |
| N/A              | L3          |
| <kbd>W</kbd>     | R1          |
| N/A              | R2          |
| N/A              | R3          |
| <kbd>↑</kbd>     | D-pad up    |
| <kbd>↓</kbd>     | D-pad down  |
| <kbd>←</kbd>     | D-pad left  |
| <kbd>→</kbd>     | D-pad right |

| Keyboard      | DobieStation               |
| ------------- | -------------------------- |
| <kbd>F1</kbd> | Dump current frame from GS |
| <kbd>.</kbd>  | Advance a single frame     |

### PS2 Homebrew
Want to test DobieStation? Check out this repository: https://github.com/PSI-Rockin/ps2demos

Compatibility not guaranteed, but some demos do work.

## Contributing
First, review the [Contribution Guide](../master/CONTRIBUTING.md). Once you have done so, take a look at the issue tracker to look for tasks you may be interested in.

## Legal
DobieStation uses `arg.h` by [Christoph "20h" Lohmann](http://www.r-36.net).
