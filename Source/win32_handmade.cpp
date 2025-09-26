#include <cstdint>
#include <windows.h>
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define internal        static 
#define local_persist   static 
#define global_variable static

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel = 4;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;

win32_window_dimension
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
RenderWeirdGradient(win32_offscreen_buffer Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer.Memory;

    for (int Y = 0; Y < Buffer.Height; ++Y) 
    {
        uint32 *Pixel = (uint32 *)Row;

        for (int X = 0; X < Buffer.Width; ++X) 
        {
            // Pixel in Memory: RR GG BB XX
            // When we put into memory we get: 0xXXBBGGRR !
            // Little Endian Architecture!
            // Microsoft people looking at that didn't like it!
            // They wanted to see the memory with the RGB in the proper place
            // So they defined it backwards in the Memory of the machine
            // ACTUAL Pixel in Memory: BB GG RR XX
            // So all windows bitmaps have Blue First, Green then Red Bytes!

            uint8 Blue  = (X + XOffset);
            uint8 Green = (Y + YOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer.Pitch;
    }
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

    int BitmapMemorySize = (Buffer->Width * Buffer->Height)*Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    //TODO: Clear to Black
    Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, 
                           int WindowWidth, int WindowHeight, 
                           win32_offscreen_buffer Buffer)
{
    StretchDIBits(DeviceContext,
                  /*
                  X, Y, Width, Height, 
                  X, Y, Width, Height, 
                  */
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer.Width, Buffer.Height,
                  Buffer.Memory, 
                  &Buffer.Info, 
                  DIB_RGB_COLORS, 
                  SRCCOPY);
}

LRESULT CALLBACK 
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
                OutputDebugStringA("WM_CLOSE\n");
            } break;

        case WM_DESTROY:
            {
                // TODO: Handle this as an error?
                GlobalRunning = false;
                OutputDebugStringA("WM_DESTROY\n");
            } break;

        case WM_ACTIVATEAPP:
            {
                OutputDebugStringA("WM_ACTIVATEAPP\n");
            } break;

        case WM_PAINT:
            {
                PAINTSTRUCT Paint;
                HDC DeviceContext = BeginPaint(Window, &Paint);
                win32_window_dimension Dimensions = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimensions.Width, Dimensions.Height, GlobalBackbuffer);

                EndPaint(Window, &Paint);
            }
        default:
            {
                //OutputDebugStringA("default\n");
                Result = DefWindowProc(Window, Message, WParam, LParam);
            } break;
    }

    return Result;
}

int CALLBACK 
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR     CommandLine,
        int       ShowCode)
{

WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback; 
    // Below is the actual proces that windows needs to know where to search
    // for the pointer of the function above.
    // A Pointer is unique to a process space, so without this it's impossible
    // for windows to know where the function above is in memory
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "Handmade", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window) 
        {
            // NOTE: Because of CS_OWNDC flag above we can just get one device context
            // and use it forever because we're not sharing with anyone
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            GlobalRunning = true;
            while (GlobalRunning) 
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);


                win32_window_dimension Dimensions = GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimensions.Width, Dimensions.Height, GlobalBackbuffer);

                ++XOffset;
                YOffset += 2;
            }
        }
        else
        {
            //TODO: (Edu) handle THIS SHIT
        }
    }
    else 
    {
    //TODO: (Edu) handle THIS SHIT 
    }

    return 0;
}

