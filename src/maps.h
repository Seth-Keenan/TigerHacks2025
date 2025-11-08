// map_data.h
#ifndef MAPS_H
#define MAPS_H

#include "raylib.h"

#define MAP_WIDTH   25
#define MAP_HEIGHT  23

#define MAP_COUNT  8

int (*GetMapByIndex(int index))[MAP_WIDTH];
int (*GetMapForFloor(int floor))[MAP_WIDTH];
int (*GetCurrentMap(void))[MAP_WIDTH];

bool Map_HasBossTile(int (*map)[MAP_WIDTH]);
bool Map_FindTile(int (*map)[MAP_WIDTH], int tileValue, float *outX, float *outY);
int  Map_FindAllTiles(int (*map)[MAP_WIDTH], int tileValue, Vector2 *outPositions, int maxOut);
bool Map_FindKeyTile(int (*map)[MAP_WIDTH], float *outX, float *outY);
bool Map_GetRandomFreeTile(int (*map)[MAP_WIDTH], float *outX, float *outY);
bool Map_IsTileSolid(int (*map)[MAP_WIDTH], int tx, int ty);

#endif