#include "definitions.h"
#include "handmade.h"

#include <cstddef>
#include <exception>
#include <system_error>
#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

//NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
                {
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            Result = (BytesWritten == MemorySize);
        }
        else
        {
        }

        CloseHandle(FileHandle);
    }

    return 0;
}

internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest)
{
    for (int Index = 0; Index < SourceACount; Index++) {
        *Dest++ = *SourceA++;
    }

    for (int Index = 0; Index < SourceBCount; Index++) {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

internal int
StringLength(char *String)
{
    int Count = 0;
    while(*String++)
    {
        ++Count;
    }

    return Count;
}

internal void
Win32BuildFilenamePath(win32_state *State, char *Filename, int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFilenameSlash - State->EXEFilename, State->EXEFilename,
               StringLength(Filename), Filename,
               DestCount, Dest);
}

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {};

    WIN32_FIND_DATA FindData;
    if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &FindData))
    {
        LastWriteTime = FindData.ftLastWriteTime;
    }

    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
    win32_game_code Result = {};

    Result.DLLLastWriteTime = Win32GetLastWriteTime(TempDLLName);

    CopyFile(SourceDLLName, TempDLLName, FALSE);

    Result.GameCodeDLL = LoadLibraryA("handmade_temp.dll");
    if (Result.GameCodeDLL)
    {
        Result.UpdateAndRender =
            (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = 
            (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

        Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
    } 

    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}


internal void
Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {XInputGetState = XInputGetStateStub;}

        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {

            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize  = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error))
                    {
                        OutputDebugStringA("Primary Buffer Was Set!\n");
                    }
                    else
                    {
                    }
                }
                else
                {
                }
            }
            else
            {
            }

            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize  = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary Buffer Was Set!\n");
            }
        }
        else
        {
        }
    }
}

internal win32_window_dimension
GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width  = ClientRect.right  - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}


internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory) 
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

    // NOTE: When the biHeight is negative, this is the clue to Windows to
    // treat the bitmap as top-down, not bottom-up, meaning that the first three
    // bytes of the image are the color for the top left pixel in the bitmap,
    // not the bottom left!
    Buffer->Info.bmiHeader.biSize        = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth       = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight      = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes      = 1;
    Buffer->Info.bmiHeader.biBitCount    = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width * Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    //TODO: Clear to Black
    Buffer->Pitch = Width*BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext, 
                           int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, Buffer->Width, Buffer->Height,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory, 
                  &Buffer->Info, 
                  DIB_RGB_COLORS, 
                  SRCCOPY);
}



internal LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message) {
        case WM_SIZE:
            {
            } break;

        case WM_CLOSE:
            {
                // TODO: Handle this with a message to the user?
                GlobalRunning = false;
            } break;

        case WM_DESTROY:
            {
                // TODO: Handle this as an error?
                GlobalRunning = false;
            } break;

        case WM_ACTIVATEAPP:
            {
#if 0
                if (WParam == TRUE)
                {
                    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
                }
                else
                {
                    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
                }
#endif
            } break;


        case WM_PAINT:
            {
                PAINTSTRUCT Paint;
                HDC DeviceContext = BeginPaint(Window, &Paint);
                win32_window_dimension Dimensions = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimensions.Width, Dimensions.Height);

                EndPaint(Window, &Paint);
            }
        default:
            {
                Result = DefWindowProc(Window, Message, WParam, LParam);
            } break;
    }

    return Result;
}

internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0)))
    {
        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample = 0;
        }

        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, 
                     DWORD ByteToLock, DWORD BytesToWrite, 
                     game_sound_output_buffer *SoundBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
    {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SoundBuffer->Samples;

        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32GetInputFileLocation(win32_state *State, int SlotIndex, int DestCount, char *Dest)
{
    Assert(SlotIndex == 1);
    Win32BuildFilenamePath(State, "recorded_input.hm", DestCount, Dest);
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    State->InputRecordingIndex = InputRecordingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputRecordingIndex, sizeof(Filename), Filename);

    State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    DWORD BytesToWrite = (DWORD)State->TotalSize;
    Assert(State->TotalSize == BytesToWrite);
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, State->GameMemoryBlock, BytesToWrite, &BytesWritten, 0);
}

internal void
Win32EndRecordingInput(win32_state *State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state *State, int InputPlayingIndex)
{
    State->InputPlayingIndex = InputPlayingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputPlayingIndex, sizeof(Filename), Filename);

    State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    DWORD BytesToRead = (DWORD)State->TotalSize;
    Assert(State->TotalSize == BytesToRead);
    DWORD BytesRead;
    ReadFile(State->PlaybackHandle, State->GameMemoryBlock, BytesToRead, &BytesRead, 0);
}

internal void
Win32EndInputPlayback(win32_state *State)
{
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlaybackInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesRead = 0;
    if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if (BytesRead == 0)
        {
            // NOTE: We've hit the end of the stream, go back to the beginning
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayback(State);
            Win32BeginInputPlayback(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}


internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, 
                                game_button_state *OldState,
                                DWORD ButtonBit, 
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadzoneThreshold)
{
    // NOTE:
    // We receive from the platform a signed int16 (SHORT) representing 
    // how much the user has moved the stick input.
    // We divide this value from the total int16 to get a normalized value from 0 to 1
    // We do this so we can later use this more easily on movement system or whatever
    real32 Result = 0;

    if (Value < -DeadzoneThreshold)
    { 
        // NOTE: What Casey is doing here he's making sure the value of the normalized stick
        // Starts at 0.0 and goes to 1.0 at the exact moment the stick leaves the 
        // Threshold zone. Otherwise the stick would start at around 0.25 because
        // that's more or less where the Deadzone ends and we begin pooling the stick
        Result = (real32)((Value + DeadzoneThreshold) / (32768.0f - DeadzoneThreshold));
        // Remember that negative int16 is up to 32768
    }
    else if (Value > DeadzoneThreshold)
    {
        Result = (real32)((Value - DeadzoneThreshold) / (32767.0f - DeadzoneThreshold));
        // And positive int16 goes to 32767 because 0 is included
    }

    char OutString[256];
    _snprintf_s(OutString, sizeof(OutString), "%.02f Stick Value\n", Result);
    OutputDebugStringA(OutString);
    return Result;
}

internal void
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
    MSG Message;

    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {

        switch (Message.message)
        {
            case WM_QUIT:
                {
                    GlobalRunning = false;
                } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
                {
                    uint32 VKCode = (uint32)Message.wParam;
                    bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                    bool32 IsDown  = ((Message.lParam & (1 << 31)) == 0);

                    if (WasDown != IsDown)
                    {
                        if (VKCode == 'W')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                        }
                        else if (VKCode == 'A')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                        }
                        else if (VKCode == 'S')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                        }
                        else if (VKCode == 'D')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                        }
                        else if (VKCode == 'Q')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        }
                        else if (VKCode == 'E')
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        }
                        else if (VKCode == VK_UP)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                        }
                        else if (VKCode == VK_DOWN)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                        }
                        else if (VKCode == VK_LEFT)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                        }
                        else if (VKCode == VK_RIGHT)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                        }
                        else if (VKCode == VK_ESCAPE)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                        }
                        else if (VKCode == VK_SPACE)
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                        }
#if HANDMADE_INTERNAL
                        else if (VKCode == 'P')
                        {
                            if (IsDown)
                            {
                                GlobalPause = !GlobalPause;
                            }
                        }
                        else if (VKCode == 'L')
                        {
                            if (IsDown)
                            {
                                if (State->InputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(State, 1);
                                }
                                else
                                {
                                    Win32EndRecordingInput(State);
                                    Win32BeginInputPlayback(State, 1);
                                }
                            }
                        }
#endif
                    }

                    bool32 AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
                    if ((VKCode == VK_F4) && AltKeyWasDown)
                   {
                        GlobalRunning = false;
                   }
                } break;

            default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
        }
    }
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);

    return Result;
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)GlobalPerfCountFrequency);

    return Result;
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer, int X, int Top, int Bottom, uint32 Color)
{
    // WARN:
    // I don't understand the role of Memory here when constructing the *Pixel
    // We're constructing the Pixel so we pass the X * BytesPerPixel this way the pixel's location
    // will be shifted horizontally to the correct place
    // We do the same with the Top, this means the pixel will start
    // with the "Top" variable offset in the Vertical position
    // Do we need memory here so we know the total bytes of our Render Buffer?
    // Why do we need to add it to the others?
    // Is it because we'll be drawing it on top of the current buffer?
    // This seems to be the case since this function is called after our main game render
    // But it's a pointer to the Pixel to render, not to a buffer...

    if (Top <= 0)
    {
        Top = 0;
    }

    if (Bottom > Backbuffer->Height)
    {
        Bottom = Backbuffer->Height;
    }

    if ((X >= 0) && (X < Backbuffer->Width))
    {
        uint8 *Pixel = ((uint8 *)Backbuffer->Memory + 
                        X * Backbuffer->BytesPerPixel +
                        Top * Backbuffer->Pitch);

        for (int Y = Top; Y < Bottom; ++Y)
        {
            *(uint32 *)Pixel = Color;
            Pixel += Backbuffer->Pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer, 
                           win32_sound_output *SoundOutput, 
                           real32 C, int PadX, int Top, int Bottom,
                           DWORD Value, uint32 Color)
{
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;

    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer, 
                      int MarkerCount, win32_debug_time_marker *Markers,
                      int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    real32 C = (real32)(Backbuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;

    for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor  < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation    < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount   < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor    < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor   < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if (MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor,  PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }

        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor,  PlayColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample,  PlayWindowColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}

internal void
Win32GetEXEFilename(win32_state *State)
{
    DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFilename, sizeof(State->EXEFilename));
    State->OnePastLastEXEFilenameSlash = State->EXEFilename;
    for (char *Scan = State->EXEFilename; *Scan; Scan++)
    {
        if (*Scan == '\\')
            State->OnePastLastEXEFilenameSlash = Scan + 1;
    }
}


int CALLBACK 
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR     CommandLine,
        int       ShowCode)
{
    win32_state Win32State = {};

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFilename(&Win32State);

    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildFilenamePath(&Win32State, "handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildFilenamePath(&Win32State, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    Win32LoadXInput();

    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback; 
    // Below is the actual proces that windows needs to know where to search
    // for the pointer of the function above.
    // A Pointer is unique to a process space, so without this it's impossible
    // for windows to know where the function above is in memory
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)

    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0,// WS_EX_TOPMOST|WS_EX_LAYERED, 
                                      WindowClass.lpszClassName, 
                                      "Handmade", 
                                      WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
                                      CW_USEDEFAULT, 
                                      CW_USEDEFAULT, 
                                      CW_USEDEFAULT, 
                                      CW_USEDEFAULT, 
                                      0, 
                                      0, 
                                      Instance, 
                                      0);
        if (Window) 
        {
            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / GameUpdateHz);
            SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample / GameUpdateHz) / 3;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

#if 0
            // NOTE: This tests the update frequency of Play/Write Cursors

            while (GlobalRunning)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                        "PC:%u WC:%u\n", PlayCursor, WriteCursor);

                // OutputDebugStringA(TextBuffer);
            }

#endif
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;


            // We'll allocate only 1 Memory Address then 
            // move the pointer of the transient storage
            // to where it's supposed to be
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                                      MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

            // NOTE: Here's the Startup of our Actual Engine
            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {

                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {};

                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                bool32 SoundIsValid = false;

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

                // rdtsc is for timing code for profiling this behaves differently according to the CPU
                uint64 LastCycleCount = __rdtsc(); 

                // NOTE: Here's where the Game Loop begins
                while (GlobalRunning) 
                {
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
                    }

                    // We initialize a new controller with 0 values than we iterate and copy
                    // the values of the sld to the new, keeping pressed button states in the process
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};

                    NewKeyboardController->IsConnected = true;
                    for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

                    if (!GlobalPause)
                    {
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                        {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                        }

                        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++ )
                        {
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                            XINPUT_STATE ControllerState;
                            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                            {
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                                // We need to check deadzones here, the values are defined by XInput
                                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                                if ((NewController->StickAverageX != 0.0f) ||
                                    (NewController->StickAverageY != 0.0f))
                                {
                                    NewController->IsAnalog = true;
                                }
                                //In case someone uses the DPAD we just override the stick values to max value
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }

                                // I don't really get this part
                                // This is for dash movement, we can't test this properly right now 
                                /* real32 Threshold = 0.5f;
                                Win32ProcessXInputDigitalButton(
                                        (NewController->StickAverageX < -Threshold) ? 1 : 0,
                                         &OldController->MoveLeft, 1,
                                         &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton(
                                        (NewController->StickAverageX > Threshold) ? 1 : 0,
                                         &OldController->MoveRight, 1,
                                         &NewController->MoveRight);
                                Win32ProcessXInputDigitalButton(
                                        (NewController->StickAverageY < -Threshold) ? 1 : 0,
                                         &OldController->MoveDown, 1,
                                         &NewController->MoveDown);
                                Win32ProcessXInputDigitalButton(
                                        (NewController->StickAverageY > Threshold) ? 1 : 0,
                                         &OldController->MoveUp, 1,
                                         &NewController->MoveUp); */

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionDown, XINPUT_GAMEPAD_A,
                                                                &NewController->ActionDown);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionRight, XINPUT_GAMEPAD_B,
                                                                &NewController->ActionRight);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionLeft, XINPUT_GAMEPAD_X,
                                                                &NewController->ActionLeft);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->ActionUp, XINPUT_GAMEPAD_Y,
                                                                &NewController->ActionUp);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &NewController->LeftShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &NewController->RightShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Start, XINPUT_GAMEPAD_START,
                                                                &NewController->Start);

                                Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                                &OldController->Back, XINPUT_GAMEPAD_BACK,
                                                                &NewController->Back);

                            }
                            else
                            {
                                // Controller is not connected
                                NewController->IsConnected = false;
                            }
                        }

                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory        = GlobalBackbuffer.Memory;
                        Buffer.Width         = GlobalBackbuffer.Width;
                        Buffer.Height        = GlobalBackbuffer.Height;
                        Buffer.Pitch         = GlobalBackbuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;

                        if (Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        if (Win32State.InputPlayingIndex)
                        {
                            Win32PlaybackInput(&Win32State, NewInput);
                        }
                        if (Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {

                            /* NOTE:
                               Here is how sound output computation works.
                               We define a safety value that is the number
                               of samples we think our game update loop may
                               vary by (let's say up to 2ms)

                               When we wake up to write audio, we will look
                               and see what the play cursor's position is and
                               we will forecast ahead where we think the play
                               cursor will be on the next frame boundary.

                               We will then  look to see if the write cursor is
                               before that by at least our safety value. If it 
                               is, the target fill position is that frame 
                               boundary plus one frame. This gives us perfect
                               audio sync in the case of a card that has low
                               enough latency.

                               If the write cursor is _after_ that safety
                               margin, then we assume we can never sync the
                               audio perfectly, so we will write one frame's 
                               worth of audio plus the safety margin's worth
                               of guard samples.
                             */


                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            // NOTE: Looking where we where in the sound output and map that into our sound buffer,
                            // that's where we're going to write to
                            DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                                                 SoundOutput.SecondaryBufferSize);

                            DWORD ExpectedSoundBytesPerFrame = 
                                (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;

                            real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);

                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                            DWORD SafeWriteCursor = WriteCursor;
                            // if Write cursor is behind in the circular buffer we unwrap it to find the delta
                            if (SafeWriteCursor < PlayCursor) 
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;

                            bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                            }
                            TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                                BytesToWrite += TargetCursor;
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }

                            // NOTE: We create the Sound Buffer then write to wherever we computed we wanted to write to before
                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&GameMemory, &SoundBuffer);
                            }

#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

                            Marker->OutputPlayCursor       = PlayCursor;
                            Marker->OutputWriteCursor      = WriteCursor;
                            Marker->OutputLocation         = ByteToLock;
                            Marker->OutputByteCount        = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

                           /* NOTE: What we're doing below is duplicating the write cursor and 
                              in case the Write Cursor has wrapped around the circular audio buffer
                              (if statement) we'll add the Buffer Size so that the position of this
                              UnwrappedCursor is the position the Write cursor WOULD BE if the
                              buffer wasn't circular. 
                              Then we can subtract both cursors to get the Delta which is our
                              Latency expressed in Bytes */

                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (UnwrappedWriteCursor < PlayCursor)
                            {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;

                            /* NOTE: We're going to divide the Delta we found before by
                               the amount of Bytes in each sample this will show us how much
                               of each sample is comprised of latency. Then we divide that
                               by samples per second so we know how much of this latency we
                               have each second the audio plays. */

                            AudioLatencySeconds = 
                                (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
                                 (real32)SoundOutput.SamplesPerSecond);

                            char TextBuffer[256];
                            _snprintf_s(TextBuffer, sizeof(TextBuffer),
                                    "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                                    ByteToLock, TargetCursor, BytesToWrite,
                                    PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);

                            // OutputDebugStringA(TextBuffer);

#endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                        }


                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            if (SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));

                                if (SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }
                            }

                            // Asserting to check if we didn't sleep for too long
                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

                            if (TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                // TODO: Log Missed Sleep
                            }

                            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {

                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            }
                        }
                        else
                        {
                            //TODO: Logging
                        }

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter = EndCounter;

                        win32_window_dimension Dimensions = GetWindowDimension(Window);
#if HANDMADE_INTERNAL
                        Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                                              DebugTimeMarkerIndex - 1, &SoundOutput, TargetSecondsPerFrame);
#endif
                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimensions.Width, Dimensions.Height);
                        ReleaseDC(Window, DeviceContext);

                        FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
                        // Debug Code to check where playcursor is so we can try to sync it with the Blit Render
                        // We do it after the page flip above to be a bit more accurate

                        {
                            DWORD DebugPlayCursor;
                            DWORD DebugWriteCursor;
                            if (GlobalSecondaryBuffer->GetCurrentPosition(&DebugPlayCursor, &DebugWriteCursor) == DS_OK)
                            {
                                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                                win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

                                Marker->FlipPlayCursor = DebugPlayCursor;
                                Marker->FlipWriteCursor = DebugWriteCursor;
                            }

                        }
#endif
                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;

                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        real64 FPS = 0.0f;
                        real64 MCPF = ((real64)CyclesElapsed / (1000 * 1000));

                        char FPSBuffer[256];
                        _snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/g | %.02ff/s | %.02fmc/f\n", MSPerFrame, FPS, MCPF);
                        // OutputDebugStringA(FPSBuffer);

#if HANDMADE_INTERNAL
                        ++DebugTimeMarkerIndex;
                        if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
                        {
                            DebugTimeMarkerIndex = 0;
                        }
#endif
                    }
                }
            }
        }
    }
    return 0;
}
