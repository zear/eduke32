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

#ifndef __osdcmds_h__
#define __osdcmds_h__

struct osdcmd_cheatsinfo {
	int32_t cheatnum;	// -1 = none, else = see DoCheats()
	int32_t volume,level;
};

extern struct osdcmd_cheatsinfo osdcmd_cheatsinfo_stat;

int32_t registerosdcommands(void);
void onvideomodechange(int32_t newmode);

extern float r_ambientlight,r_ambientlightrecip;

#pragma pack(push,1)
// key bindings stuff
typedef struct {
    char *name;
    int32_t id;
} keydef_t;

extern keydef_t ConsoleKeys[];
extern char *ConsoleButtons[];
#pragma pack(pop)

#endif	// __osdcmds_h__

