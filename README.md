# Introduction

ty is a collection of tools to manage Teensy devices (or teensies). It provides:
- libty, a C library
- and tyc, a command-line tool

It currently runs on Linux and Windows only.

- [Build instructions](#build)
  - [Linux](#build_linux)
  - [Windows](#build_windows)
- [Usage](#usage)
  - [Upload firmware](#usage_upload)
  - [Serial monitor](#usage_monitor)
  - [Other commands](#usage_misc)

<a name="build"/>
# Build instructions

ty can be built using GCC or Clang.

Experimental features are disabled by default, enable them by turning ON the EXPERIMENTAL option in cmake with `-DEXPERIMENTAL=1`.

<a name="build_linux"/>
## Linux

<a name="build_linux_dependencies"/>
### Dependencies

To install the dependencies on Debian or Ubuntu execute:
```bash
sudo apt-get install git build-essential cmake libudev-dev
```

On Arch Linux you can do so (as root):
```bash
pacman -S --needed git base-devel cmake udev
```

<a name="build_linux_compile"/>
### Compile

To get the latest development sources you need to use git:
```bash
git clone https://github.com/Koromix/ty.git
```

You can then build ty:
```bash
cd ty
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

You will need to install CMake and MinGW (with make) to build ty under Windows. Visual Studio is not supported at the moment.

### Cross-compilation

An easier option is to cross-compile the windows binary from Linux. You need to install MinGW-w64 first.

On Debian and Ubuntu, install cmake and mingw-w64:
```bash
sudo apt-get install cmake mingw-w64
```

If you use Arch Linux, execute as root:
```bash
pacman -S --needed cmake mingw-w64-toolchain
```

You can then use the appropriate toolchain file provided in the contrib/cmake directory:
```bash
cd ty
mkdir -p build/win32 && cd build/win32
cmake -DCMAKE_TOOLCHAIN_FILE=../../contrib/cmake/i686-w64-mingw32.cmake ../..
make
```

<a name="usage"/>
# Usage

You can manage multiple devices connected simultaneously, ty uniquely identifies each device by its position in the host USB topology. Meaning if it stays on the same USB port, it is the same device for ty. That's necessary because across reboots and resets, Teensies look completely different to the host. Use `tyc list` to discover teensies (`--verbose` to get details).

When you want to target a specific device, use `tyc --device <path>#<serial> <command>`. The format of path is specific to ty and can be found using `tyc list`, serial is the USB serial number. Either can be omitted.

<a name="usage_upload"/>
## Upload firmware

Use `tyc upload <filename.hex>` to upload a specific firmware to your device. It is checked for compatibility with your model before being uploaded. By default, a reboot is triggered but you can use `--wait` to wait for the bootloader to show up, meaning ty will wait for you to press the button on your board.

<a name="usage_monitor"/>
## Serial monitor

`tyc monitor` opens a text connection with your Teensy. It is either done through the serial device (/dev/ttyACM*) or through the HID serial emulation (SEREMU) in other USB modes. ty uses the correct mode automatically.

You can use the `--reconnect` option to detect I/O errors (such as a reset, or after a brief unplugging) and reconnect immediately. Other errors will exit the program.

The `--raw` option will disable line-buffering/editing and immediately send everything you type in the terminal.

See `tyc help monitor` for other options. Note that Teensy being a USB device, serial settings are ignored. They are provided in case your application uses them for specific purposes.

<a name="usage_misc"/>
## Other commands

You can learn about other commands using `tyc help`. Get specific help for them using `tyc help <command>`. The main ones are:
* list: list teensies, and show details if used with `--verbose`
* reset: reset the Teensy device (currently by issuing a reboot followed by a reset)
