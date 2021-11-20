libhs {#mainpage}
=====

Overview {#overview}
========

libhs is a C library to enumerate HID and serial devices and interact with them. It is:

- **single-file**: one header is all you need to make it work.
- **public domain**: use it, hack it, do whatever you want.
- **multiple platforms**: Windows (≥ Vista), Mac OS X (≥ 10.9) and Linux.
- **multiple compilers**: MSVC (≥ 2015), GCC and Clang.
- **driverless**: uses native OS-provided interfaces and does not require custom drivers.

Build {#build}
========

Just [download libhs.h from the GitHub repository](https://github.com/Koromix/libraries). This file provides both the interface and the implementation. To instantiate the implementation, `#define HS_IMPLEMENTATION` in ONE source file, before including libhs.h.

libhs depends on a few OS-provided libraries that you need to link:

| OS                      | Dependencies
| ----------------------- | ------------------------------------------------------------------------------------
| __Windows (MSVC)__      | Nothing to do, libhs uses `#pragma comment(lib)`
| __Windows (MinGW-w64)__ | Link _user32_, _advapi32_, _setupapi_ and hid `-luser32 -ladvapi32 -lsetupapi -lhid`
| __OSX (Clang)__         | Link _CoreFoundation_ and _IOKit_
| __Linux (GCC)__         | Link _libudev_ `-ludev`

This library is developed as [part of the TyTools project](https://github.com/Koromix/tytools) where you can find the original non-amalgamated libhs source code. The amalgamated header file is automatically produced by CMake scripts.

Look at [Sean Barrett's excellent stb libraries](https://github.com/nothings/stb) for the reasoning behind this mode of distribution.

In order to use the library you can:

- [look at the examples](https://github.com/Koromix/tytools/tree/master/src/libhs/examples)
- [consult the reference documentation](modules.html)
