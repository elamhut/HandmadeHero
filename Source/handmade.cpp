#include "definitions.h"
#include "handmade.h"
//TODO Remove this cmath call later!
#include <math.h>


internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / 400; // TODO: Hardcoded value here for Tone

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
        GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal int32
RoundReal32ToInt32(real32 Real32ToRound)
{
	int32 Result = (int32)(Real32ToRound + 0.5f);

	return Result;
}

internal int32
RoundReal32ToUInt32(real32 Real32ToRound)
{
	uint32 Result = (uint32)(Real32ToRound + 0.5f);

	return Result;
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer, 
		   	  real32 RealMinX, real32 RealMinY,
			  real32 RealMaxX, real32 RealMaxY,
			  real32 R, real32 G, real32 B)
{
	int32 MinX = RoundReal32ToInt32(RealMinX);
	int32 MaxX = RoundReal32ToInt32(RealMaxX);
	int32 MinY = RoundReal32ToInt32(RealMinY);
	int32 MaxY = RoundReal32ToInt32(RealMaxY);

	if (MinX < 0)
	{
		MinX = 0;
	}

	if (MinY < 0)
	{
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) |
					(RoundReal32ToUInt32(G * 255.0f) << 8)  |
					(RoundReal32ToUInt32(B * 255.0f) << 0));

    uint8 *Row = ((uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel + MinY * Buffer->Pitch);

	for (int Y = MinY; Y < MaxY; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;

		for (int X = MinX; X < MaxX; ++X)
		{
			*Pixel++ = Color;
		}

		Row += Buffer->Pitch;
	}

}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // NOTE: Assertion to make sure game_button_state array is the same size
    // as the amount of buttons we've added to the struct
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
            (ArrayCount(Input->Controllers[0].Buttons))) 
    //NOTE: Assertion to make sure our game state isn't bigger than our available memory
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;

    if (!Memory->IsInitialized)
    {
        Memory->IsInitialized = true;
    }

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) 
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsAnalog)
        {
		}
        else
        {
			real32 dPlayerX = 0.0f;
			real32 dPlayerY = 0.0f;

			if (Controller->MoveUp.EndedDown)
			{
				dPlayerY = -1.0f;
			}

			if (Controller->MoveDown.EndedDown)
			{
				dPlayerY = 1.0f;
			}

			if (Controller->MoveLeft.EndedDown)
			{
				dPlayerX = -1.0f;
			}

			if (Controller->MoveRight.EndedDown)
			{
				dPlayerX = 1.0f;
			}
			GameState->PlayerX += dPlayerX * 100 * Input->DeltaTime;
			GameState->PlayerY += dPlayerY * 100 * Input->DeltaTime;
		}
    }

	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

	uint32 TileMap[9][17] = {
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},
		{0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1},
		{0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},
		{0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},
		{0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1},
		{0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	};

	real32 UpperLeftX = 0.0f;
	real32 UpperLeftY = 0.0f;
	real32 TileWidth  = 60.0f;
	real32 TileHeight = 60.0f;

	for (int Row = 0; Row < 9; ++Row)
	{

		for (int Column = 0; Column < 17; ++Column)
		{
			real32 MinX = UpperLeftX + ((real32)Column * TileWidth);
			real32 MinY = UpperLeftY + ((real32)Row * TileHeight);
			real32 MaxX = MinX + TileWidth;
			real32 MaxY = MinY + TileHeight;

			real32 TileColor  = (real32)(0.5 * TileMap[Row][Column]);
			DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, TileColor, TileColor, TileColor);
		}
	}

	real32 PlayerR = 0.8f;
	real32 PlayerG = 0.2f;
	real32 PlayerB = 1.0f;
	real32 PlayerWidth = 0.75f * TileWidth;
	real32 PlayerHeight = TileHeight;
	real32 PlayerLeft = GameState->PlayerX - 0.5f * PlayerWidth;
	real32 PlayerTop = GameState->PlayerY - PlayerHeight;
	DrawRectangle(Buffer, 
				  PlayerLeft, PlayerTop, 
				  PlayerLeft + PlayerWidth, 
				  PlayerTop  + PlayerHeight, 
				  PlayerR, PlayerG, PlayerB);

}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}

/*
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

            uint8 Blue  = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);

            *Pixel++ = ((Green << 8) | (Blue << 16));
        }

        // NOTE: What we're doing here is putting the Row Pointer at the beggining
        // of the next Row of Pixels by incrementing it by the quantity of Bytes
        // contained in the Width of the screen (Pitch is Width * BytesPerPixel)
        Row += Buffer->Pitch;
    }
} */
