@echo off
setlocal enabledelayedexpansion

:: ─────────────────────────────────────────────────────────
:: build.bat – Auto-build for SessionAppFramework (Windows)
:: ─────────────────────────────────────────────────────────

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%Build
set LIBSESSION_DIR=%SCRIPT_DIR%libsession-util
set LIBSESSION_BUILD=%LIBSESSION_DIR%\Build

echo [1/3] Building libsession-util local dependencies...

if not exist "%LIBSESSION_BUILD%\src\session-util.lib" (
    if not exist "%LIBSESSION_BUILD%\src\Release\session-util.lib" (
        echo    ^> Patching libsession-util/CMakeLists.txt...
        :: Use a more robust powershell call
        powershell -NoProfile -ExecutionPolicy Bypass -Command "$path = '%LIBSESSION_DIR%\CMakeLists.txt'; (Get-Content $path) -replace 'target_compile_options\(libsession-util_src', '# target_compile_options(libsession-util_src' | Set-Content $path"

        echo    ^> Configuring libsession-util...
        if not exist "%LIBSESSION_BUILD%" mkdir "%LIBSESSION_BUILD%"
        
        cmake -S "%LIBSESSION_DIR%" -B "%LIBSESSION_BUILD%" ^
              -D STATIC_BUNDLE=ON ^
              -D BUILD_STATIC_DEPS=ON ^
              -D WITH_TESTS=OFF
        
        echo    ^> Compiling libsession-util...
        cmake --build "%LIBSESSION_BUILD%" --config Release --parallel
    ) else (
        echo    ^> libsession-util already built (Release).
    )
) else (
    echo    ^> libsession-util already built.
)

echo [2/3] Configuring SessionAppFramework...

:: Find internal headers required by libsession-util
set OXENC_INC=%LIBSESSION_DIR%\external\oxen-libquic\external\oxen-encoding
set FMT_INC=%LIBSESSION_DIR%\external\oxen-logging\fmt\include
set SPDLOG_INC=%LIBSESSION_DIR%\external\oxen-logging\spdlog\include
set PROTO_INC=%LIBSESSION_DIR%\proto

:: Collect all .lib files from libsession-util build
powershell -NoProfile -ExecutionPolicy Bypass -Command "$libs = Get-ChildItem -Path '%LIBSESSION_BUILD%' -Filter *.lib -Recurse | %% { $_.FullName }; $libs -join ';'" > libs_temp.txt
set /p LIBSESSION_LIBS=<libs_temp.txt
del libs_temp.txt

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
      -D CMAKE_BUILD_TYPE="Release" ^
      -D SAF_BUILD_EXAMPLES=ON ^
      -D SAF_ENABLE_ONION=ON ^
      -D LIBSESSION_INCLUDE_DIRS="%LIBSESSION_DIR%\include;%OXENC_INC%;%FMT_INC%;%SPDLOG_INC%;%PROTO_INC%" ^
      -D LIBSESSION_LIBRARIES="%LIBSESSION_LIBS%"

echo [3/3] Compiling SessionAppFramework...
cmake --build "%BUILD_DIR%" --config Release --parallel

echo.
echo ✅  Build complete!

if not defined GITHUB_ACTIONS (
    pause
)
