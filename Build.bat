@echo off

set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

REM Optimization Switches /O2 /Oi /fp:fast

IF NOT EXIST .\Build\ mkdir .\Build\
pushd .\Build\
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% ../../HandmadeHero/Source/handmade.cpp -Fmhandmade.map -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ../../HandmadeHero/Source/win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
