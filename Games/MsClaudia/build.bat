@ECHO OFF
set DisabledWarnings= -wd4100 -wd4189 -wd4800 -wd4244 -wd4201 -wd4459 -wd4458 -wd4091

set CommonCompilerFlags=-MTd -D_CRT_SECURE_NO_WARNINGS -nologo -Gm- -GR- -EHa -O2 -Oi %DisabledWarnings%  -WX -W4 -FC

set CommonLinkerFlags= -MACHINE:X64 -incremental:no -opt:ref  Shell32.lib user32.lib gdi32.lib winmm.lib

IF NOT EXIST "build\" mkdir build
PUSHD build
rc /fo resource.res ..\\resource.rc 

echo %CommonCompilerFlags% | findstr /I /C:"/O2" /C:"-O2"  >nul
if %errorlevel%==0 (
    SET CONFIG=Release
    SET OUT_NAME=MsClaudia_RELEASE.exe
    SET EXTRA_DEFS=/DNDEBUG
) else (
    SET CONFIG=Debug
    SET OUT_NAME=MsClaudia_DEBUG.exe
    SET EXTRA_DEFS=/D_DEBUG
)


if exist *.obj del *.obj
cl %CommonCompilerFlags% %EXTRA_DEFS% ..\\game.cpp ..\\loader.cpp resource.res /link /SUBSYSTEM:WINDOWS /LIBPATH:..\\lib %CommonLinkerFlags% /OUT:%OUT_NAME%
POPD build