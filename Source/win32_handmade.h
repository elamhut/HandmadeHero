#pragma once

struct win32_offscreen_buffer
{
    // NOTE: Pixels are always 32-bit wide
    // Memory Order is BB GG RR xx
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};


