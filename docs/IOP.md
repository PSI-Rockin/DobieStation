TODO:
* List of IOP modules and their purposes
* Memory map
* Details on the SIF

# IOP documentation

The Input-Output Processor (IOP) is a MIPS R3000 CPU; the same one found in the original PlayStation. It handles access to SPU2, the controller, the CD/DVD drive, the memory card, and other peripherals. The IOP also provides backwards compatibility for most PSX games.

When the console powers on, the IOP loads modules from the BIOS. These modules contain functionality related to, for example, file input/output and controller management. Most (?) commercial games upload .IRX patches in order to modify or extend the capabilities of certain modules.

The EE and IOP communicate through the Sub-system Interface (SIF).

## Memory and I/O map (incomplete)

|Address|Description|
|-------|-----------|
|0x00000000-0x00200000|2 MB RAM|
|0x1F801070|I_STAT?|
|0x1F801074|I_MASK?|
|0x1FA00000|POST3|
|0x1FC00000-0x20000000|BIOS (same as EE)|
|0xFFFE0130|Cache control?|
