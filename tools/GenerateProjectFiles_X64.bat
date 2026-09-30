@echo on
setlocal

:: Shared body of the GenerateProjectFiles_X64_VS*.bat wrappers.
:: %1 is the CMake Visual Studio generator name, e.g. "Visual Studio 17 2022".
IF "%~1"=="" (
  echo "Usage: GenerateProjectFiles_X64.bat <Visual Studio generator name>"
  goto failure
)

:: The Visual Studio 18 2026 generator needs CMake 4.2 or newer. Visual Studio bundles a new
:: enough CMake but only puts it on PATH inside its Developer Command Prompt.
where cmake > nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
  echo "cmake was not found on PATH. Install CMake 4.2 or newer, or run this from a Visual Studio Developer Command Prompt."
  goto failure
)

:: This script lives in <repo>/tools, so the repo root is one level up.
for %%I in ("%~dp0..") do set REPO_ROOT_PATH=%%~fI
set DEPS_ROOT_PATH=%REPO_ROOT_PATH%\deps
set THIRDPARTY_ROOT_PATH=%REPO_ROOT_PATH%\thirdparty
set DIST_ROOT_PATH=%REPO_ROOT_PATH%\dist\Win64

IF NOT EXIST "%REPO_ROOT_PATH%\build" mkdir "%REPO_ROOT_PATH%\build"
pushd "%REPO_ROOT_PATH%\build"

echo "Rebuilding Mikan x64 Project files for %~1..."

cmake .. -G "%~1" -A x64 ^
-DCMAKE_INSTALL_PREFIX="%DIST_ROOT_PATH%" ^
-DCEF_ROOT="%DEPS_ROOT_PATH%/cef/cef_binary_145.0.27+g4ddda2e+chromium-145.0.7632.117_windows64" ^
-DOpenCV_DIR="%DEPS_ROOT_PATH%\opencv\build" ^
-DOPENVR_ROOT_DIR="%THIRDPARTY_ROOT_PATH%\openvr" ^
-DOPENVR_HEADERS_ROOT_DIR="%THIRDPARTY_ROOT_PATH%\openvr\include" ^
-DSDL2_LIBRARY="%DEPS_ROOT_PATH%\SDL2-2.30.10\lib\x64\sdl2.lib" ^
-DSDL2_INCLUDE_DIR="%DEPS_ROOT_PATH%\SDL2-2.30.10\include" ^
-DSDL2TTF_LIBRARY="%DEPS_ROOT_PATH%\SDL2_ttf-2.24.0\lib\x64\sdl2_ttf.lib" ^
-DSDL2TTF_INCLUDE_DIR="%DEPS_ROOT_PATH%\SDL2_ttf-2.24.0\include" ^
-DSDL2_IMAGE_LIBRARY="%DEPS_ROOT_PATH%\SDL2_image-2.8.8\lib\x64\sdl2_image.lib" ^
-DSDL2_IMAGE_INCLUDE_DIR="%DEPS_ROOT_PATH%\SDL2_image-2.8.8\include" ^
-DCMAKE_PREFIX_PATH="%DEPS_ROOT_PATH%\easy_profiler\lib\cmake\easy_profiler" ^
-DNUGET_PATH="%DEPS_ROOT_PATH%" ^
-DCMAKE_UNITY_BUILD=ON

IF %ERRORLEVEL% NEQ 0 (
  echo "Error generating Mikan 64-bit project files"
  popd
  goto failure
)

popd
EXIT /B 0

:failure
pause
EXIT /B 1
