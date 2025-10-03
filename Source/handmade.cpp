#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
    }
}


internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;

    for (int Y = 0; Y < Buffer->Height; ++Y) 
    {
        uint32 *Pixel = (uint32 *)Row;

        for (int X = 0; X < Buffer->Width; ++X) 
        {
            // Pixel in Memory: RR GG BB XX
            // When we put into memory we get: 0xXXBBGGRR !
            // Little Endian Architecture!
            // Microsoft people looking at that didn't like it!
            // They wanted to see the memory with the RGB in the proper place
            // So they defined it backwards in the Memory of the machine
            // ACTUAL Pixel in Memory: BB GG RR XX
            // So all windows bitmaps have Blue First, Green then Red Bytes!

            uint8 Blue  = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void
GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, 
                    game_sound_output_buffer *SoundBuffer)
{
    local_persist int BlueOffset = 0;
    local_persist int GreenOffset = 0;
    local_persist int ToneHz = 256;

    game_controller_input *Input0 = &Input->Controllers[0];
    if(Input0->IsAnalog)
    {
        BlueOffset += (int)4.0f * (Input0->EndX);
        ToneHz = 256 + (int)(128.0f * (Input0->EndY));
    }
    else
    {
    }

    if (Input0->Down.EndedDown)
    {
        GreenOffset += 1;
    }

    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
    return;
}
