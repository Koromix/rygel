@echo off
setlocal EnableDelayedExpansion

cd %~dp0
cd ..\..\..\..

call bootstrap.bat
felix.exe -pFast rekkord

for /f "tokens=2 delims= " %%i in ('bin\Fast\rekkord.exe --version') do (
    set RAW_VERSION=%%i
    goto out
)
:out

for /f "tokens=1,2 delims=-_" %%i in ("%RAW_VERSION%") do (
    set version=%%i
    set part=%%j
)
if "%part%"=="" (
    set VERSION=%version%
) else (
    set VERSION=%version%.%part%
)

set PACKAGE_DIR=bin\Packages\rekkord\windows
mkdir %PACKAGE_DIR%

copy bin\Fast\rekkord.exe %PACKAGE_DIR%\rekkord.exe
copy src\rekkord\README.md %PACKAGE_DIR%\README.md
copy LICENSE.txt %PACKAGE_DIR%\LICENSE.txt

cd %PACKAGE_DIR%

REM Create ZIP file
tar.exe -a -c -f ..\..\rekkord_%VERSION%_win64.zip LICENSE.txt README.md rekkord.exe
