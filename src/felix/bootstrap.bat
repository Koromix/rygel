@echo off

setlocal enableDelayedExpansion
cd %~dp0

set SRC=*.cc ..\core\libcc\libcc.cc ..\core\libwrap\json.cc ..\..\vendor\pugixml\src\pugixml.cpp
set BIN=..\..\felix.exe
set BUILD=..\..\bin\BootstrapFelix

where /q cl
if NOT ERRORLEVEL 1 (
    where /q clang-cl
    if NOT ERRORLEVEL 1 (
        echo Bootstrapping felix with Clang...
        mkdir %BUILD%
        clang-cl /nologo /std:c++latest /I../.. /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /DLIBCC_NO_MINIZ /DLIBCC_NO_BROTLI /c %SRC% /Fo%BUILD%\
        lld-link /nologo %BUILD%\*.obj ws2_32.lib advapi32.lib shell32.lib ole32.lib /out:%BUILD%\felix.exe
        %BUILD%\felix.exe --no_presets --features=OptimizeSpeed,StaticLink -O %BUILD%\Fast felix
        move %BUILD%\Fast\felix.exe %BIN%

        echo Cleaning up...
        ping -n 4 127.0.0.1 >NUL
        rmdir /S /Q %BUILD%\Fast
        del /Q %BUILD%\*
        rmdir /Q %BUILD%

        exit /B
    )

    echo Bootstrapping felix with MSVC...
    mkdir %BUILD%
    cl /nologo /std:c++latest /I../.. /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /DLIBCC_NO_MINIZ /DLIBCC_NO_BROTLI /c %SRC% /Fo%BUILD%\
    link /nologo %BUILD%\*.obj ws2_32.lib advapi32.lib shell32.lib ole32.lib /out:%BUILD%\felix.exe
    %BUILD%\felix.exe --no_presets --features=OptimizeSpeed,StaticLink -O %BUILD%\Fast felix
    move %BUILD%\Fast\felix.exe %BIN%

    echo Cleaning up...
    ping -n 4 127.0.0.1 >NUL
    rmdir /S /Q %BUILD%\Fast
    del /Q %BUILD%\*
    rmdir /Q %BUILD%

    exit /B
)

where /q g++
if NOT ERRORLEVEL 1 (
    echo Bootstrapping felix with GCC...
    mkdir %BUILD%
    g++ -std=gnu++2a -O0 -I../.. -DNDEBUG -DNOMINMAX  -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE -DLIBCC_NO_MINIZ -DLIBCC_NO_BROTLI %SRC% -lws2_32 -ladvapi32 -lshell32 -lole32 -w -o%BUILD%\felix.exe
    %BUILD%\felix.exe --no_presets --features=OptimizeSpeed,StaticLink -O %BUILD%\Fast felix
    move %BUILD%\Fast\felix.exe %BIN%

    echo Cleaning up...
    ping -n 4 127.0.0.1 >NUL
    rmdir /S /Q %BUILD%\Fast
    del /Q %BUILD%\*
    rmdir /Q %BUILD%

    exit /B
)

echo Could not find any compiler (cl/link, clang-cl/link, g++) in PATH
echo To build with Clang or MSVC, you need to use the Visual Studio command prompt
