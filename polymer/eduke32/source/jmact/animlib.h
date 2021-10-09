//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment

This file is part of Duke Nukem 3D version 1.5 - Atomic Edition

Duke Nukem 3D is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Original Source: 1996 - Todd Replogle
Prepared for public release: 03/21/2003 - Charlie Wiederhold, 3D Realms
Modifications for JonoF's port by Jonathon Fowler (jonof@edgenetwk.com)
*/
//-------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
//
//      ANIMLIB.H
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _animlib_public_
#define _animlib_public_
#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************
//
//      ANIM_LoadAnim ()
//
// Setup internal anim data structure
//
//****************************************************************************

void ANIM_LoadAnim(char * buffer);

//****************************************************************************
//
//      ANIM_FreeAnim ()
//
// Free up internal anim data structure
//
//****************************************************************************

void ANIM_FreeAnim(void);

//****************************************************************************
//
//      ANIM_NumFrames ()
//
// returns the number of frames in the current anim
//
//****************************************************************************

int32_t ANIM_NumFrames(void);

//****************************************************************************
//
//      ANIM_DrawFrame ()
//
// Draw the frame to a returned buffer
//
//****************************************************************************

uint8_t * ANIM_DrawFrame(int32_t framenumber);

//****************************************************************************
//
//      ANIM_GetPalette ()
//
// return the palette of the anim
//****************************************************************************

uint8_t * ANIM_GetPalette(void);

#ifdef __cplusplus
};
#endif
#endif
