TODO:
* More syscalls and their descriptions

# BIOS documentation

The PS2 BIOS resides in a 4 MB ROM. Its responsibilities include the following:

* Booting up and initializing hardware components
* Loading modules into IOP memory
* Intercepting exceptions and calling the appropriate handlers depending on the type of exception
* Providing a large variety of useful functions for games through the use of the SYSCALL instruction

## Bootup
Both the EE and IOP start executing at 0xBFC00000. The below pseudocode shows how the BIOS determines which initialization procedure to call:

```
Get processor revision id (PRID) from COP0 register 15
If PRID equals 0x2E20
  Begin EE initialization
Else
  Begin IOP initialization
```

*TODO: Details on both procedures*

## Syscalls

When the SYSCALL instruction is executed, program execution jumps to a handler at 0x80000180. The BIOS then loads the byte before the syscall and calls the function associated with the byte number. This means that syscalls require two instructions. E.g.:

```
addiu zero, zero, 0x64
syscall
```

This will call function number 0x64 (which is FlushCache).

Registers a0-t1 ($4-$9) are used as arguments. v0 is used as a return value.

### _EnableIntc - 0x14

Parameters:
* a0 - Bit number of INTC_MASK to modify

Return value:
1 if the function changed the value of INTC_MASK, 0 otherwise.

Description:
Writes ```1 << a0``` to INTC_MASK.

### RFU060 (InitMainThread) - 0x3C

Parameters:
* a1 - Stack base
* a2 - Stack size

Return value:
The thread's stack pointer.

Description:
Creates the main thread with priority 0 and initializes its stack. If a1 equals -1, then the stack pointer equals the top address of RDRAM (0x02000000). Otherwise, the stack pointer equals a1 + a2 minus the memory needed to store the thread's context.

### RFU061 (InitHeap) - 0x3D

Parameters:
* a0 - Heap base
* a1 - Heap size

Return value:
The current thread's heap address.

Description:
Initializes the heap of the current thread. If a1 equals -1, then the heap address equals the top address of RDRAM (0x02000000). Otherwise, the heap address equals a1 + a2.

### EndOfHeap - 0x3E

Parameters:
None.

Return value:
The current thread's heap address.

Description:
Returns the current thread's heap address.

### FlushCache - 0x64

*TODO*
