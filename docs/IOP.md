TODO:
* List of IOP modules and their purposes
* Memory map
* Details on the SIF

# IOP documentation

The Input-Output Processor (IOP) is a MIPS R3000 CPU; the same one found in the original PlayStation. It handles access to SPU2, the controller, the CD/DVD drive, the memory card, and other peripherals. The IOP also provides backwards compatibility for most PSX games.

When the console powers on, the IOP loads modules from the BIOS. These modules contain functionality related to, for example, file input/output and controller management. Most (?) commercial games upload .IRX patches in order to modify or extend the capabilities of certain modules.

The EE and IOP communicate through the Sub-system Interface (SIF).
