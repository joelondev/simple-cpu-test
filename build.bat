@echo off
setlocal

set "SRC=src\main.c src\func.c src\workloads.c"
set "OUT=partumtestkit.exe"
set "OBJDIR=build"

if not exist "%OBJDIR%" mkdir "%OBJDIR%"

where cl >nul 2>nul
if %errorlevel%==0 (
    echo Building with MSVC...
    cl /nologo /O2 /W3 /Fo:%OBJDIR%\ /Fe:%OUT% %SRC%
    if errorlevel 1 goto :fail
    goto :built
)

where clang-cl >nul 2>nul
if %errorlevel%==0 (
    echo Building with clang-cl...
    clang-cl /nologo /O2 /W3 /Fo:%OBJDIR%\ /Fe:%OUT% %SRC%
    if errorlevel 1 goto :fail
    goto :built
)

echo.
echo No C compiler found.
echo Open an "x64 Native Tools Command Prompt for VS" or install LLVM/clang-cl,
echo then run build.bat again.
exit /b 1

:built
if not exist "%OUT%" (
    echo Build failed: %OUT% was not created.
    exit /b 1
)

echo Built %OUT%
echo.

if /I "%~1"=="test" (
    echo Running quick test...
    "%OUT%" --quick -r 1
    exit /b %errorlevel%
)

echo Run "%OUT% --quick" for a short test or "%OUT%" for the full benchmark.
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1
