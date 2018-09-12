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

### PS2 Homebrew
Want to test DobieStation? Check out this repository: https://github.com/PSI-Rockin/ps2demos

Compatibility not guaranteed, but some demos do work.

## Contributing
First, review the [Contribution Guide](../master/CONTRIBUTING.md). Once you have done so, take a look at the issue tracker to look for tasks you may be interested in.

## Legal
DobieStation uses `arg.h` by [Christoph "20h" Lohmann](http://www.r-36.net).
