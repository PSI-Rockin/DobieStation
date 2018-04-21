# DobieStation
A young PS2 emulator with plans for an optimized Android port, as well as a fast, accurate, and easy-to-use PC port. Some homebrew can run, and few commercial games can boot too, but nothing is playable yet. Not intended for general use.

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
DobieStation requires a copy of the PS2 BIOS, which must be dumped from your PS2. DobieStation takes the name of a BIOS file as an argument from the command. Optionally, you may also enter the name of an ELF/ISO file. The "-skip" flag will tell DobieStation to stop booting the BIOS and execute the ELF/ISO.

If you only enter the BIOS at the command line, ELFs and ISOs can be loaded from the menubar.

### PS2 Homebrew
Want to test DobieStation? Check out this repository: https://github.com/PSI-Rockin/ps2demos

Compatibility not guaranteed, but some demos do work.

## Contributing
First, review the [Contribution Guide](../master/CONTRIBUTING.md). Once you have done so, take a look at the issue tracker to look for tasks you may be interested in.
