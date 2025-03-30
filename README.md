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
- Z80 Assembly syntax highlighting
- Integrates [SJASMPlus](https://github.com/z00m128/sjasmplus)

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
