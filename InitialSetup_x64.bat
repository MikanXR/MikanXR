@echo off
setlocal

set UNZIP_EXE=%~dp0/tools/7zip/7za.exe

:: GSTREAMER_ONLY=1 installs the GStreamer MSIs and nothing else. The MSIs install
:: system-wide, so a job that restored a cached deps folder still has to run them, and it
:: must not wipe the folder it just restored.
IF DEFINED GSTREAMER_ONLY goto gstreamer_only

::Clean up the old build folder
IF EXIST build (
del /f /s /q build > nul
rmdir /s /q build
)

::Clean up the old deps folder
IF EXIST deps (
del /f /s /q deps > nul
rmdir /s /q deps
)

:: Fetch dependencies in the "deps" folders
mkdir deps
pushd deps

:: Download and unzip the prebuilt libs.
:: The SDL devel zips carry the runtime DLLs in lib/x64, so no separate
:: runtime zips are needed.
echo "Downloading SDL2-devel..."
curl https://www.libsdl.org/release/SDL2-devel-2.30.10-VC.zip --output sdl2-devel.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading SDL2-devel-2.30.10-VC.zip"
  goto failure
)
%UNZIP_EXE% e sdl2-devel.zip -y -r -spf
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping SDL2-devel-2.30.10-VC.zip"
  goto failure
)

echo "Downloading SDL2-image-devel..."
curl -L https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.8/SDL2_image-devel-2.8.8-VC.zip --output sdl2img-devel.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading SDL2_image-devel-2.8.8-VC.zip"
  goto failure
)
%UNZIP_EXE% e sdl2img-devel.zip -y -r -spf
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping SDL2_image-devel-2.8.8-VC.zip"
  goto failure
)

echo "Downloading SDL2-ttf-devel..."
curl -L https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.24.0/SDL2_ttf-devel-2.24.0-VC.zip --output sdl2ttf-devel.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading SDL2_ttf-devel-2.24.0-VC.zip"
  goto failure
)
%UNZIP_EXE% e sdl2ttf-devel.zip -y -r -spf
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping SDL2_ttf-devel-2.24.0-VC.zip"
  goto failure
)

echo "Downloading OpenCV..."
curl -L https://github.com/opencv/opencv/releases/download/4.10.0/opencv-4.10.0-windows.exe > opencv-4.10.0-windows.exe
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading opencv-4.10.0-windows.exe"
  goto failure
)
opencv-4.10.0-windows.exe -o"." -y
IF %ERRORLEVEL% NEQ 0 (
  echo "Error running self extracting zip opencv-4.10.0-windows.exe"
  goto failure
)

echo "Downloading glew..."
curl -L https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip --output glew-2.2.0-win32.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading glew-2.2.0-win32.zip"
  goto failure
)
%UNZIP_EXE% e glew-2.2.0-win32.zip -y -r -spf
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping glew-2.2.0-win32.zip"
  goto failure
)

echo "Downloading Spout2"
curl -L https://github.com/leadedge/Spout2/archive/refs/tags/2.007h.zip --output SPOUT.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading SPOUT.zip"
  goto failure
)
%UNZIP_EXE% e SPOUT.zip -y -r -spf
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping SPOUT.zip"
  goto failure
)

if defined SKIP_GSTREAMER goto skip_gstreamer
call :install_gstreamer
IF ERRORLEVEL 1 goto failure
:skip_gstreamer

:: Only MikanARKitVideo uses the CUDA Toolkit and it needs GStreamer too, so a setup
:: that skipped GStreamer skips this as well. SKIP_CUDA=1 declines it on its own, at
:: the cost of that one plugin (see cmake/ThirdParty.cmake).
if defined SKIP_GSTREAMER goto skip_cuda
if defined SKIP_CUDA goto skip_cuda
call :install_cuda
IF ERRORLEVEL 1 goto failure
:skip_cuda

echo "Downloading easy_profiler..."
curl -L https://github.com/yse/easy_profiler/releases/download/v2.1.0/easy_profiler-v2.1.0-msvc15-win64.zip --output easy_profiler-v2.1.0-msvc15-win64.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error easy_profiler-v2.1.0-msvc15-win64.zip"
  goto failure
)
%UNZIP_EXE% e easy_profiler-v2.1.0-msvc15-win64.zip -y -r -spf -oeasy_profiler
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping easy_profiler-v2.1.0-msvc15-win64.zip"
  goto failure
)

:: DirectX Shader Compiler release: its dxc.exe carries the SPIR-V backend the Windows SDK's copy
:: lacks, which the Vulkan path of MikanClientTestCPP compiles its shaders with
echo "Downloading DirectX Shader Compiler..."
curl -L https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip --output dxc_2026_07_29.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading dxc_2026_07_29.zip"
  goto failure
)
%UNZIP_EXE% e dxc_2026_07_29.zip -y -r -spf -odxc
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping dxc_2026_07_29.zip"
  goto failure
)

:: Download pre-compiled libharu library (PDF generator)
echo "Downloading libharu..."
curl -L https://github.com/MikanXR/libharu/releases/download/2.4.5/libharu-2.4.5-static.zip --output libharu-2.4.5-static.zip
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading libharu-2.4.5-static.zip"
  goto failure
)
%UNZIP_EXE% e libharu-2.4.5-static.zip -y -r -spf -olibharu-2.4.5-static
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping libharu-2.4.5-static.zip"
  goto failure
)

echo "Downloading CEF (Chromium Embedded Framework)..."
curl -L https://cef-builds.spotifycdn.com/cef_binary_145.0.27+g4ddda2e+chromium-145.0.7632.117_windows64.tar.bz2 --output cef_binary_windows64.tar.bz2
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading cef_binary_windows64.tar.bz2"
  goto failure
)
"%UNZIP_EXE%" x cef_binary_windows64.tar.bz2 -so | "%UNZIP_EXE%" x -aoa -si -ttar -ocef
IF %ERRORLEVEL% NEQ 0 (
  echo "Error extracting cef_binary_windows64.tar.bz2"
  goto failure
)

:: ONNX Runtime (DirectML flavor) - used by the scene lighting estimator.
:: A .nupkg is a zip. Contains headers + onnxruntime.dll built against DirectML.
echo "Downloading ONNX Runtime DirectML 1.20.1..."
curl -L https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/1.20.1/microsoft.ml.onnxruntime.directml.1.20.1.nupkg --output onnxruntime-directml.nupkg
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading Microsoft.ML.OnnxRuntime.DirectML 1.20.1"
  goto failure
)
%UNZIP_EXE% x onnxruntime-directml.nupkg -oonnxruntime -y > nul
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping onnxruntime-directml.nupkg"
  goto failure
)
del onnxruntime-directml.nupkg

echo "Downloading DirectML 1.15.4..."
curl -L https://api.nuget.org/v3-flatcontainer/microsoft.ai.directml/1.15.4/microsoft.ai.directml.1.15.4.nupkg --output directml.nupkg
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading Microsoft.AI.DirectML 1.15.4"
  goto failure
)
%UNZIP_EXE% x directml.nupkg -odirectml -y > nul
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping directml.nupkg"
  goto failure
)
del directml.nupkg

:: The package ships every architecture (arm, x86, linux, xbox) at ~350MB total.
:: Only x64-win is ever used, so drop the rest to keep deps/ (and the CI cache) small.
for /d %%A in (directml\bin\*) do (
  if /I NOT "%%~nxA"=="x64-win" rmdir /s /q "%%A"
)

:: NuGet tool used to fetch c# packages
echo "Downloading nuget..."
curl -L https://dist.nuget.org/win-x86-commandline/latest/nuget.exe --output nuget.exe

:: Exit back out of the deps folder
popd

EXIT /B 0

:gstreamer_only
IF NOT EXIST deps mkdir deps
pushd deps
call :install_gstreamer
IF ERRORLEVEL 1 goto failure
popd
EXIT /B 0

:: Downloads and runs both GStreamer MSIs. Called from the deps folder.
:install_gstreamer
set GSTREAMER_VERSION=1.26.10
call :install_gstreamer_msi "GStreamer 1.0 (MinGW x86_64)" gstreamer-runtime gstreamer-1.0-mingw-x86_64-%GSTREAMER_VERSION%.msi
IF ERRORLEVEL 1 EXIT /B 1
call :install_gstreamer_msi "GStreamer 1.0 (Development Files) (MinGW x86_64)" gstreamer-devel gstreamer-1.0-devel-mingw-x86_64-%GSTREAMER_VERSION%.msi
IF ERRORLEVEL 1 EXIT /B 1
EXIT /B 0

:: Downloads and installs one GStreamer MSI: %1 is the installed product name to look for,
:: %2 the label used in messages and in the log name, %3 the MSI file name. A product already
:: installed at %GSTREAMER_VERSION% is left alone, because running the MSI over an identical
:: install puts msiexec in maintenance mode, where the secure repair check rejects the devel
:: package's elevated custom action under /qn (error 1730, msiexec exit code 1603).
:: msiexec is a GUI process, and launched plainly from a batch file in an unattended session
:: it returns 0 at once without installing anything (the release runner did exactly that).
:: start /wait blocks until the install is really done and passes its exit code through, /qn
:: keeps it fully silent, and the verbose log names the reason when the exit code is not 0.
:install_gstreamer_msi
call :query_installed_version %1
if "%INSTALLED_VERSION%"=="%GSTREAMER_VERSION%" (
  echo "%~2 %GSTREAMER_VERSION% is already installed, skipping"
  EXIT /B 0
)
echo "Downloading %~2 installer"
curl -L https://gstreamer.freedesktop.org/data/pkg/windows/%GSTREAMER_VERSION%/mingw/%~3 --output %~3
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading %~3"
  EXIT /B 1
)
echo "Installing %~2 (silent, takes a minute or two)"
start /wait "" msiexec /i %~3 /qn /norestart /l*v %~2-install.log
IF %ERRORLEVEL% NEQ 0 (
  echo "Error installing %~2 installer, msiexec exit code %ERRORLEVEL%"
  findstr /i "error return value" %~2-install.log
  EXIT /B 1
)
EXIT /B 0

:: Sets INSTALLED_VERSION to the version the product named %1 is installed at, or to
:: nothing when no such product is installed.
:query_installed_version
set "INSTALLED_VERSION="
set "PRODUCT_KEY="
for /f "delims=" %%K in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" /s /f "%~1" /d /e 2^>nul ^| findstr /b /c:"HKEY_"') do if not defined PRODUCT_KEY set "PRODUCT_KEY=%%K"
if not defined PRODUCT_KEY EXIT /B 0
for /f "tokens=2,*" %%A in ('reg query "%PRODUCT_KEY%" /v DisplayVersion 2^>nul ^| findstr /c:"DisplayVersion"') do set "INSTALLED_VERSION=%%B"
EXIT /B 0

:: Installs the CUDA Toolkit's cudart package, which carries the include\cuda.h and
:: lib\x64\cuda.lib that MikanARKitVideo compiles and links against. Nothing else of the
:: Toolkit is wanted here: no nvcc, no runtime libraries, no display driver. The network
:: installer is a 10MB stub that fetches just the package named on its command line.
:: -s runs it unattended, which is also how its NVIDIA license is accepted, and the
:: install needs administrator rights, so an unelevated setup gets a UAC prompt here.
:: It writes CUDA_PATH machine-wide rather than into this process, so the shell that ran
:: setup will not see it: generate project files from a new shell.
:install_cuda
set CUDA_VERSION=13.1.2
set CUDA_SHORT_VERSION=13.1
set CUDA_MINIMUM_VERSION=13000
set "CUDA_TOOLKIT_DIR=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v%CUDA_SHORT_VERSION%"

:: A Toolkit the build can already use is left alone whatever its version, because
:: installing over a newer one would point CUDA_PATH back at this older release for every
:: project on the machine. CUDA_PATH and the CUDA_VERSION in its cuda.h are what
:: cmake/ThirdParty.cmake decides on, so they are what gets read here too.
set INSTALLED_CUDA_VERSION=0
if exist "%CUDA_PATH%\include\cuda.h" (
  for /f "tokens=3" %%V in ('findstr /b /c:"#define CUDA_VERSION " "%CUDA_PATH%\include\cuda.h"') do set INSTALLED_CUDA_VERSION=%%V
)
if %INSTALLED_CUDA_VERSION% GEQ %CUDA_MINIMUM_VERSION% (
  echo "CUDA Toolkit %INSTALLED_CUDA_VERSION% is already installed, skipping"
  EXIT /B 0
)
:: The installer sets CUDA_PATH machine-wide, which the shell that ran setup never sees, so
:: this second check keeps a rerun in that same shell from installing on top of itself.
if exist "%CUDA_TOOLKIT_DIR%\include\cuda.h" (
  echo "CUDA Toolkit %CUDA_SHORT_VERSION% is already installed, skipping"
  EXIT /B 0
)
echo "Downloading CUDA Toolkit network installer"
curl -L https://developer.download.nvidia.com/compute/cuda/%CUDA_VERSION%/network_installers/cuda_%CUDA_VERSION%_windows_network.exe --output cuda_%CUDA_VERSION%_windows_network.exe
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading cuda_%CUDA_VERSION%_windows_network.exe"
  EXIT /B 1
)
echo "Installing the CUDA Toolkit cudart package (silent, accepts the NVIDIA license)"
start /wait "" cuda_%CUDA_VERSION%_windows_network.exe -s cudart_%CUDA_SHORT_VERSION%
IF %ERRORLEVEL% NEQ 0 (
  echo "Error installing the CUDA Toolkit, installer exit code %ERRORLEVEL%"
  echo "Set SKIP_CUDA=1 to build without MikanARKitVideo"
  EXIT /B 1
)
:: The sub-package name is the NVIDIA installer's, not ours, and it reports success for a
:: name it does not recognize, so confirm what the build actually needs is on disk.
if not exist "%CUDA_TOOLKIT_DIR%\include\cuda.h" (
  echo "CUDA Toolkit installed but include\cuda.h is missing from %CUDA_TOOLKIT_DIR%"
  EXIT /B 1
)
if not exist "%CUDA_TOOLKIT_DIR%\lib\x64\cuda.lib" (
  echo "CUDA Toolkit installed but lib\x64\cuda.lib is missing from %CUDA_TOOLKIT_DIR%"
  EXIT /B 1
)
echo "CUDA Toolkit installed - it sets CUDA_PATH machine-wide, so generate project files from a new shell"
EXIT /B 0

:failure
pause
EXIT /B 1