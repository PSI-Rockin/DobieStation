Lots of TODO: Discuss packets as well as how to utilize the different paths

# GIF documentation

The Graphics Interface (GIF) is the main method of communication between the EE and the GS. The GIF can take data from three different PATHs:

* PATH1 - From VU1
* PATH2 - From the VIF
* PATH3 - From DMAC channel 2

All three PATHs are capable of operating in parallel, which allows the massive bandwidth of the GS to be fully utilized. However, lower-number paths have higher priority. The GIF will first look in PATH1, then in PATH2, and finally in PATH3.

