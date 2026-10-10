@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not defined WEBVIEW2_SDK set "WEBVIEW2_SDK=C:\WebView2\microsoft.web.webview2.1.0.4258.31\build\native"
set "OUT=screenask.exe"

 echo.
 echo ============================================
 echo   ScreenAsk - PNG glass UI + WebView2
 echo ============================================
 echo.

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc was not found. Open the MSYS2 UCRT64 shell and run this file again.
    goto :fail
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ was not found. Open the MSYS2 UCRT64 shell and run this file again.
    goto :fail
)
if not exist "%WEBVIEW2_SDK%\include\WebView2.h" (
    echo ERROR: WebView2.h was not found:
    echo   %WEBVIEW2_SDK%\include\WebView2.h
    echo Set WEBVIEW2_SDK to your WebView2 SDK native folder if it is installed elsewhere.
    goto :fail
)
for %%A in (bar_bg.png search.png camera.png settings.png button_active.png slider_track.png slider_knob.png divider.png) do (
    if not exist "assets\%%A" (
        echo ERROR: missing assets\%%A
        goto :fail
    )
)
if exist "%OUT%" del /q "%OUT%"
if exist "screenask_ui.o" del /q "screenask_ui.o"
if exist "screenask_host.o" del /q "screenask_host.o"

echo [1/4] compiling the actual PNG glass-bar UI module (screenask.c)...
gcc -std=c11 -O2 -Wall -Wextra -c screenask.c -o screenask_ui.o
if errorlevel 1 goto :fail

echo.
echo [2/4] compiling the existing ScreenAsk host and WebView2 integration (screenask.cpp)...
g++ -x c++ -std=gnu++17 -O2 -municode -mwindows -static-libgcc -static-libstdc++ ^
  -I"%WEBVIEW2_SDK%\include" ^
  -c screenask.cpp -o screenask_host.o
if errorlevel 1 goto :fail

echo.
echo [3/4] linking one ScreenAsk executable...
g++ -municode -mwindows -static-libgcc -static-libstdc++ -o "%OUT%" screenask_host.o screenask_ui.o ^
  -lole32 -loleaut32 -luuid -lshell32 -luser32 -lwinhttp -lcrypt32 ^
  -lcomctl32 -lgdi32 -lmsimg32 -lgdiplus -lshlwapi -lwinpthread
if errorlevel 1 goto :fail

if not exist "%WEBVIEW2_SDK%\x64\WebView2Loader.dll" (
    echo ERROR: WebView2Loader.dll was not found:
    echo   %WEBVIEW2_SDK%\x64\WebView2Loader.dll
    goto :fail
)
echo.
echo [4/4] copying the WebView2 runtime loader...
copy /Y "%WEBVIEW2_SDK%\x64\WebView2Loader.dll" "WebView2Loader.dll" >nul
if errorlevel 1 goto :fail
if not exist "%OUT%" goto :fail

rem Bundle the GCC POSIX-thread runtime required by this executable.
rem Prefer the active MSYS2 UCRT64 toolchain path; fall back to the standard install.
set "PTHREAD_DLL="
if defined MSYSTEM_PREFIX if exist "%MSYSTEM_PREFIX%\bin\libwinpthread-1.dll" set "PTHREAD_DLL=%MSYSTEM_PREFIX%\bin\libwinpthread-1.dll"
if not defined PTHREAD_DLL if exist "C:\msys64\ucrt64\bin\libwinpthread-1.dll" set "PTHREAD_DLL=C:\msys64\ucrt64\bin\libwinpthread-1.dll"
if defined PTHREAD_DLL (
    echo Copying libwinpthread-1.dll beside the executable...
    copy /Y "%PTHREAD_DLL%" "libwinpthread-1.dll" >nul
    if errorlevel 1 goto :fail
) else (
    echo WARNING: libwinpthread-1.dll was not found automatically.
    echo Copy it from your MSYS2 UCRT64 bin folder beside screenask.exe.
)

echo.
echo ============================================
echo BUILD COMPLETE
echo ============================================
echo Output: %~dp0%OUT%
echo Keep assets\, WebView2Loader.dll, and libwinpthread-1.dll beside screenask.exe.
echo.
pause
exit /b 0

:fail
echo.
echo ============================================
echo BUILD FAILED - see the error above
echo ============================================
echo No successful build is being claimed unless this script reaches BUILD COMPLETE.
echo.
pause
exit /b 1
