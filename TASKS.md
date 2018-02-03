# Tasks

This is a non-exhaustive list of features/improvements that potential contributors can work on.

Please review the [Contribution Guide](../master/CONTRIBUTING.md) first. After that, contact https://github.com/PSI-Rockin over GitHub, and let me know what you're interested in doing. This is so that people don't independently work on the same tasks. More tasks will be added as the need arises.

The general difficulty of each task is represented in brackets. "Difficulty" is very much subjective; each task takes into consideration knowledge needed and the time it would take to be implemented. If you're interested in a task but need assistance or clarification, please let me (PSI-Rockin) know.

## Core
* [Medium] Rewrite the disassembly infrastructure.
  * The current disassembly code, located throughout the opcode definitions in the interpreter, is messy and controlled by an ugly compile-time macro (```NDISASSEMBLE``` - core/emotioninterpreter.hpp). The rewrite will allow for runtime-controlled disassembly and more accurate output.
  * To complete this task, port the disassembly for all current EE opcodes into core/emotiondisasm.cpp, then remove all the disassembly printf statements and ```NDISASSEMBLE``` junk from the interpreter files.
  * Two opcodes have already been implemented in emotiondisasm.cpp to provide an example.
* [Medium/Hard] Implement functionality for lines and triangles in the software renderer.
  * Primitives are the name for geometric shapes rendered by the Graphics Synthesizer (GS). The GS supports points, lines, line strips, triangles, triangle strips, triangle fans, and sprites.
  * Points and sprites (2D rectangles) are currently supported. Only stubs exist for lines and triangles.
  * Locate "render_line" and "render_triangle" in core/gs.cpp and use Bresenham's line algorithm to complete this task.

## Qt
* [Easy] Add a framelimiter that prevents emulation from going beyond 60 FPS, and add an FPS counter that updates every second in the title bar.
* [Easy] Support loading ELF files from the menubar. This is simple enough on its own, but it requires that emulation doesn't start until after a file is loaded. Also allow starting DobieStation from the command line with only the BIOS name argument.
* [Medium/Hard] Create a debugger window for the EE. The window should have, at minimum, a disassembler, an EE register viewer, a call stack, and breakpoints.
  * For obvious reasons, this can only be done after the disassembler rewrite.
* [Medium/Hard] Split emulation onto a separate thread, keeping the GUI on the main thread. Use QThread for this.

## Misc
* [Easy] Support other build systems, such as cmake and meson.
