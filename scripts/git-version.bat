@echo off

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
echo * This Source Code Form is subject to the terms of the Mozilla Public
echo * License, v. 2.0. If a copy of the MPL was not distributed with this
echo * file, You can obtain one at http://mozilla.org/MPL/2.0/.
echo */
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
