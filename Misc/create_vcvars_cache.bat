@echo off
setlocal

set cache_file=%1
if "%cache_file%" == "" (
	echo cache file not passed as argument >&2
	goto :EOF
)

set saved_path=%PATH%
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

for /f "tokens=*" %%p in ('where cl') do set CL_BIN=%%p
call set path_extra=%%PATH:%saved_path%=%%

rem write cache file ================================================

echo @echo off> %cache_file%
echo.>> %cache_file%
echo rem generated file>> %cache_file%
echo rem source it at startup for faster msvc setup>> %cache_file%
echo.>> %cache_file%

echo set path_extra=%path_extra%>> %cache_file%
echo set PATH=%%path_extra%%;%%PATH%%>> %cache_file%
echo set INCLUDE=%INCLUDE%>> %cache_file%
echo set LIB=%LIB%>> %cache_file%
echo set CL_BIN=%CL_BIN%>> %cache_file%

rem finished ========================================================

echo.
echo vcvars cache saved to "%cache_file%"