ty
==

ty manages things Teensy devices (or teensies) and Teensy projects.

You can manage multiple devices connected simultaneously, ty uniquely identifies each device by its position in the host USB topology. Meaning if it stays on the same USB port, it is the same device for ty. That's necessary because across reboots and resets, Teensies look completely different to the host. Use `ty list` to discover teensies (`--verbose` to get details).

When you want to target a specific device, use `ty --device <path>#<serial> <command>`. Path is actually specific to ty and is the one returned by `ty list`, and serial is the USB serial number. Either can omitted.

Upload firmware
------

`ty upload <filename.hex>` will upload a specific firmware to your device. It is checked for compatibility with your model before being uploaded. By default, a reboot is triggered but you can use `--wait` to wait for the bootloader to show up, meaning ty will wait for you to press the button on your board.

Serial monitor
------

`ty monitor` opens a text connection with your Teensy. It is either done through the serial device (/dev/ttyACM*) or through the HID serial emulation (SEREMU) in other USB modes. ty uses the correct mode automatically.

You can use the `--reconnect` option to detect I/O errors (such as a reset, or after a brief unplugging) and reconnect immediately. Other errors will exit the program.

The `--raw` option will disable line-buffering/editing and immediately send everything you type in the terminal.

See `ty help monitor` for other options. Note that Teensy being a USB device, serial settings are ignored. They are provided in case your application uses them for specific purposes.

Other commands
------

You can learn about other commands using `ty help`. Get specific help for these using `ty help <command>`. The main ones are:
* list: list teensies, and show details if used with `--verbose`
* reset: reset the Teensy device (by issuing a reboot followed by a reset)
