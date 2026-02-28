@ECHO OFF
IF NOT EXIST "build\" mkdir build
PUSHD build

SET platform="-m64"
SET platformname="x64"
IF "%1"=="x64" ( GOTO COMPILE )
IF "%1"=="x86" (
SET platform="-m32"
SET platformname="x86"
) ELSE ( GOTO COMPILE )
:COMPILE

:COMPILE
REM include debug symbols -Z7
REM debug mode -Od

cl ..\\game.cpp -EHa -O2 user32.lib gdi32.lib winmm.lib
POPD
