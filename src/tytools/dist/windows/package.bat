@echo off
setlocal EnableDelayedExpansion

cd %~dp0
cd ..\..\..\..

start boostrap.bat
felix -pParanoid tycmd tycommander tycommanderc tyuploader

for /f "tokens=2 delims= " %%i in ('bin\Fast\tycmd.exe --version') do (
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

set PACKAGE_DIR=bin\Packages\tytools\windows
mkdir %PACKAGE_DIR%

copy bin\Fast\tycmd.exe %PACKAGE_DIR%\tycmd.exe
copy bin\Fast\TyCommander.exe %PACKAGE_DIR%\TyCommander.exe
copy bin\Fast\TyCommanderC.exe %PACKAGE_DIR%\TyCommanderC.exe
copy bin\Fast\TyUploader.exe %PACKAGE_DIR%\TyUploader.exe
copy src\tytools\README.md %PACKAGE_DIR%\README.md
copy src\tytools\LICENSE.txt %PACKAGE_DIR%\LICENSE.txt
copy src\tytools\dist\windows\tytools.wxi %PACKAGE_DIR%\tytools.wxi
copy src\tytools\LICENSE.rtf %PACKAGE_DIR%\LICENSE.rtf
copy src\tytools\assets\images\tycommander.ico %PACKAGE_DIR%\tytools.ico
copy src\tytools\dist\windows\banner.jpg %PACKAGE_DIR%\banner.jpg
copy src\tytools\dist\windows\dialog.jpg %PACKAGE_DIR%\dialog.jpg

echo ^<?xml version="1.0" encoding="utf-8"?^> > %PACKAGE_DIR%\tytools.wxs
echo ^<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"^> >> %PACKAGE_DIR%\tytools.wxs
echo     ^<Package Language="1033" >> %PACKAGE_DIR%\tytools.wxs
echo              Scope="perMachine" >> %PACKAGE_DIR%\tytools.wxs
echo              Manufacturer="Niels Martignène" Name="TyTools" Version="%VERSION%" >> %PACKAGE_DIR%\tytools.wxs
echo              ProductCode="*" >> %PACKAGE_DIR%\tytools.wxs
echo              UpgradeCode="72663aca-47a7-4b9b-aa53-aa067b872b8a"^> >> %PACKAGE_DIR%\tytools.wxs
echo         ^<?include tytools.wxi ?^> >> %PACKAGE_DIR%\tytools.wxs
echo     ^</Package^> >> %PACKAGE_DIR%\tytools.wxs
echo ^</Wix^> >> %PACKAGE_DIR%\tytools.wxs

cd %PACKAGE_DIR%

REM Create MSI package
wix extension add -g WixToolset.UI.wixext
wix build tytools.wxs -o ..\..\tytools_%VERSION%_win64.msi -ext WixToolset.UI.wixext

REM Create ZIP file
tar.exe -a -c -f ..\..\tytools_%VERSION%_win64.zip README.md LICENSE.txt tycmd.exe TyCommander.exe TyCommanderC.exe TyUploader.exe
