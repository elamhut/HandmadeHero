@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib


IF NOT EXIST .\Build\ mkdir .\Build\
pushd .\Build\
cl %CommonCompilerFlags% ../../HandmadeHero/Source/handmade.cpp -Fmhandmade.map /LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ../../HandmadeHero/Source/win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
