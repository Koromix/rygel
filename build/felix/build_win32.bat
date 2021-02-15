@echo off

setlocal enableDelayedExpansion
cd %~dp0

set SRC=..\..\src\felix\*.cc ..\..\src\core\libcc\libcc.cc ..\..\src\core\libwrap\json.cc ..\..\vendor\miniz\miniz.c
set BIN=..\..\felix.exe

where /q link
if NOT ERRORLEVEL 1 (
    where /q cl
    if NOT ERRORLEVEL 1 (
        echo Bootstrapping felix with MSVC...
        mkdir tmp
        cl /nologo /std:c++latest /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /c %SRC% /Fotmp\
        link /nologo tmp\*.obj /out:tmp\felix.exe
        tmp\felix.exe -m Fast -f Static -O tmp\fast felix
        move tmp\fast\felix.exe %BIN%

        echo Cleaning up...
        ping -n 4 127.0.0.1 >NUL
        rmdir /S /Q tmp\fast
        del /Q tmp\*
        rmdir /Q tmp

        exit /B
    )

    where /q clang-cl
    if NOT ERRORLEVEL 1 (
        echo Bootstrapping felix with Clang...
        mkdir tmp
        clang-cl /nologo /std:c++latest /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /c %SRC% /Fotmp\
        lld-link /nologo tmp\*.obj /out:tmp\felix.exe
        tmp\felix.exe -m Fast -f Static -O tmp\fast felix
        move tmp\fast\felix.exe %BIN%

        echo Cleaning up...
        ping -n 4 127.0.0.1 >NUL
        rmdir /S /Q tmp\fast
        del /Q tmp\*
        rmdir /Q tmp

        exit /B
    )
)

where /q g++
if NOT ERRORLEVEL 1 (
    echo Bootstrapping felix with GCC...
    mkdir tmp
    g++ -std=gnu++2a -O0 -DNDEBUG -DNOMINMAX  -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE %SRC% -w -otmp\felix.exe
    tmp\felix.exe -m Fast -f Static -O tmp\fast felix
    move tmp\fast\felix.exe %BIN%

    echo Cleaning up...
    ping -n 4 127.0.0.1 >NUL
    rmdir /S /Q tmp\fast
    del /Q tmp\*
    rmdir /Q tmp

    exit /B
)

echo Could not find any compiler (cl/link, clang-cl/link, g++) in PATH
echo To build with Clang or MSVC, you need to use the Visual Studio command prompt
