# nyukomatic
[![Version Badge](https://img.shields.io/github/v/release/alexanderk23/nyukomatic)](https://github.com/alexanderk23/nyukomatic/releases/latest)
[![Build](https://github.com/alexanderk23/nyukomatic/actions/workflows/cmake.yml/badge.svg?event=push)](https://github.com/alexanderk23/nyukomatic/actions/workflows/cmake.yml)

A ZX Spectrum live-coding tool inspired by [Bazematic](https://github.com/gasman/bazematic)
and [Bonzomatic](https://github.com/Gargaj/Bonzomatic).

![scrshot](https://github.com/user-attachments/assets/5acc8853-b786-46a9-94a8-9188b601e03e)

[**Download latest release**](https://github.com/alexanderk23/nyukomatic/releases/latest) •
[Report bug](https://github.com/alexanderk23/nyukomatic/issues/new?labels=bug&template=bug_report.md) •
[Request a new feature](https://github.com/alexanderk23/nyukomatic/issues/new?labels=enhancement&template=feature_request.md)

## About the project
Like other [live-coding](https://livecode.demozoo.org) tools,
**nyukomatic** combines a code editor and a Z80 assembler, allowing you to
see the results of code changes in real time. Unlike [Bazematic](https://github.com/gasman/bazematic),
which runs in a browser and uses [RASM](https://github.com/EdouardBERGE/rasm) as a compiler, it is a
stand-alone native executable (Win/Linux/macOS) and uses [SJASMPlus](https://github.com/z00m128/sjasmplus)
which is more familiar to many Z80 coders in terms of syntax.

Continuing the now established tradition of naming live-coding tools, **nyukomatic** is named after
[Nyuk](https://github.com/akanyuk), the organizer of [Multimatograf](https://multimatograf.ru/)
demoparty who held the first ZX Spectrum live-coding [event](https://livecode.demozoo.org/serie/Multimatograf.html#mc)
in Russia.

## Key Features
- Native cross-platform app (with Windows, Linux and macOS builds available)
- Fast, cycle-accurate Pentagon 128 emulation powered by [z80 library](https://github.com/kosarev/z80)
- Sender/grabber network modes with relaying via [BonzomaticServer](https://github.com/alkama/BonzomaticServer)
- Can receive network broadcast data available from Z80 code by reading ports
  (requires [patched Bonzomatic server](https://github.com/alexanderk23/BonzomaticServer))
- Integrates [SJASMPlus](https://github.com/z00m128/sjasmplus): code is compiled and executed in real time as you type
- Z80 Assembly syntax highlighting

## Building nyukomatic on Linux (Debian)

```bash 
sudo apt install -y libgl1-mesa-dev libfontconfig-dev libxmu-dev libxi-dev libgl-dev libxrandr-dev libxinerama-dev xorg-dev libglu1-mesa-dev libssl-dev
git clone https://github.com/alexanderk23/nyukomatic
cd nyukomatic
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## Usage
**nyukomatic** can operate in either **sender** or **grabber** mode.

- In **sender mode**, nyukomatic transmits the user's actions (code changes, cursor movements, etc.)
to the Bonzomatic server, which then relays them to all grabbers that have the same **room** and
**nickname** specified in their server URL.

- In **grabber mode**, input is disabled, and **nyukomatic** displays the actions of a sender with
a matching **room** and **nickname**.

By default, the program launches in **sender mode without a server connection**.
You can experiment with the code, but it won’t be transmitted anywhere.

To connect, open the settings window, specify the Bonzomatic server URL in the format:
```
ws://bonzomatic.example.com/room/nickname/
```
and click the "connect" button.

## Broadcasting port values
**nyukomatic** supports receiving arbitrary port values over the network from external utilities.
These network-transmitted port values can be read in Z80 assembly code using standard instructions.

This enables dynamic code behavior based on external conditions. For example, [sending FFT spectrum
analyzer data](https://github.com/alexanderk23/nm-fft-example) from a microphone input and modifying
effect parameters in real-time based on music (similar to shader showdown competitions).

### Implementation  
1. **External Utility Setup**:  
   - Connect to a Bonzomatic server room using an **empty nickname**.  
   - Send a JSON message containing an array of port-value pairs:  
     - Single port: `[port_number, value]`  
     - Value array: `[base_port_number, [value_1, value_2, ...]]`  

     ```jsonc
     {
         "ports": [
             [ 4783, 42 ], // Single port value
             [ 1019, [1, 2, 3] ] // Array of values (mapped to ports)
         ]
     }
     ```

2. **Z80 Code Reading**:  
   - **Single port**:  

     ```asm
     ld bc, port_number
     in a, (c)     ; Value stored in register A
     ```
   - **Array of ports**:  
     Each array element maps to `base_port_number - (index * 256)`.  
     Use [`inir`](https://www.jnz.dk/z80/inir.html) for efficient bulk reading:  

     ```asm
     ld bc, base_port_number
     ld hl, dest_addr
     inir          ; Reads values sequentially to memory at HL
     ```

### Example  
For this JSON input:  
```jsonc
{
    "ports": [
        [ 4783, 42 ],
        [ 1019, [1, 2, 3] ]
    ]
}
```
**Resulting port values** (`4783` = `0x12AF`, `1019` = `0x03FB`):
- `0x12AF` → `42`  
- `0x03FB` → `1`, `0x02FB` → `2`, `0x01FB` → `3`  

**Assembly demonstration**:  
```asm
; Read single value
ld bc, 0x12AF
in a, (c)       ; A = 42

; Read value array
ld bc, 0x03FB
ld hl, 0x4000
inir            ; Writes 0x01, 0x02, 0x03 to addresses 0x4000-0x4002
```

## Acknowledgements
### Original projects
- [Bazematic](https://github.com/gasman/bazematic): a browser-based ZX Spectrum live-coding environment
- [Bonzomatic](https://github.com/Gargaj/Bonzomatic): a live shader coding tool and Shader Showdown workhorse

### Libraries and other included software
- [Boost Regex](https://github.com/boostorg/regex): provides regular expression support for C++
- [cxxopts](https://github.com/jarro2783/cxxopts): a lightweight C++ option parser library
- [Dear ImGui](https://github.com/ocornut/imgui): a bloat-free graphical user interface library for C++
- [FreeType](https://github.com/freetype/freetype): a freely available software library to render fonts
- [GLEW](https://github.com/nigels-com/glew): a cross-platform open-source C/C++ extension loading library
- [GLFW](https://github.com/glfw/glfw): an Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan application development
- [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit): a syntax highlighting text editor for ImGui
- [IXWebSocket](https://github.com/machinezone/IXWebSocket): a C++ library for WebSocket client and server development
- [json.cpp](https://github.com/jart/json.cpp): a baroque JSON parsing / serialization library for C++
- [OpenSSL](https://github.com/openssl/openssl): an TLS/SSL and crypto library
- [rang](https://github.com/agauniyal/rang): colors for your Terminal
- [SJASMPlus](https://github.com/z00m128/sjasmplus): a command-line cross-compiler of assembly language for Z80 CPU
- [SSE2NEON](https://github.com/DLTcollab/sse2neon): A C/C++ header file that converts Intel SSE intrinsics to Arm/Aarch64 NEON intrinsics
- [xxhash_cpp](https://github.com/RedSpah/xxhash_cpp): a port of the xxHash library to C++17
- [z80](https://github.com/kosarev/z80): a fast and flexible Z80/i8080 emulator
- [zlib](https://github.com/madler/zlib): a massively spiffy yet delicately unobtrusive compression library
