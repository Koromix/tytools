# Introduction

ty is a collection of tools to manage Teensy devices (or teensies). It provides:
- libty, a C library
- and tyc, a command-line tool

It currently runs on Linux, Windows and Mac OS X.

- [Build instructions](#build)
  - [Linux](#build_linux)
  - [Windows](#build_windows)
  - [Mac OS X](#build_darwin)
- [Usage](#usage)
  - [List devices](#usage_list)
  - [Upload firmware](#usage_upload)
  - [Serial monitor](#usage_monitor)
  - [Other commands](#usage_misc)

<a name="build"/>
# Build instructions

You can download a source release from the release page on GitHub or clone the repository.
Pre-built binaries are available for Windows and Mac OS X.

ty can be built using GCC or Clang.

Experimental features are disabled by default, enable them by turning on the EXPERIMENTAL option
in cmake with `-DEXPERIMENTAL=1`.

<a name="build_linux"/>
## Linux

<a name="build_linux_dependencies"/>
### Dependencies

To install the dependencies on Debian or Ubuntu execute:
```bash
sudo apt-get install build-essential cmake libudev-dev
```

On Arch Linux you can do so (as root):
```bash
pacman -S --needed base-devel cmake udev
```

<a name="build_linux_compile"/>
### Compilation

Open the project directory in a terminal and execute:
```bash
mkdir -p build/linux && cd build/linux
cmake ../..
make
```

If you want to build a debug binary, you have to specify the build type:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ../..
```

<a name="build_windows"/>
## Windows

Pre-built binaries are provided in the releases section.

### Native compilation

You need to install [CMake](http://www.cmake.org/) and MinGW-w64 to build ty under Windows.
Visual Studio is not supported at the moment, nor is the historical MinGW toolchain.

To install MinGW-w64, the [TDM-GCC](http://tdm-gcc.tdragon.net/) package is probably the easiest
option. Make sure to use the TDM64 MinGW-w64 edition.

To compile ty, open a console, go to the project directory and execute:
```bash
mkdir build\win32 && cd build\win32
cmake -G "MinGW Makefiles" ..\..
mingw32-make
```

### Cross-compilation

An easier option is to cross-compile the windows binary from Linux. You need to install MinGW-w64
first.

On Debian and Ubuntu, install cmake and mingw-w64:
```bash
sudo apt-get install cmake mingw-w64
```

If you use Arch Linux, execute (as root):
```bash
pacman -S --needed cmake mingw-w64-toolchain
```

You can then use the appropriate toolchain file provided in the contrib/cmake directory. Open the
project directory in a console and execute:
```bash
mkdir -p build/win32 && cd build/win32
cmake -DCMAKE_TOOLCHAIN_FILE=../../contrib/cmake/i686-w64-mingw32.cmake ../..
make
```

<a name="build_darwin"/>
## Mac OS X

Pre-built binaries are provided in the releases section.

### Native compilation

Install Xcode, the developer command-line tools and CMake.

Open a console in the project directory and execute:
```bash
mkdir -p build/darwin && cd build/darwin
cmake ../..
make
```

CMake supports universal binaries but does not build them by default. To generate universal
binaries for i386 and x86_64, use:
```bash
cmake -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" ../..
```

### Cross-compilation

You need to install [osxcross](https://github.com/tpoechtrager/osxcross).

Open a terminal and load the OSX environment (using osxcross-env). Go to the project directory
and execute:
```bash
mkdir -p build/darwin && cd build/darwin
cmake -DCMAKE_TOOLCHAIN_FILE=../../contrib/cmake/x86_64-darwin-clang.cmake ../..
make
```

You can also make universal binaries under Linux. To build one with i386 and x86_64 binaries, use:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../../contrib/cmake/x86_64-darwin-clang.cmake -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" ../..
```

<a name="usage"/>
# Usage

You can manage multiple devices connected simultaneously, ty uniquely identifies each device by its
position in the host USB topology. Meaning if it stays on the same USB port, it is the same device
for ty. That's necessary because across reboots and resets, Teensies look completely different to
the host.

When you want to target a specific device, use `tyc --device <path>#<serial> <command>`. The format
of path is specific to ty and can be found using `tyc list`, serial is the USB serial number. Either
can be omitted.

<a name="usage_list"/>
## List devices

`tyc list` lists plugged Teensy devices. Here is how it looks:
```
+ usb-1-2#34130 teensy31
+ usb-4-2#29460 teensy
+ usb-4-3#32250 teensy30
```

If you want detailed information about plugged devices, use `--verbose`:
```
+ usb-4-3#32250 teensy30
  - model: Teensy 3.0
  - capabilities: upload, reset
  - interfaces:
      * HalfKay Bootloader: /dev/hidraw2
```

You can also watch device changes with `--watch`.

<a name="usage_upload"/>
## Upload firmware

Use `tyc upload <filename.hex>` to upload a specific firmware to your device. It is checked for
compatibility with your model before being uploaded.

By default, a reboot is triggered but you can use `--wait` to wait for the bootloader to show up,
meaning ty will wait for you to press the button on your board.

<a name="usage_monitor"/>
## Serial monitor

`tyc monitor` opens a text connection with your Teensy. It is either done through the serial device
(/dev/ttyACM*) or through the HID serial emulation (SEREMU) in other USB modes. ty uses the correct
mode automatically.

You can use the `--reconnect` option to detect I/O errors (such as a reset, or after a brief
unplugging) and reconnect immediately. Other errors will exit the program.

The `--raw` option will disable line-buffering/editing and immediately send everything you type in
the terminal.

See `tyc help monitor` for other options. Note that Teensy being a USB device, serial settings are
ignored. They are provided in case your application uses them for specific purposes.

<a name="usage_misc"/>
## Other commands

You can learn about other commands using `tyc help`. Get specific help for them using
`tyc help <command>`. The main ones are:
* reset: reset the Teensy device (currently by issuing a reboot followed by a reset)
