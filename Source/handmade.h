#pragma once
#include "definitions.h"

/*
 * NOTE:
 * HANDMADE_SLOW:
 * 0 - No slow code allowed
 * 1 - Slow code allowed
 * HANDMADE_INTERNAL:
 * 0 - Build for public release
 * 1 - Developer build
 */


#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// NOTE: Services that the Platform layer provides to the game:
#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    uint32 ContentsSize;
    void *Contents;
};

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *BitmapMemory);
internal bool32 *DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
#endif

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    // We're checking if the value (Current use is for Filesize) is equal or less than an uint32
    // This is because we could be reading a file bigger than 4gb
    // But we want an arbitrary boundary of 4gb filesize, we crash if not.

    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return(Result);
}

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

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Button[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[4];
};


struct game_memory
{
    bool32 IsInitialized;

    uint64 PermanentStorageSize;
    void *PermanentStorage; //NOTE: REQUIRED to be cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; //NOTE: REQUIRED to be cleared to zero at startup
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

internal void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);

struct game_state
{
    int ToneHz;
    int GreenOffset;
    int BlueOffset;
};
