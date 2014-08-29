# Introduction

ty is a command-line tool to manage Teensy devices (or teensies). it currently runs on Linux and Windows only.

- [Build instructions](#build)
  - [Linux](#build_linux)
  - [Windows](#build_windows)
- [Usage](#usage)
  - [Upload firmware](#usage_upload)
  - [Serial monitor](#usage_monitor)
  - [Other commands](#usage_misc)

<a name="build"/>
# Build instructions

ty can be built using GCC or Clang. Building it results in a single command-line binary that can be moved around.

<a name="build_linux"/>
## Linux

<a name="build_linux_dependencies"/>
### Dependencies

cmake and libudev are the only dependencies needed at the moment.

To install these on Ubuntu:
```bash
sudo apt-get install cmake libudev-dev
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
mkdir build && cd build
cmake ..
make
```

<a name="build_windows"/>
## Windows

Pre-built binaries are provided in the releases section.

You will need to install CMake and MinGW (with make) to build ty under Windows. Visual Studio is not supported at the moment.

<a name="usage"/>
# Usage

You can manage multiple devices connected simultaneously, ty uniquely identifies each device by its position in the host USB topology. Meaning if it stays on the same USB port, it is the same device for ty. That's necessary because across reboots and resets, Teensies look completely different to the host. Use `ty list` to discover teensies (`--verbose` to get details).

When you want to target a specific device, use `ty --device <path>#<serial> <command>`. Path is actually specific to ty and is the one returned by `ty list`, and serial is the USB serial number. Either can omitted.

<a name="usage_upload"/>
## Upload firmware

`ty upload <filename.hex>` will upload a specific firmware to your device. It is checked for compatibility with your model before being uploaded. By default, a reboot is triggered but you can use `--wait` to wait for the bootloader to show up, meaning ty will wait for you to press the button on your board.

<a name="usage_monitor"/>
## Serial monitor

`ty monitor` opens a text connection with your Teensy. It is either done through the serial device (/dev/ttyACM*) or through the HID serial emulation (SEREMU) in other USB modes. ty uses the correct mode automatically.

You can use the `--reconnect` option to detect I/O errors (such as a reset, or after a brief unplugging) and reconnect immediately. Other errors will exit the program.

The `--raw` option will disable line-buffering/editing and immediately send everything you type in the terminal.

See `ty help monitor` for other options. Note that Teensy being a USB device, serial settings are ignored. They are provided in case your application uses them for specific purposes.

<a name="usage_misc"/>
## Other commands

You can learn about other commands using `ty help`. Get specific help for these using `ty help <command>`. The main ones are:
* list: list teensies, and show details if used with `--verbose`
* reset: reset the Teensy device (by issuing a reboot followed by a reset)
