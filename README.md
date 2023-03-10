# m8080

`m8080` is a cycle-accurate Intel 8080 emulator library. It successfully passes every test ROM developed for the original Intel 8080 I could find... and it can run [Space Invaders](examples/invaders.c)!

The library is written as a [single header file](https://github.com/nothings/stb#how-do-i-use-these-libraries) in the style of the stb libraries.

#### Overview

The emulator is represented by the structure `m8080`. It doesn't contain memory, instead it has a generic `void* userdata`. The user has to implement the functions `m8080_rb` (read byte) and `m8080_wb` (write byte) so the emulator knows how to access memory.

The function `m8080_step` takes the current state as input, emulates one instruction, updates the state and returns the number of cycles it would have taken on an actual Intel 8080.

The function is basically a big `switch` statement. Simple instructions are inlined, for example, `mov a, b` is just `c->a = c->b`. More complicated instructions are implemented in auxiliary functions such as `m8080_add`, `m8080_sub`, `m8080_call`, etc. The user doesn't need to worry about these functions.

See the provided [examples](examples) for more.
