# Emotion Engine documentation

The Emotion Engine (EE) is a custom MIPS III CPU with some MIPS IV instructions. On real PS2s, the EE contains other components such as the DMAC, the Vector Units, the GIF, etc. However, this document will refer to the MIPS core and its coprocessors.

## Memory map
|Address|Name|
|-------|----|
|0x00000000-0x02000000| 32 MB RDRAM (possibly mirrored to 0x10000000?)|
|0x10000000-0x10002000| Timer registers|
|0x10002000-0x10003000| IPU registers|
|0x10003000-0x10003800| GIF registers|
|0x10003800-0x10003C00| VIF0 registers|
|0x10003C00-0x10004000| VIF1 registers|
|0x10004000-0x10008000| FIFO registers|
|0x10008000-0x1000F000| DMAC registers|
|0x11000000-0x11001000| VU0 micro mem|
|0x11004000-0x11005000| VU0 mem|
|0x11008000-0x1100C000| VU1 micro mem|
|0x1100C000-0x11010000| VU1 mem|
|0x12000000-0x13000000| GS privileged registers|
|0x1FC00000-0x20000000| BIOS|

## EE Registers
The EE has 32 128-bit general purpose registers (GPRs), two 128-bit LO and HI registers for storing the results of multiplication and division routines, and a 32-bit program counter.

The standard MIPS instructions only use the lower 64 bits of the registers. However, Multimedia Instructions (MMI) can make use of the full 128-bits.

MIPS assembly has certain conventions for the GPRs. While the GPRs may be used for any purpose (except for $0, which is not writable), compilers will only use them for specific purposes. This is to ensure that MIPS modules written by different programmers are able to interact with each other.

The following is a list of registers and their conventions:

```
Register  Name    Description
$0        zero    Hardwired to zero; any attempt to modify is ignored
$1        at      Assembler temporary; used for pseudo-instructions
$2-3      v0-v1   Return values from subroutines
$4-7      a0-a3   Arguments passed to subroutines; not preserved
$8-15     t0-t7   Temporary data; not preserved
$16-23    s0-s7   Static data; preserved
$24-25    t8-t9   More temporary data; not preserved
$26-27    k0-k1   Reserved for kernel
$28       gp      Global pointer
$29       sp      Stack pointer
$30       fp      Frame pointer
$31       ra      Return address from subroutine

; Special-purpose registers
-         pc      Program counter
-         lo/hi   Stores the low 32-bits and high 32-bits of MULT/DIV instructions, respectively
```

## Known differences between the MIPS standard and the EE

* MULT/MULTU as well as the related MMI instructions MULT1/MULTU1 specify an additional destination register. This allows the LO part of the result to be stored directly without having to use an MFLO instruction.

```
; If t1 = 5 and t2 = 10...
multu t0, t1, t2
; Both t0 and LO will contain 50.
```
