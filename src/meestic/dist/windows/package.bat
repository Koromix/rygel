@echo off

setlocal EnableDelayedExpansion

cd %~dp0
cd ..\..\..\..

felix -pFast meestic MeesticGui

for /f "tokens=2 delims= " %%i in ('bin\Fast\meestic.exe --version') do (
    set RAW_VERSION=%%i
    goto out
)
:out

for /f "tokens=1,2 delims=_" %%i in ("%RAW_VERSION%") do (
    set version=%%i
    set part=%%j
)
if "%part%" == ""; set part=0
set VERSION=%version%.%part%

set PACKAGE_DIR=bin\Packages\meestic\windows
mkdir %PACKAGE_DIR%

copy bin\Fast\meestic.exe %PACKAGE_DIR%\meestic.exe
copy bin\Fast\MeesticGui.exe %PACKAGE_DIR%\MeesticGui.exe
copy src\meestic\images\meestic.ico %PACKAGE_DIR%\meestic.ico
copy src\meestic\dist\windows\meestic.wxi %PACKAGE_DIR%\meestic.wxi

echo ^<?xml version="1.0" encoding="utf-8"?^> > %PACKAGE_DIR%\meestic.wxs
echo ^<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"^> >> %PACKAGE_DIR%\meestic.wxs
echo     ^<Package Language="1033" >> %PACKAGE_DIR%\meestic.wxs
echo              Scope="perMachine" >> %PACKAGE_DIR%\meestic.wxs
echo              Manufacturer="Niels Martignène" Name="Meestic" Version="%VERSION%" >> %PACKAGE_DIR%\meestic.wxs
echo              ProductCode="*" >> %PACKAGE_DIR%\meestic.wxs
echo              UpgradeCode="92bca6b0-7814-433e-af50-603ad948758e"^> >> %PACKAGE_DIR%\meestic.wxs
echo         ^<?include meestic.wxi ?^> >> %PACKAGE_DIR%\meestic.wxs
echo     ^</Package^> >> %PACKAGE_DIR%\meestic.wxs
echo ^</Wix^> >> %PACKAGE_DIR%\meestic.wxs

cd %PACKAGE_DIR%
wix build meestic.wxs -o meestic_%VERSION%_win64.msi
