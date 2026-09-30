@echo off
:: Installs the GStreamer runtime and devel MSIs system-wide. The MSIs register
:: GSTREAMER_1_0_ROOT_MINGW_X86_64 machine-wide, which cmake/FindGStreamer.cmake reads.
:: Called by tools\SetupDevEnvironment.bat, and on its own by the release workflow.
::
:: Usage: tools\InstallGStreamer.bat

setlocal EnableExtensions EnableDelayedExpansion

set GSTREAMER_VERSION=1.26.10

:: Downloads and install logs stay out of deps\, which InitialSetup_x64.bat owns and wipes.
:: The short path has no spaces, which keeps the elevated msiexec command line unquoted.
set "GST_DOWNLOAD_DIR=%TEMP%\MikanXR-gstreamer"
if not exist "%GST_DOWNLOAD_DIR%" mkdir "%GST_DOWNLOAD_DIR%"
for %%F in ("%GST_DOWNLOAD_DIR%") do set "GST_DOWNLOAD_DIR=%%~sF"

:: The MSIs install per machine, which needs elevation
set "IS_ELEVATED="
net session > nul 2>&1
if %errorlevel% equ 0 set "IS_ELEVATED=1"

pushd "%GST_DOWNLOAD_DIR%"
call :install_gstreamer_msi "GStreamer 1.0 (MinGW x86_64)" gstreamer-runtime gstreamer-1.0-mingw-x86_64-%GSTREAMER_VERSION%.msi
if errorlevel 1 goto failure
call :install_gstreamer_msi "GStreamer 1.0 (Development Files) (MinGW x86_64)" gstreamer-devel gstreamer-1.0-devel-mingw-x86_64-%GSTREAMER_VERSION%.msi
if errorlevel 1 goto failure
popd
exit /b 0

:failure
popd
exit /b 1

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
  echo %~2 %GSTREAMER_VERSION%: already installed
  exit /b 0
)
echo %~2 %GSTREAMER_VERSION%: downloading...
curl -L https://gstreamer.freedesktop.org/data/pkg/windows/%GSTREAMER_VERSION%/mingw/%~3 --output %~3
if %errorlevel% neq 0 (
  echo Error downloading %~3
  exit /b 1
)
echo %~2 %GSTREAMER_VERSION%: installing ^(silent, takes a minute or two^)...
set "MSI_ARGS=/i %GST_DOWNLOAD_DIR%\%~3 /qn /norestart /l*v %GST_DOWNLOAD_DIR%\%~2-install.log"
if defined IS_ELEVATED (
  start /wait "" msiexec %MSI_ARGS%
) else (
  rem One UAC prompt per MSI. A declined prompt is reported as msiexec's own
  rem user-cancel code, 1602.
  powershell -NoProfile -Command "try { $p= Start-Process msiexec -ArgumentList '%MSI_ARGS%' -Verb RunAs -Wait -PassThru; exit $p.ExitCode } catch { exit 1602 }"
)
set "MSI_RESULT=%errorlevel%"
if not "%MSI_RESULT%"=="0" (
  echo Error installing %~2, msiexec exit code %MSI_RESULT%
  if exist %~2-install.log findstr /i "error return value" %~2-install.log
  exit /b 1
)
exit /b 0

:: Sets INSTALLED_VERSION to the version the product named %1 is installed at, or to
:: nothing when no such product is installed.
:query_installed_version
set "INSTALLED_VERSION="
set "PRODUCT_KEY="
for /f "delims=" %%K in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" /s /f "%~1" /d /e 2^>nul ^| findstr /b /c:"HKEY_"') do if not defined PRODUCT_KEY set "PRODUCT_KEY=%%K"
if not defined PRODUCT_KEY exit /b 0
for /f "tokens=2,*" %%A in ('reg query "%PRODUCT_KEY%" /v DisplayVersion 2^>nul ^| findstr /c:"DisplayVersion"') do set "INSTALLED_VERSION=%%B"
exit /b 0
