@echo off
REM Build script for Notiman C++ implementation

echo Cleaning old build...
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist

echo Configuring with CMake...
cmake -B build -G "Visual Studio 18 2026"
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building Release configuration...
"%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" build\Notiman.slnx /p:Configuration=Release /v:minimal
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo Build successful!
echo.
echo Executables:
echo   CLI:  build\src\cli\Release\notiman.exe
echo   Host: build\src\host\Release\notiman-host.exe
echo   Proxy: build\src\proxy\Release\notiman-proxy.exe

echo f | xcopy /s /f /y build\src\cli\Release\notiman.exe dist\notiman.exe
echo f | xcopy /s /f /y build\src\host\Release\notiman-host.exe dist\notiman-host.exe
echo f | xcopy /s /f /y build\src\proxy\Release\notiman-proxy.exe dist\notiman-proxy.exe
