# DobieStation
A young PS2 emulator with plans for an optimized Android port, as well as a fast, accurate, and easy-to-use PC port. Basic homebrew is capable of running, but nothing else yet. Not intended for general use.

IRC: Join #dobiestation on irc.badnik.net for discussing development.

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
DobieStation requires a copy of the PS2 BIOS, which must be dumped from your PS2, and can only be run from the command line. Loading files from the GUI will be supported in the near future.

DobieStation takes two arguments from the command line: the name of the BIOS file, and the name of an ELF file. Additionally, the "-skip" flag will tell DobieStation to stop booting the BIOS and execute the ELF.

### PS2 Homebrew
Want to test DobieStation? Check out this repository: https://github.com/PSI-Rockin/ps2demos

Compatibility not guaranteed, but some demos do work.

## Contributing
First, review the [Contribution Guide](../master/CONTRIBUTING.md). Once you have done so, take a look at the [Task List](../master/TASKS.md) and pick something that interests you.
