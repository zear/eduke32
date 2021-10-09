//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#ifndef __premap_h__
#define __premap_h__

extern char *g_gameNamePtr;
extern char pow2char[];

extern int16_t SpriteCacheList[MAXTILES][3];
extern int32_t g_levelTextTime;
extern int32_t g_numRealPalettes;
extern int32_t voting,vote_map,vote_episode;
extern palette_t CrosshairColors;
int32_t G_EnterLevel(int32_t g);
int32_t G_FindLevelForFilename(const char *fn);
void G_CacheMapData(void);
void G_FadeLoad(int32_t r,int32_t g,int32_t b,int32_t start,int32_t end,int32_t step);
void G_FreeMapState(int32_t mapnum);
void G_NewGame(int32_t vn,int32_t ln,int32_t sk);
void G_ResetTimers(void);
void G_SetCrosshairColor(int32_t r,int32_t g,int32_t b);
void G_UpdateScreenArea(void);
void P_RandomSpawnPoint(int32_t snum);
void P_ResetInventory(int32_t snum);
void P_ResetPlayer(int32_t snum);
void P_ResetStatus(int32_t snum);
void P_ResetWeapons(int32_t snum);
void clearfifo(void);
void xyzmirror(int32_t i,int32_t wn);

#endif
