# emunes
A *noexcept-quality*\* NES emulator written in C++17.

Passes almost all Blargg's tests: vbl/nmi & instruction timing; dummy reads; sprite behaviour.

Runs a large amount of NTSC-compatible games with mappers: 0, 1, 2, 3, 7.

## List of passed tests

- [x] branch\_timing\_tests
- [x] cpu\_dummy\_reads
- [ ] cpu\_interrupts\_v2
    - [x] cli\_latency
    - [x] nmi\_and\_brk
    - [x] nmi\_and\_irq
    - [ ] irq\_and\_dma
    - [ ] branch\_delays\_irq
- [x] \(official only) cpu\_timing\_test6
- [x] instr\_misc
- [x] \(offical only) instr\_test\_v5
- [x] \(offical only) instr\_timing
- [x] oam\_read
- [x] ppu\_open\_bus
- [x] ppu\_sprite\_hit
- [x] ppu\_sprite\_overflow
- [x] ppu\_vbl\_nmi

## Building

**Enter the following commands to build under Linux**
```
apt install libsdl2-dev g++
cd project_root_directory
mkdir bin
make
```

Type 'emunes filepath' to load a ROM.

\*The source code contains the **noexcept** keyword everywhere.

## Screenshots
![alt tag](/res/images/1.png)
![alt tag](/res/images/2.png)
![alt tag](/res/images/3.png)

