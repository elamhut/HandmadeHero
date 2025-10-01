#if !defined(HANDMADE_H)


struct game_offscreen_buffer
{
    // NOTE: Pixels are always 32-bit wide
    // Memory Order is BB GG RR xx
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, 
                                  int BlueOffset, 
                                  int GreenOffset, 
                                  game_sound_output_buffer *SoundBuffer,
                                  int ToneHz);

#define HANDMADE_H
#endif
