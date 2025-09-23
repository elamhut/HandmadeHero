@echo off

mkdir .\Build\
pushd .\Build\
cl -Zi ../../HandmadeHero/Source/win32_handmade.cpp user32.lib
popd
