## TyTools 0.9.9 (2023-08-06)

**Main changes:**

- Tolerate longer delays in Teensy code
- Port TyTools to Qt 6

**Platform support:**

- Only macOS 11, 12, 13 are supported
- Only Windows 10 and 11 (64-bit) are supported

**Repository change :**

- This repository does not contain any code anymore because I host [everything in my monorepo](https://github.com/Koromix/rygel)
- For reasons explained here: https://github.com/Koromix/rygel#mono-repository

## TyTools 0.9.8 (2022-10-02)

- Support Teensy USB Type Serial + MTP (@KurtE)
- Support infinite log size

## TyTools 0.9.7 (2022-01-23)

- Fix upload failure regression introduced in version 0.9.6

## TyTools 0.9.6 (2022-01-17)

**Main Fixes:**

- Fix support for big firmware files (for Teensy 4.0 and 4.1)
- Signal Serial readiness to Teensy in SEREMU mode
- Increase wait delay after board reset

**Other changes:**

- Companion app TyUpdater is renamed to TyUploader

## TyTools 0.9.5 (2021-11-03)

**New Features:**

- Add per-board RTC mode setting to TyCommander (localtime, UTC, ignore)

**Other changes:**

- Change URL in About dialog
- Remove leftover debug statement

## TyTools 0.9.4 (2021-10-21)

**New Features:**

- Set RTC time when uploading to Teensy 4.0 and 4.1 (thanks @mecparts)
- Support Teensy Micro Mod (@KurtE)

**Other changes:**

- Use bright blue icon for TyCommander and bright green for TyUpdater
- Fix Qt HighDPI support being set incorrectly

## TyTools 0.9.3 (2020-11-26)

- Fix wrong COM port detection with Teensyduino 1.54 on Windows (thanks to @luni64 and @Defragster)

## TyTools 0.9.2 (2020-08-07)

**New Features:**

- Show Dual/Triple Serial Teensy builds as multiple boards

**Other changes:**

- Add TyCommander upload --delegate to cooperate with Teensy Loader
- Fix support for composite devices on Windows 7 (e.g. Serial + MIDI)
- Remove all Windows XP compatibility code

**OS support:**

- Prevous release builds did not actually work on Windows XP, but the code for it was still there...
- ... now it is official, and it is not possible to build TyTools or libhs with XP support anymore

## TyTools 0.9.1 (2020-06-27)

**New Features:**

- Support Teensy 4.1 (@KurtE)

**Other changes:**

- Disable autocompletion in serial input field
- Allow 1984K flash to be used on Teensy 4.0
- Add VID:PID pairs for dual and triple serial Teensy sketches

## TyTools 0.9.0 (2019-12-02)

**New Features:**

- Support Teensy 4 (caveat: no RTC programming yet)
- Support macOS Catalina
- Configurable per-board serial rate (useful for serial non-Teensy boards)

**Main Fixes:**

- Look less ugly on HiDPI screens
- Fix ugly "classic Windows" style in TyUpdater
- Fix extra empty lines in serial monitor in some cases
- Fix JSON format in tycmd list -Ojson (string escaping, array)

## TyTools 0.8.11 (2019-03-31)

**New Features:**

- Support Teensy 4 beta boards (experimental)

**OS support:**

- macOS 10.10 (Yosemite) is now the oldest supported version
- Windows 7 is now the oldest supported version

**Other changes:**

- Use Qt 5.12.2 on Windows
- Use Qt 5.9.7 on macOS

## TyTools 0.8.10 (2018-09-23)

**New Features:**

- Detect running Teensy model with bcdDevice descriptor value (introduced in Teensyduino 1.42)
- Add @-prefixed commands 'send_file' and 'send' to TyCommander serial input
- Support loading firmwares from stdin in tycmd upload and tycmd identity

**Main Fixes:**

- Fix errors with Teensy 3.5 firmwares (caused by 'RAM = 256K' change introduced Teensyduino 1.42)
- Fix rare Intel HEX parse errors

*Note: This is the mostly the same as 0.8.9 except this time the macOS version works. For Windows and Linux users, nothing has changed (except the version number).*

## TyTools 0.8.8 (2019-08-20)

**Main Fixes:**

- Fix firmware model detection with LTO, -mpure-code
- Fix occasional serial read truncation with HID boards on Win32

**Miscellaneous:**

- Remove artificial Teensy upload delays (makes upload faster)

## TyTools 0.8.7 (2017-06-06)

*This project has been renamed to TyTools, along with the tools included in this project.*

Old name | New name
-------- | -----------
TyQt     | TyCommander
tyc      | tycmd
UpTy     | TyUpdater

**New Features:**

- Support non-Teensy USB serial devices ("Generic" boards)
- Customize board models and PID:VID matching with TyTools.ini
- Add option to configure board serial log directory
- Add tyc identify to analyze firmware models
- Add TyCommanderC attach (and detach) commands

**Main Fixes:**

- Fix crash with some USB3 controllers on Windows (pre-10)
- Restore compatibility with Mac OS X Yosemite
- Fix broken Teensy firmware compatibility check with LTO builds
- Expand Teensy VID/PID list for new USB modes
- Support FTDI chips (@jbliesener)
- Fix incorrect use of case-sensitive test with firmware extensions
- Configure serial speed (115200 bauds) in tycmd and TyCommander
- Fix board selection behavior on insertion and removal
- Fix optline bug with non-option permutation and option values

**UI changes:**

- Change serial input field to QComboBox, which supports history dropdown (@tni)
- Reduce board list wiget height and show tag on top
- Use monospaced fonts in serial monitor
- Turn board log path label into clickable URLs with tooltips (@tni)

**Miscellaneous:**

- Harmonize libty and TyCommander INI configuration path
- Echo 'Send File' (when echo is enabled) commands in serial log
- Rename 'TY_' environment variables to 'TYTOOLS_'
- Add tweakable board drop delay
- Increase limit number of board log files from 3 to 4
- Rotate board log file when 'Clear on reset' is checked
- Add Win32 crash memory dumper to TyCommander
- Use Unity builds for releases
- Upgrade Win32 and OSX builds to Qt 5.7.1
- Improve thread-safety of serial log file handling
- Provide single-file amalgamation of libhs
- Replace all uses of linked lists with dynamic arrays
- Various small fixes and code improvements

## TyQt 0.8.0 (2016-11-05)

**New Features:**

- Add support for Teensy 3.5 and 3.6, including beta versions
- Implement command-line multi-action with TyQt --multi
- Add Send File to serial and Seremu boards
- Add per-board serial log file
- Add simple firmware updater application called UpTy
- Improve Arduino integration when building for different board models

**Main Fixes:**

- Enumerate boards / devices without opening them
- Improve USB 3.0 device enumeration on Windows (invisible Teensies)
- Fix TyQt freeze with blocking serial writes
- Fix discovery of Serial IAD devices on Windows (Windows 10 only)
- Fix potential crash after application reset
- Assert DTR in all cases when opening serial devices
- Wait for device enumeration on Win32 when the driver wizard is running
- Fix 'Clear on Serial' behavior when enabling serial
- Fix support for Windows running under Virtual PC (probably)

**UI changes:**

- Add collapsible groups on settings page in Compact Mode
- Add keyboard shortcuts to switch board (Ctrl + Tab)
- Merge Compact Mode and splitter collapse
- Show board product string in tyc and TyQt Info tab
- Show task progress in Compact Mode
- Focus the serial input widget when changing tab or board selection
- Many other small GUI tweaks

**Miscellaneous:**

- Add Teensy model / usage debug information
- Add log statements for added and removed devices
- Show relevant task in log messages
- Improve board task error messages
- Continue 'tyc upload' when some firmwares are invalid
- Add global setting to disable serial by default
- Small tyc command-line changes
- Various small fixes and code improvements

## TyQt 0.7.6 (2016-07-28)

- Fix incorrect exit code with TyQt remote commands
- (this fixes the error message when uploading from Arduino 1.6.10)
- Fix potential crashes in HID code on OSX
- Fix intermittent crash when enumerating some devices on Win32
- Fix incorrect I/O error messages in HalfKay code
- Add debug log with Teensy usage value
- Use smaller 128x128 icons to waste less memory
- Build release binaries with Qt 5.6.1-1

## TyQt 0.7.5 (2016-04-29)

*The windows version is now built with MSVC 2015 and is relatively smaller.*

**Main Changes:**

- Improve Compact Mode (board selection combobox, action toolbar)
- Add context menu to the board list
- Add global Preferences dialog
- Add setting to limit number of parallel tasks
- Enable Teensy++ 2.0 support by default
- Configure new windows like the current one
- Add dedicated Log Window (errors and debug)
- Add action to reset TyQt and settings
- Add per-board scrollback size setting
- Add per-board encoding setting
- Disable persistent settings for ambiguous boards (without a unique S/N)
- Memorize the full board model when possible
- Keep a list of recent firmwares for each board
- Add action to forget the firmware associated with a board
- Show RawHID interface device path
- Add pending board status icon
- Use slightly more colorful status icons
- Move newline / echo settings to Monitor tab
- Make attachMonitor a persistent board setting
- Add more context to some errors messages
- Avoid useless error / warning when rebooting a board already in bootloader mode
- Disable UI actions for busy boards

**Main Fixes:**

- Fix IRP leak on Win32 that leads to non-paged memory exhaustion after a while
- Fix random serial timeout behavior with Serial devices on Win32
- Fix support for USB 3.0 ports on OSX (missing devices)
- Improve robustness / checks in Intel HEX parser
- Fix autoscroll quirks with the Monitor text control
- Use lower maxTasks by default to limit USB problems (especially on Windows)
- Fix crash with extension-less firmware filenames
- Fix tyc monitor statements not respecting --quiet
- Fix relatively slow UI updates on board changes
- Try harder to show the board selection dialog on top

**Code Refactoring:**

- Extract "low-level" cross-platform device code to libhs
- Port to MSVC for the Windows packages
- Diffuse code refactoring

## TyQt 0.7.0 (2016-01-20)

**New features:**

- Select and work on multiple boards at once
- Add Arduino integration dialog
- Attach/detach function to let other softwares access the device
- Rename/alias boards in TyQt
- Save individual board settings
- Add tyqtc to pilot TyQt (win32)
- Add JSON output in tyc with tyc list -Ojson

**Main Changes:**

- Vastly improved monitor performance
- Show board status with dedicated icons
- Visual warning on task errors
- Show last uploaded firmware in board list
- Board tags are now "-" (e.g. 112340-Teensy)
- Drop missing boards after a longer delay
- Faster firmwares uploads
- Use short COM port names when possible (win32)
- Static binaries (no dependency on libty.dll)
- Improve command-line syntax for TyQt (close to tyc)
- Make TyQt autostart opt-in (--autostart)
- Select board using device path in tyc/tyqt (e.g. "tyc upload --board @COM1")
- Extend board selection syntax to "[][-][@]"
- Increase size of monitor scrollback in TyQt to 200000 lines
- Resizable board list using a QSplitter
- Replace "Upload" tab with "Settings" tab
- Allow selection of missing boards in TyQt
- Disable word-wrap in monitor text widget
- Add monitor send button
- Add keyboard shortcut to clear monitor (Ctrl + Alt + X)
- Remove annoying LICENSE prompt from OSX bundles

**Main Fixes:**

- Support El Capitan (osx)
- Fix freeze of serial read in some cases on Win32
- Better handling of device close on Windows XP
- Hide harmless spurious I/O errors when uploading
- Make README.txt/LICENSE.txt notepad-friendly (win32)

## TyQt 0.6.4 (2015-09-23)

- Add support for Teensy 3.2

## TyQt 0.6.3 (2015-06-16)

**Main changes:**

- Windows XP support
- Longer board drop delay

**Minor fixes:**

- Ensure non-blocking behavior of tyc monitor on Windows
- Apply serial settings in tyc monitor on reconnect
- Always quit TyQt when the last window is closed

## TyQt 0.6.2  (2015-03-29)

- Working Teensy LC support

## TyQt 0.6.1 (2015-03-26)

- Disable Teensy LC support (broken until I can get one)
- Always assert DTR line on Windows and OS X, to not block on "while (!Serial);"
- Ignore devices on permission errors, instead of aborting enumeration

## TyQt 0.6.0 (2015-03-18)

- Tentative Teensy LC support
- Clear on reset option for monitor
- Better autoscrolling behavior in the monitor (keyboard navigation works)
- Go to bootloader with CLI: tyc reset --bootloader

## TyQt 0.5.7 (2015-03-11)

- Use bigger serial buffer to avoid overflows
- Don't hog the CPU if the Teensy sends a lot of stuff
- Better autoscroll behavior in the monitor

## TyQt 0.5.6 (2015-03-09)

- Easier to use command-line tool
- Bug fixes and code cleanup

## TyQt 0.5.5 (2015-02-28)

- Single-instance mode for TyQt: send commands using tyqt.exe (see tyqt.exe --help)
- Minimal interface option to hide board list, toolbar and statusbar
- Bug fixes

## TyQt 0.5.1 (2015-02-20)

- Proper packaging: MSI on Windows, Bundle on OS X
- Better art and icons (well, I try)

## TyQt 0.5.0 (2015-02-10)

- Teensy Qt!

## tyc 0.4.0 (2015-02-12)

- Support Mac OS X

## tyc 0.3.0 (2015-01-12)

- Add native compilation instructions for Windows

## tyc 0.2.1 (2014-12-21)

- Dynamic device detection and management

## ty 0.1.0 (20134-09-01)

- Document the EXPERIMENTAL option in README.md
