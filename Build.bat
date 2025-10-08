@echo off

IF NOT EXIST .\Build\ mkdir .\Build\
pushd .\Build\
cl -MT -nologo -Gm- -GR -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7 -Fmwin32_handmade.map ../../HandmadeHero/Source/win32_handmade.cpp /link -opt:ref user32.lib gdi32.lib winmm.lib
popd
