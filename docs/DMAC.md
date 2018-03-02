TODO:
* Channel and control registers
* DMA transfer modes (normal, source chain, etc)

# DMAC documentation

The DMA Controller (DMAC) provides the EE with different communication options for the rest of the hardware. Ten different DMAC channels are available:

| ID | Name | Direction |
| -- | ---- | --------- |
| 0  | VIF0 | to        |
| 1  | VIF1 | both      |
| 2  | GIF  | both      |
| 3  | IPU  | from      |
| 4  | IPU  | to        |
| 5  | SIF0 | from      |
| 6  | SIF1 | to        |
| 7  | SIF2 | both      |
| 8  | SPR  | from      |
| 9  | SPR  | to        |
