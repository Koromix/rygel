@echo off

setlocal enableDelayedExpansion
cd %~dp0

set SRC=*.cc ..\core\libcc\libcc.cc ..\core\libwrap\json.cc ..\..\vendor\pugixml\src\pugixml.cpp

set TEMP=..\..\bin\BootstrapFelix
set BUILD=..\..\bin\Fast
set BINARY=..\..\felix.exe

if EXIST %BINARY% (
    %BINARY% -pFast felix && copy %BUILD%\felix.exe %BINARY% && exit /B
    if EXIST %BINARY% del %BINARY%
)

where /q cl
if NOT ERRORLEVEL 1 (
    where /q clang-cl
    if NOT ERRORLEVEL 1 (
        echo Bootstrapping felix with Clang...
        mkdir %TEMP%
        clang-cl /nologo /std:c++20 /I../.. /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /c %SRC% /Fo%TEMP%\
        lld-link /nologo %TEMP%\*.obj ws2_32.lib advapi32.lib shell32.lib ole32.lib /out:%TEMP%\felix.exe
        %TEMP%\felix.exe -pFast felix %*
        copy %BUILD%\felix.exe %BINARY% >NUL

        echo Cleaning up...
        timeout /T 2 /NOBREAK >NUL
        del /Q %TEMP%\*
        rmdir /Q %TEMP%

        exit /B
    )

    echo Bootstrapping felix with MSVC...
    mkdir %TEMP%
    cl /nologo /std:c++20 /I../.. /W0 /EHsc /MP /DNDEBUG /DNOMINMAX /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE /c %SRC% /Fo%TEMP%\
    link /nologo %TEMP%\*.obj ws2_32.lib advapi32.lib shell32.lib ole32.lib /out:%TEMP%\felix.exe
    %TEMP%\felix.exe -pFast felix %*
    copy %BUILD%\felix.exe %BINARY% >NUL

    echo Cleaning up...
    timeout /T 2 /NOBREAK >NUL
    del /Q %TEMP%\*
    rmdir /Q %TEMP%

    exit /B
)

where /q g++
if NOT ERRORLEVEL 1 (
    echo Bootstrapping felix with GCC...
    mkdir %TEMP%
    g++ -std=gnu++2a -O0 -I../.. -DNDEBUG -DNOMINMAX  -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE %SRC% -lws2_32 -ladvapi32 -lshell32 -lole32 -luuid -w -o%TEMP%\felix.exe
    %TEMP%\felix.exe -pFast felix %*
    copy %BUILD%\felix.exe %BINARY% >NUL

    echo Cleaning up...
    timeout /T 2 /NOBREAK >NUL
    del /Q %TEMP%\*
    rmdir /Q %TEMP%

    exit /B
)

echo Could not find any compiler (cl/link, clang-cl/link, g++) in PATH
echo To build with Clang or MSVC, you need to use the Visual Studio command prompt
exit /B 1
