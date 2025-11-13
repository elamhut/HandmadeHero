#include "definitions.h"
#include "handmade.h"
//TODO Remove this cmath call later!
#include <math.h>
#include <windows.h>
#include <stdio.h>


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

internal uint32
RoundReal32ToUInt32(real32 Real32ToRound)
{
	uint32 Result = (uint32)(Real32ToRound + 0.5f);

	return Result;
}

#include "math.h"
internal int32
FloorReal32ToInt32(real32 Real32ToTruncate)
{
	int32 Result = (int32)floorf(Real32ToTruncate);

	return Result;
}

internal int32
TruncateReal32ToInt32(real32 Real32ToTruncate)
{
	int32 Result = (int32)Real32ToTruncate;

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

inline tile_map *
GetTileMap(world *World, int32 TileMapX, int32 TileMapY)
{
	tile_map *TileMap = 0;

	if ((TileMapX >= 0) && (TileMapX < World->TileMapCountX) &&
		(TileMapY >= 0) && (TileMapY < World->TileMapCountY))
	{
		TileMap = &World->TileMaps[TileMapY * World->TileMapCountX + TileMapX];
	}

	return TileMap;
}

inline uint32
GetTileValueUnchecked(world *World, tile_map *TileMap, int32 TileX, int32 TileY)
{
	Assert(TileMap);
	Assert((TileX >= 0) && (TileX < World->CountX) &&
		   (TileY >= 0) && (TileY < World->CountY));

	uint32 TileMapValue = TileMap->Tiles[TileY * World->CountX + TileX];
	return TileMapValue;
}

inline bool32
IsTileMapPointEmpty(world *World, tile_map *TileMap, int32 TestTileX, int32 TestTileY)
{
	bool32 Empty = false;

	if (TileMap)
	{
		if ((TestTileX >= 0) && (TestTileX < World->CountX) &&
			(TestTileY >= 0) && (TestTileY < World->CountY))
		{
			uint32 TileMapValue = GetTileValueUnchecked(World, TileMap, TestTileX, TestTileY);
			Empty = (TileMapValue == 0);
		}
	}

	return Empty;
}

inline canon_position
GetCanonPosition(world *World, raw_position Pos)
{
	canon_position Result;

	Result.TileMapX = Pos.TileMapX;
	Result.TileMapY = Pos.TileMapY;

	real32 X = Pos.TileMapRelativeX - World->UpperLeftX;
	real32 Y = Pos.TileMapRelativeY - World->UpperLeftY;
	Result.TileX = FloorReal32ToInt32(X / World->TileWidth);
	Result.TileY = FloorReal32ToInt32(Y / World->TileHeight);

	Result.TileRelativeX = X - Result.TileX * World->TileWidth;
	Result.TileRelativeY = Y - Result.TileY * World->TileWidth;

	Assert(Result.TileRelativeX >= 0);
	Assert(Result.TileRelativeY >= 0);
	Assert(Result.TileRelativeX < World->TileWidth);
	Assert(Result.TileRelativeY < World->TileHeight);

	if (Result.TileX < 0)
	{
		Result.TileX = World->CountX + Result.TileX;
		--Result.TileMapX;
	}

	if (Result.TileY < 0)
	{
		Result.TileY = World->CountY + Result.TileY;
		--Result.TileMapY;
	}

	if (Result.TileX >= World->CountX)
	{
		Result.TileX =  Result.TileX - World->CountX;
		++Result.TileMapX;
	}

	if (Result.TileY >= World->CountY)
	{
		Result.TileY =  Result.TileY - World->CountY;
		++Result.TileMapY;
	}

	return Result;
}

internal bool32
IsWorldPointEmpty(world *World, raw_position RawPos)
{
	bool32 Empty = false;

	canon_position Pos = GetCanonPosition(World, RawPos);
	tile_map *TileMap = GetTileMap(World, Pos.TileMapX, Pos.TileMapY);
	Empty = IsTileMapPointEmpty(World, TileMap, Pos.TileX, Pos.TileY);

	return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // NOTE: Assertion to make sure game_button_state array is the same size
    // as the amount of buttons we've added to the struct
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
            (ArrayCount(Input->Controllers[0].Buttons))) 
    //NOTE: Assertion to make sure our game state isn't bigger than our available memory
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);


#define TILEMAP_COUNT_X 17
#define TILEMAP_COUNT_Y 9
	
	uint32 Tiles00[TILEMAP_COUNT_Y][TILEMAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
		{1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
		{1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}
	};

	uint32 Tiles01[TILEMAP_COUNT_Y][TILEMAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
		{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
	};

	uint32 Tiles10[TILEMAP_COUNT_Y][TILEMAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
		{0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}
	};

	uint32 Tiles11[TILEMAP_COUNT_Y][TILEMAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
		{0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
	};

	tile_map TileMaps[2][2];

	TileMaps[0][0].Tiles = (uint32 *)Tiles00;
	TileMaps[0][1].Tiles = (uint32 *)Tiles10;
	TileMaps[1][0].Tiles = (uint32 *)Tiles01;
	TileMaps[1][1].Tiles = (uint32 *)Tiles11;

	world World;
	World.CountX = TILEMAP_COUNT_X;
	World.CountY = TILEMAP_COUNT_Y;
	World.UpperLeftX = -30;
	World.UpperLeftY = 0;
	World.TileWidth  = 60;
	World.TileHeight = 60;
	World.TileMapCountX = 2;
	World.TileMapCountY = 2;
	World.TileMaps = (tile_map *)TileMaps;

	real32 PlayerWidth = 0.75f * World.TileWidth;
	real32 PlayerHeight = World.TileHeight;

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
		GameState->PlayerX = 150;
		GameState->PlayerY = 150;
        Memory->IsInitialized = true;
    }

	tile_map *TileMap = GetTileMap(&World, GameState->PlayerTileMapX, GameState->PlayerTileMapY);
	Assert(TileMap)

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

			dPlayerX *= 64.0f;
			dPlayerY *= 64.0f;

			real32 NewPlayerX = GameState->PlayerX + Input->DeltaTime * dPlayerX;
			real32 NewPlayerY = GameState->PlayerY + Input->DeltaTime * dPlayerY;

			raw_position RawPos = {GameState->PlayerTileMapX, GameState->PlayerTileMapY, NewPlayerX, NewPlayerY};
			raw_position PlayerLeft  = RawPos;
			raw_position PlayerRight = RawPos;
			PlayerLeft.TileMapRelativeX  -= 0.5f * PlayerWidth;
			PlayerRight.TileMapRelativeX += 0.5f * PlayerWidth;

			if (IsWorldPointEmpty(&World, RawPos)     &&
				IsWorldPointEmpty(&World, PlayerLeft) &&
				IsWorldPointEmpty(&World, PlayerRight))
			{
				canon_position PlayerPos = GetCanonPosition(&World, RawPos);
				GameState->PlayerTileMapX = PlayerPos.TileMapX;
				GameState->PlayerTileMapY = PlayerPos.TileMapY;
				GameState->PlayerX = World.UpperLeftX + World.TileWidth  * PlayerPos.TileX + PlayerPos.TileRelativeX;
				GameState->PlayerY = World.UpperLeftY + World.TileHeight * PlayerPos.TileY + PlayerPos.TileRelativeY;
			}
		}
    }

	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 1.0f);

	for (int Row = 0; Row < 9; ++Row)
	{

		for (int Column = 0; Column < 17; ++Column)
		{
			real32 MinX = World.UpperLeftX + ((real32)Column * World.TileWidth);
			real32 MinY = World.UpperLeftY + ((real32)Row * World.TileHeight);
			real32 MaxX = MinX + World.TileWidth;
			real32 MaxY = MinY + World.TileHeight;

			real32 TileColor = GetTileValueUnchecked(&World, TileMap, Column, Row) == 0 ? 1.f : 0.5f;
			DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, TileColor, TileColor, TileColor);
		}
	}

	real32 PlayerR = 0.8f;
	real32 PlayerG = 0.2f;
	real32 PlayerB = 1.0f;
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
