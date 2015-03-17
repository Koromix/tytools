@echo off

setlocal

set OUTFILE=%1
if [%OUTFILE%] == [] set OUTFILE=CON

(

for /F %%s in ('git describe --tags') do set TY_VERSION=%%s

if DEFINED TY_VERSION (
    set TY_VERSION=%TY_VERSION:~1%

    echo /*
    echo * This Source Code Form is subject to the terms of the Mozilla Public
    echo * License, v. 2.0. If a copy of the MPL was not distributed with this
    echo * file, You can obtain one at http://mozilla.org/MPL/2.0/.
    echo */
    echo.
    echo #ifndef TY_VERSION_H
    echo #define TY_VERSION_H
    echo.
    echo #define TY_VERSION "%TY_VERSION%"
    echo.
    echo #endif
) else (
    type "%~dp0\..\src\version.h"
)

) >%OUTFILE% 2>NUL

endlocal
