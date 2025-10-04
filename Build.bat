@echo off

IF NOT EXIST .\Build\ mkdir .\Build\
pushd .\Build\
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi ../../HandmadeHero/Source/win32_handmade.cpp user32.lib gdi32.lib 
popd
