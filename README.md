![TyQt Logo](/contrib/images/readme_banner.png)

TyQt can help you track and monitor your Teensy devices. It runs on Linux, Windows and Mac OS X.

Two other tools are provided alongside TyQt:
- UpTy is an easy to use barebone Teensy firmware updater
- tyc is the command-line equivalent of TyQt and provides a similar feature set

Download the [latest release from GitHub](https://github.com/Koromix/ty/releases). You can find development [builds on BinTray](https://bintray.com/koromix/ty/ty/view#files).

- [Command-line usage](#tyc)
  - [List devices](#tyc_list)
  - [Upload firmware](#tyc_upload)
  - [Serial monitor](#tyc_monitor)
  - [Bootloader and reset](#tyc_reset)
- [Build instructions](#build)
  - [Linux](#build_linux)
  - [Windows](#build_windows)
  - [Mac OS X](#build_darwin)

<a name="tyc"/>
# Command-line usage

You can manage multiple devices connected simultaneously, ty uniquely identifies each device by its
position in the host USB topology. Meaning if it stays on the same USB port, it is the same device
for ty. That's necessary because across reboots and resets, Teensies look completely different to
the host.

To target a specific device, use `tyc <command> --board "[<serial>][-<family>][@<location>]"`.
_serial_ is the USB serial number, _family_ is the board family name and _location_ can be the
virtual path computed by ty (see `tyc list`) or an OS device path (e.g. /dev/hidraw1 or COM1).
Any of them can be omitted. See the examples in the table below.

Tag filter       | Result
---------------- | ---------------------------------------------------------------------------
714230           | Select board with serial number 714230
-Teensy          | Select board with family name 'Teensy'
@usb-1-2-2       | Select board plugged in USB port 'usb-1-2-2'
@COM1            | Select board linked to the OS-specific device 'COM1'
714230@usb-1-2-2 | Select board plugged in 'usb-1-2-2' and with serial number is 714230

You can learn about the various commands using `tyc help`. Get specific help for them using
`tyc help <command>`.

<a name="tyc_list"/>
## List devices

`tyc list` lists plugged Teensy devices. Here is how it looks:
```
add 34130@usb-1-2 Teensy 3.1
add 29460@usb-4-2 Teensy
add 32250@usb-4-3 Teensy 3.0
```

Use `--verbose` if you want detailed information about available devices:
```
add 32250@usb-4-3 Teensy 3.0
  + capabilities:
    - upload
    - reset
  + interfaces:
    - HalfKay Bootloader: /dev/hidraw2
```

If you need to read structured information in your scripts, you can set the output to JSON with `--output json`:
```
{"action": "add", "tag": "714230@usb-6-3", "serial": 714230, "location": "usb-6-3", "model": "Teensy", "capabilities": ["reboot", "serial"], "interfaces": [["Seremu", "/dev/hidraw4"]]}
{"action": "add", "tag": "1126140@usb-6-2", "serial": 1126140, "location": "usb-6-2", "model": "Teensy LC", "capabilities": ["upload", "reset"], "interfaces": [["HalfKay Bootloader", "/dev/hidraw3"]]}
```

You can also watch device changes with `--watch`, both in plain and JSON mode.

Action | Meaning
------ | ------------------------------------------------------------------------------
add    | This board was plugged in or was already there
change | Something changed, maybe the board rebooted
miss   | This board is missing, either it was unplugged (remove) or it is changing mode
remove | This board has been missing for some time, consider it removed

<a name="tyc_upload"/>
## Upload firmware

Use `tyc upload <filename.hex>` to upload a specific firmware to your device. It is checked for
compatibility with your model before being uploaded.

By default, a reboot is triggered but you can use `--wait` to wait for the bootloader to show up,
meaning ty will wait for you to press the button on your board.

<a name="tyc_monitor"/>
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

<a name="tyc_reset"/>
## Bootloader and reset

`tyc reset` will restart your device. Since Teensy devices (at least the ARM ones) do not provide
a way to trigger a reset, ty will instead start the bootloader first and then issue a reset
without programming anything.

You can also use `tyc reset -b` to start the bootloader. This is the same as pushing the button on
your Teensy.

<a name="build"/>
# Build instructions

You can download a source release from the release page on GitHub or clone the repository.
Pre-built binaries are available for Windows and Mac OS X.

ty can be built using GCC or Clang.

<a name="build_linux"/>
## Linux

### Prerequisites

To install the dependencies on Debian or Ubuntu execute:
```bash
sudo apt-get install build-essential cmake libudev-dev qtbase5-dev
```

On Arch Linux you can do so (as root):
```bash
pacman -S --needed base-devel cmake udev qt5-base
```

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

You need to install [CMake](http://www.cmake.org/) and MinGW-w64 to build ty under Windows.
Visual Studio is not supported at the moment, nor is the historical MinGW toolchain. You should
add it to the PATH variable when the installer asks you to.

Using [Qt Creator](http://www.qt.io/download-open-source/) is probably the easiest option to build
ty on Windows. If you use the online installer, make sure to select the compiler on the components
page, in Tools > MinGW (use the latest version).

<a name="build_darwin"/>
## Mac OS X

Pre-built binaries are provided in the releases section.

Install Xcode, the developer command-line tools, [CMake](http://www.cmake.org/) and
[Qt Creator](http://www.qt.io/download-open-source/). The native Clang compiler can build ty.
