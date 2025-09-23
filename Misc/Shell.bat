@echo off

set vcvars_cache_file=%~dp0\vcvars_cache.bat
if not exist %vcvars_cache_file% (
	call %~dp0\create_vcvars_cache.bat %vcvars_cache_file%
)
call %vcvars_cache_file%

doskey /exename=cmd.exe /macrofile=%home%\config\doskey.macrofile
clink inject

set vcvars_cache_file=set path=D:\GitProjects\HandmadeHero\Misc\;%path%

