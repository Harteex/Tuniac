set ProgFilesRoot=%ProgramFiles%
set ProgFiles86Root=%ProgramFiles(x86)%
if not "%ProgFiles86Root%"=="" goto 64bitWindows
set ProgFiles86Root=%ProgramFiles%
:64bitWindows

set 64bitcl=true
if exist "%ProgFiles86Root%\Microsoft Visual Studio 14.0\VC\bin\amd64\cl.exe" goto 64bitcl
set 64bitcl=false
:64bitcl


call "%Programfiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86

cd ..\..\..\

rem ######## turbojpeglib
cd libjpeg-turbo\


rem #turbojpeg Release x86:
mkdir .\build32
cd build32\
del *.*
"%ProgFiles86Root%\CMake\bin\cmake" -DCMAKE_BUILD_TYPE=Release -DWITH_JPEG8=1 -G "Visual Studio 14" ..\.
devenv libjpeg-turbo.sln /project "jpeg-static" /Clean
devenv libjpeg-turbo.sln /project "jpeg-static" /Rebuild "Release"
if exist ..\Release\x86\ rmdir /S /Q ..\Release\x86
mkdir ..\Release\x86
move /Y .\Release\jpeg-static.lib ..\Release\x86\jpeg-static.lib


cd ..


rem if "64bitcl"=="false" goto done
call "%Programfiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64


rem #turbojpeg Release x64
mkdir .\build64
cd build64\
del *.*
"%ProgFiles86Root%\CMake\bin\cmake" -DCMAKE_BUILD_TYPE=Release -DWITH_JPEG8=1 -G "Visual Studio 14 Win64" ..\.
devenv libjpeg-turbo.sln /Project "jpeg-static" /Clean
devenv libjpeg-turbo.sln /Project "jpeg-static" /Rebuild "Release"
if exist ..\Release\x64\ rmdir /S /Q ..\Release\x64
mkdir ..\Release\x64
move /Y .\Release\jpeg-static.lib ..\Release\x64\jpeg-static.lib

move /Y .\jconfig.h ..\jconfig.h

:done

pause