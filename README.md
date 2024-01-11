You can find these instructions on the [official web page](http://koromix.dev/tytools).

# Overview

TyTools is a collection of **independent tools** and you only need one executable to use
any of them. The Qt-based GUI tools are statically compiled to make that possible.

Tool        | Type                      | Description
----------- | ------------------------- | ----------------------------------------------------
TyCommander | Qt GUI (static)           | Upload, monitor and communicate with multiple boards
TyUploader  | Qt GUI (static)           | Simple firmware / sketch uploader
tycmd       | Command-line<br>_No Qt !_ | Command-line tool to manage Teensy boards

Download the [latest release from GitHub](https://github.com/Koromix/tytools/releases). You can
find [development builds on koromix.dev](https://koromix.dev/files/tytools/).

All the code related to these programs is under **public domain**, you can do whatever you want
with it. See the LICENSE file or [unlicense.org](https://unlicense.org/) for more information.

Use the following links for more information:

- Documentation: https://koromix.dev/tytools
- Changelog: https://github.com/Koromix/rygel/blob/master/src/tytools/CHANGELOG.md

# Source code

This repository does not contain the code of TyTools but only exists as a front. For pratical reasons, I've started using a single repository for all my projects in 2018 because it is easier to manage.

The source code is available here: https://github.com/Koromix/rygel/ (in the *src/tytools* subdirectory).

Monorepositories have two killer features for me:

- Cross-project refactoring
- Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
