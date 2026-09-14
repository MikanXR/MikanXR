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

:: Download pre-compiled Refureku libraries
echo "Downloading Refureku..."
curl -L https://github.com/MikanXR/Refureku/releases/download/v2.2.2/rfk_v2.2.1_windows.7z --output rfk_v2.2.1_windows.7z
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading rfk_v2.2.1_windows.7z"
  goto failure
)
%UNZIP_EXE% e rfk_v2.2.1_windows.7z -y -r -spf -orfk
IF %ERRORLEVEL% NEQ 0 (
  echo "Error unzipping rfk_v2.2.1_windows.7z"
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
echo "Downloading gstreamer-runtime installer"
curl -L https://gstreamer.freedesktop.org/data/pkg/windows/1.26.10/mingw/gstreamer-1.0-mingw-x86_64-1.26.10.msi --output gstreamer-1.0-mingw-x86_64-1.26.10.msi
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading gstreamer-1.0-mingw-x86_64-1.26.10.msi"
  EXIT /B 1
)
msiexec /i gstreamer-1.0-mingw-x86_64-1.26.10.msi /qb
IF %ERRORLEVEL% NEQ 0 (
  echo "Error installing gstreamer-runtime installer"
  EXIT /B 1
)

echo "Downloading gstreamer-devel installer"
curl -L https://gstreamer.freedesktop.org/data/pkg/windows/1.26.10/mingw/gstreamer-1.0-devel-mingw-x86_64-1.26.10.msi --output gstreamer-1.0-devel-mingw-x86_64-1.26.10.msi
IF %ERRORLEVEL% NEQ 0 (
  echo "Error downloading gstreamer-1.0-devel-mingw-x86_64-1.26.10.msi"
  EXIT /B 1
)
msiexec /i gstreamer-1.0-devel-mingw-x86_64-1.26.10.msi /qb
IF %ERRORLEVEL% NEQ 0 (
  echo "Error installing gstreamer-devel installer"
  EXIT /B 1
)
EXIT /B 0

:failure
pause
EXIT /B 1