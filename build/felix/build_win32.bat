@echo off

setlocal enableDelayedExpansion
cd %~dp0

set SRC=..\..\src\felix\*.cc ..\..\src\libcc\libcc.cc ..\..\vendor\miniz\miniz.c
set BIN=..\..\felix.exe

where /q link
if NOT ERRORLEVEL 1 (
    where /q clang-cl
    if NOT ERRORLEVEL 1 (
        echo Building felix with Clang
        clang-cl /W0 /EHsc /MP /O2 /DNDEBUG /c %SRC% /Fo.\
        lld-link /nologo *.obj shlwapi.lib /out:%BIN%
        del *.obj
        exit /B
    )

    where /q cl
    if NOT ERRORLEVEL 1 (
        echo Building felix with MSVC
        cl /nologo /W0 /EHsc /MP /O2 /DNDEBUG /c %SRC% /Fo.\
        link /nologo *.obj shlwapi.lib /out:%BIN%
        del *.obj
        exit /B
    )
)

where /q g++
if NOT ERRORLEVEL 1 (
    echo Building felix with GCC
    g++ -std=gnu++17 -O2 -DNDEBUG %SRC% -lshlwapi -o%BIN%
    exit /B
)

echo Could not find any compiler (cl/link, clang-cl/link, g++) in PATH
echo To build with Clang or MSVC, you need to use the Visual Studio command prompt
