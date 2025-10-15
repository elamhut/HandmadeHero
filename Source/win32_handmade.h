#pragma once
#include "definitions.h"
#include "handmade.h"

struct win32_offscreen_buffer
{
    // NOTE: Pixels are always 32-bit wide
    // Memory Order is BB GG RR xx
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch; // This is the quantity of Bytes contained in the Width of the screen (Width * BytesPerPixel).
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    DWORD SafetyBytes;
    real32 tSine;
    int LatencySampleCount;
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
    DWORD ExpectedFlipPlayCursor;
};

struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

    bool32 IsValid;
};

struct win32_state
{
    uint64 TotalSize;
    void *GameMemoryBlock;

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;
};
