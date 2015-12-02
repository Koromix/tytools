@echo off
REM ty, a collection of GUI and command-line tools to manage Teensy devices
REM
REM Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
REM Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

setlocal

set OUTFILE=%1
if NOT [%OUTFILE%] == [] (
    call "%~f0" >"%OUTFILE%"
    goto end
)

(
    for /F %%t in ('git describe --tags') do (
        set tag=%%t
        goto git_version
    )
) 2>NUL
goto src_version

:git_version
echo /*
echo  * ty, a collection of GUI and command-line tools to manage Teensy devices
echo  *
echo  * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
echo  * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
echo  */
echo.
echo #ifndef TY_VERSION_H
echo #define TY_VERSION_H
echo.
echo #define TY_VERSION "%tag:~1%"
echo.
echo #endif
goto end

:src_version
type "%~dp0\..\libty\version.h"
goto end

:end
endlocal
