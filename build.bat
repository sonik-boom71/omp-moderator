@echo off
setlocal

rem On PATH rather than quoted: a quoted path containing "(x86)" breaks for /f.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :no_vs

rem open.mp for Windows is 32-bit, so the component must be too.
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
cd /d "%~dp0"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build || exit /b 1
ctest --test-dir build --output-on-failure || exit /b 1

echo.
echo Done: build\Moderator.dll
exit /b 0

:no_vs
echo Visual Studio with the C++ workload was not found.
exit /b 1
