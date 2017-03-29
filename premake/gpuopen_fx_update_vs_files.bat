@echo off

if %1.==. goto usage

set arg1=%1

:: strip off relative path
if "%arg1:~0,3%" == "..\" set arg1=%arg1:~3%

set startdir=%cd%
cd "%~dp0"

echo --- amd_lib ---
cd ..\amd_lib\shared\d3d11\premake
call :createvsfilesforsharedcode
call :createvsfileswithminimaldependencies

echo --- amd_sdk ---
cd ..\..\..\..\framework\d3d11\amd_sdk\premake
call :createvsfilesforsharedcode
call :createvsfileswithminimaldependencies

echo --- dxut core ---
cd ..\..\dxut\Core
call :createvsfilesforsharedcode

echo --- dxut optional ---
cd ..\Optional
call :createvsfilesforsharedcode
cd ..\..\..\..\
:: we don't keep solution files for amd_lib, amd_sdk, or dxut
call :cleanslnfiles

echo --- %arg1% ---
cd %arg1%\premake
call :createvsfiles
cd ..\..\

:: sample, capture_viewer, etc.
for /f %%a in ('dir /a:d /b %arg1%_* 2^>nul') do call :createvsfilesforsamples %%a

cd "%startdir%"

goto :EOF

::--------------------------
:: SUBROUTINES
::--------------------------

:: sample, capture_viewer, etc.
:createvsfilesforsamples
if exist %1\premake (
    echo --- %1 ---
    cd %1\premake
    call :createvsfiles
    cd ..\..\
)
goto :EOF

:: run premake for vs2013 and vs2015
:createvsfiles
..\..\premake\premake5.exe vs2013
..\..\premake\premake5.exe vs2015
goto :EOF

:: run premake for vs2013 and vs2015
:createvsfilesforsharedcode
..\..\..\..\premake\premake5.exe vs2013
..\..\..\..\premake\premake5.exe vs2015
goto :EOF

:: run premake for vs2013 and vs2015
:createvsfileswithminimaldependencies
..\..\..\..\premake\premake5.exe --file=premake5_minimal.lua vs2013
..\..\..\..\premake\premake5.exe --file=premake5_minimal.lua vs2015
goto :EOF

:: delete unnecessary sln files
:cleanslnfiles
del /f /q amd_lib\shared\d3d11\build\AMD_LIB_2013.sln
del /f /q amd_lib\shared\d3d11\build\AMD_LIB_2015.sln

del /f /q amd_lib\shared\d3d11\build\AMD_LIB_Minimal_2013.sln
del /f /q amd_lib\shared\d3d11\build\AMD_LIB_Minimal_2015.sln

del /f /q framework\d3d11\amd_sdk\build\AMD_SDK_2013.sln
del /f /q framework\d3d11\amd_sdk\build\AMD_SDK_2015.sln

del /f /q framework\d3d11\amd_sdk\build\AMD_SDK_Minimal_2013.sln
del /f /q framework\d3d11\amd_sdk\build\AMD_SDK_Minimal_2015.sln

del /f /q framework\d3d11\dxut\Core\DXUT_2013.sln
del /f /q framework\d3d11\dxut\Core\DXUT_2015.sln

del /f /q framework\d3d11\dxut\Optional\DXUTOpt_2013.sln
del /f /q framework\d3d11\dxut\Optional\DXUTOpt_2015.sln
goto :EOF

::--------------------------
:: usage should be last
::--------------------------

:usage
echo   usage: %0 library_dir_name
echo      or: %0 ..\library_dir_name
echo example: %0 AMD_AOFX
echo      or: %0 ..\AMD_AOFX
