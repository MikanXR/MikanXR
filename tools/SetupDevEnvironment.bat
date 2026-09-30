@echo off
:: Installs the machine-level tools MikanXR's build and developer tooling need, skipping
:: anything already present. It builds nothing, and it leaves the prebuilt dependencies
:: under deps/ to InitialSetup_x64.bat, which runs after it.
::
::   1. Git, CMake, Node.js (TypeScript bindings), and Python 3.12 through winget
::   2. Inno Setup (installer packaging) through winget, only with -innosetup
::   3. The Visual Studio components the build needs, added to every 2022 or newer
::      instance missing one. Visual Studio itself is never installed: with no 2022 or
::      newer instance the script stops and says what to install.
::   4. A repo-local Python virtual environment at .venv holding tools\requirements.txt
::
:: Usage: tools\SetupDevEnvironment.bat [-innosetup]

setlocal EnableExtensions EnableDelayedExpansion

:: This script lives in <repo>\tools, so the repo root is one level up
for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"

set "WANT_INNOSETUP="
for %%A in (%*) do (
  if /i "%%~A"=="-innosetup" set "WANT_INNOSETUP=1"
)

:: -- winget ------------------------------------------------------------------
where winget > nul 2>&1
if errorlevel 1 (
  echo ERROR: winget ^(App Installer^) was not found.
  echo Install "App Installer" from the Microsoft Store, then rerun this script.
  exit /b 1
)

:: -- Tools -------------------------------------------------------------------
echo.
echo === Tools ===
call :winget_install Git.Git "Git"
:: CMake on PATH lets the generate scripts run outside a Developer Command Prompt, and
:: the Visual Studio 18 2026 generator needs CMake 4.2 or newer
call :winget_install Kitware.CMake "CMake"
call :winget_install OpenJS.NodeJS.LTS "Node.js LTS"
:: winget names Python packages per minor version, so this pins the minor version
call :winget_install Python.Python.3.12 "Python 3.12"
if defined WANT_INNOSETUP (
  call :winget_install JRSoftware.InnoSetup "Inno Setup"
) else (
  echo Inno Setup: skipped ^(pass -innosetup to install it for installer packaging^)
)

:: -- Visual Studio -----------------------------------------------------------
echo.
echo === Visual Studio ===
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSSETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe"
set "VS_INSTANCE_COUNT=0"
if exist "%VSWHERE%" (
  rem The version range's comma and parenthesis are escaped rather than quoted: a second
  rem quoted span in the command would make cmd strip the quotes around the vswhere path
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -products * -version [17.0^,^) -property installationPath`) do (
    set /a VS_INSTANCE_COUNT+=1
    call :check_vs_instance "%%I"
  )
)
if !VS_INSTANCE_COUNT! EQU 0 (
  echo ERROR: No Visual Studio 2022 or newer was found.
  echo Install Visual Studio 2022 or 2026 with these workloads, then rerun this script:
  echo   - Desktop development with C++
  echo   - .NET desktop development
  exit /b 1
)

:: -- Python virtual environment ----------------------------------------------
echo.
echo === Python virtual environment ===
set "VENV_PYTHON=%REPO_ROOT%\.venv\Scripts\python.exe"
if exist "%VENV_PYTHON%" (
  echo .venv already exists.
) else (
  call :find_python312
  if not defined PYTHON312 (
    echo Python 3.12 is not on this shell's PATH yet. Open a new terminal and rerun this script.
    exit /b 1
  )
  echo Creating .venv with Python 3.12...
  !PYTHON312! -m venv "%REPO_ROOT%\.venv"
  if errorlevel 1 (
    echo ERROR: Could not create .venv.
    exit /b 1
  )
)
"%VENV_PYTHON%" -m pip install --disable-pip-version-check -q -r "%REPO_ROOT%\tools\requirements.txt"
if errorlevel 1 (
  echo ERROR: Could not install tools\requirements.txt into .venv.
  exit /b 1
)
echo Python packages from tools\requirements.txt are installed in .venv.

:: -- Next steps --------------------------------------------------------------
echo.
echo === Done ===
echo Open a new terminal so newly installed tools are on PATH, then from the repo root:
echo   InitialSetup_x64.bat
echo   GenerateProjectFiles_X64_VS2022.bat  or  GenerateProjectFiles_X64_VS2026.bat
echo Run the Python tools with .venv\Scripts\python, or activate the environment first
echo with .venv\Scripts\activate.
endlocal
exit /b 0

:: Installs the winget package %1 (display name %2) unless winget already lists it.
:: winget fails with negative exit codes, which "if errorlevel 1" does not catch, so
:: the codes are compared exactly.
:winget_install
winget list -e --id %~1 --accept-source-agreements > nul 2>&1
if %errorlevel% equ 0 (
  echo %~2: already installed
  exit /b 0
)
echo %~2: installing...
winget install -e --id %~1 --accept-package-agreements --accept-source-agreements
if %errorlevel% neq 0 echo   winget returned an error installing %~2. Rerun, or install it by hand.
exit /b 0

:: Adds the components the build needs to the Visual Studio instance at %1 when it lacks
:: any: the C++ toolset, and the C# compiler and .NET Framework 4.7.2 targeting pack
:: that the C# bindings and test app build against
:check_vs_instance
set "VS_REQUIRED=Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.Net.Component.4.7.2.TargetingPack Microsoft.VisualStudio.Component.Roslyn.Compiler"
"%VSWHERE%" -path "%~1" -requires %VS_REQUIRED% -property installationPath | findstr /r "." > nul
if not errorlevel 1 (
  echo %~1: has the required components
  exit /b 0
)
:: Build Tools instances name their workloads differently from the IDE editions
set "VS_PRODUCT="
for /f "usebackq delims=" %%P in (`call "%VSWHERE%" -path "%~1" -property productId`) do set "VS_PRODUCT=%%P"
if /i "%VS_PRODUCT%"=="Microsoft.VisualStudio.Product.BuildTools" (
  set "VS_WORKLOADS=--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Workload.ManagedDesktopBuildTools"
) else (
  set "VS_WORKLOADS=--add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Workload.ManagedDesktop"
)
echo %~1: adding the C++ and .NET desktop workloads ^(accept the elevation prompt^)...
"%VSSETUP%" modify --installPath "%~1" %VS_WORKLOADS% --add Microsoft.Net.Component.4.7.2.TargetingPack --includeRecommended --passive --norestart --wait
"%VSWHERE%" -path "%~1" -requires %VS_REQUIRED% -property installationPath | findstr /r "." > nul
if errorlevel 1 (
  echo   The components are still missing. Add the workloads above with the Visual Studio Installer.
) else (
  echo   Components added.
)
exit /b 0

:: Sets PYTHON312 to a command line that runs Python 3.12, preferring the launcher. A
:: Python installed moments ago is on the user PATH but not yet on this shell's, so the
:: launcher's and the interpreter's install locations are checked as well. The plain
:: "python" command is never used, since it can resolve to the Microsoft Store alias.
:find_python312
set "PYTHON312="
set "PY_LAUNCHER="
for /f "delims=" %%L in ('where py 2^>nul') do if not defined PY_LAUNCHER set "PY_LAUNCHER=%%L"
if not defined PY_LAUNCHER if exist "%LOCALAPPDATA%\Programs\Python\Launcher\py.exe" set "PY_LAUNCHER=%LOCALAPPDATA%\Programs\Python\Launcher\py.exe"
if not defined PY_LAUNCHER if exist "%SystemRoot%\py.exe" set "PY_LAUNCHER=%SystemRoot%\py.exe"
if defined PY_LAUNCHER (
  set PYTHON312="!PY_LAUNCHER!" -3.12
  exit /b 0
)
if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" set PYTHON312="%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
if not defined PYTHON312 if exist "%ProgramFiles%\Python312\python.exe" set PYTHON312="%ProgramFiles%\Python312\python.exe"
exit /b 0
