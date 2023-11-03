Many, many, years ago, I saw [Bisqwit's video](https://www.youtube.com/watch?v=1e0OCwlaEZM) where he writes an emulator for MicroBlaze, a "soft core" architecture from Xilinx, and boots Linux on it. Ever since then, I've wanted to code my own emulator capable of running Linux! Of course, back when I saw the video, I was a hapless middle schooler who couldn't even manage to write a [CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) interpreter despite its meager 30 opcodes... but I've gotten a little better at programming since then, so I think we'll have some more success this time around.

Now, for the crucial question: which architecture to emulate? I decided to go with RISC-V, for a few reasons:

- It's simple and therefore easy to emulate
- It's fully supported by mainline Linux
- The specifications are widely available
- I want to learn about RISC-V because...
- It's totally hip and cool&trade;

Specifically, I'll be building an emulator for RV64G, which just means the 64-bit ISA plus some basic extensions designed for "general-purpose" code.

[Buildroot](https://buildroot.org/) makes it exceptionally easy to build a Linux image tailored towards our needs. I went with the following opt