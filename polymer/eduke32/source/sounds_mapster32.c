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

// Stripped sounds.c for use in Mapster32, breaks all ties to game & music

//#include <conio.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"

#include "fx_man.h"
//#include "music.h"
//#include "duke3d.h"
#include "osd.h"

#include "cache1d.h"
#include "macros.h"
#include "mathutil.h"
#include "build.h"  // vec3_t

#ifdef WIN32
#include "winlayer.h"
#endif

#include "sounds_mapster32.h"

#define LOUDESTVOLUME 150
#define MUSICANDSFX 5

static char SM32_havesound = 0;

char SoundToggle = 1;
int32_t NumVoices = 32;
int32_t MixRate = 44100;

int32_t backflag,g_numEnvSoundsPlaying;

void MUSIC_Update(void) {}  // needed when linking

void S_Callback(uint32_t);
extern void initprintf(const char *f, ...);

/*
===================
=
= SoundStartup
=
===================
*/


int32_t S_SoundStartup(void)
{
    int32_t status;
    int32_t fxdevicetype;
    void *initdata = 0;

    // TODO: read config
    int32_t FXVolume=220, /*NumVoices=32,*/ NumChannels=2, NumBits=16, ReverseStereo=0;

    fxdevicetype = ASS_AutoDetect;

#ifdef WIN32
    initdata = (void *) win_gethwnd();
#endif

    status = FX_Init(fxdevicetype, NumVoices, NumChannels, NumBits, MixRate, initdata);
    if (status == FX_Ok)
    {
        FX_SetVolume(FXVolume);
        FX_SetReverseStereo(ReverseStereo);
        status = FX_SetCallBack(S_Callback);
    }

    if (status != FX_Ok)
    {
        initprintf("Sound startup error: %s", FX_ErrorString(FX_Error));
        return -2;
    }

    SM32_havesound = 1;

    return 0;
}

/*
===================
=
= SoundShutdown
=
===================
*/

void S_SoundShutdown(void)
{
    int32_t status;

    if (!SM32_havesound)
        return;

    status = FX_Shutdown();
    if (status != FX_Ok)
        initprintf("Sound shutdown error: %s", FX_ErrorString(FX_Error));
}

int32_t S_LoadSound(uint32_t num)
{
    int32_t   fp = -1, l;

    if (!SM32_havesound) return 0;
    if (num >= MAXSOUNDS || SoundToggle == 0) return 0;

    if (g_sounds[num].filename == NULL)
    {
        OSD_Printf(OSD_ERROR "Sound (#%d) not defined!\n",num);
        return 0;
    }

    if (g_sounds[num].filename1) fp = kopen4load(g_sounds[num].filename1,pathsearchmode);
    if (fp == -1) fp = kopen4load(g_sounds[num].filename,pathsearchmode);
    if (fp == -1)
    {
        OSD_Printf(OSDTEXT_RED "Sound %s(#%d) not found!\n",g_sounds[num].filename,num);
        return 0;
    }

    l = kfilelength(fp);
    g_sounds[num].soundsiz = l;

    g_sounds[num].lock = 200;

    allocache((intptr_t *)&g_sounds[num].ptr,l,(char *)&g_sounds[num].lock);
    kread(fp, g_sounds[num].ptr , l);
    kclose(fp);
    return 1;
}

extern vec3_t pos;
extern int16_t ang, cursectnum;

int32_t S_PlaySound3D(int32_t num, int32_t i, const vec3_t *pos)
{
    int32_t sndist, cx, cy, cz, j/*,k*/;
    int32_t pitche,pitchs,cs;
    int32_t voice, sndang, ca, pitch;

    //    if(num != 358) return 0;

    if (num >= MAXSOUNDS ||
            !SM32_havesound ||
//        ((g_sounds[num].m&8) && ud.lockout) ||
            SoundToggle == 0 ||
            g_sounds[num].num > 3 ||
            FX_VoiceAvailable(g_sounds[num].pr) == 0)
        return -1;

    if (g_sounds[num].m&128)
    {
        S_PlaySound(num);
        return 0;
    }

    if (g_sounds[num].m&4)
    {
        for (j=0; j<MAXSOUNDS; j++)
//            for (k=0; k<g_sounds[j].num; k++)
            if ((g_sounds[j].num > 0) && (g_sounds[j].m&4))
                return -1;
    }

    cx = pos->x;
    cy = pos->y;
    cz = pos->z;
    cs = cursectnum;
    ca = ang;

    sndist = FindDistance3D((cx-pos->x),(cy-pos->y),(cz-pos->z)>>4);

    if (i >= 0 && (g_sounds[num].m&16) == 0 && PN == MUSICANDSFX && SLT < 999 && (sector[SECT].lotag&0xff) < 9)
        sndist = divscale14(sndist,(SHT+1));

    pitchs = g_sounds[num].ps;
    pitche = g_sounds[num].pe;
    cx = klabs(pitche-pitchs);

    if (cx)
    {
        if (pitchs < pitche)
            pitch = pitchs + (rand()%cx);
        else pitch = pitche + (rand()%cx);
    }
    else pitch = pitchs;

    sndist += g_sounds[num].vo;
    if (sndist < 0) sndist = 0;
    if (cs > -1 && sndist && PN != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,SX,SY,SZ-(24<<8),SECT))
        sndist += sndist>>5;
    /*
        switch (num)
        {
        case PIPEBOMB_EXPLODE:
        case LASERTRIP_EXPLODE:
        case RPG_EXPLODE:
            if (sndist > (6144))
                sndist = 6144;
            if (g_player[screenpeek].ps->cursectnum > -1 && sector[g_player[screenpeek].ps->cursectnum].lotag == 2)
                pitch -= 1024;
            break;
        default:
    */
    if (cursectnum > -1 && sector[cursectnum].lotag == 2 && (g_sounds[num].m&4) == 0)
        pitch = -768;
    if (sndist > 31444 && PN != MUSICANDSFX)
        return -1;
//        break;
//    }

    if (g_sounds[num].num > 0 && PN != MUSICANDSFX)
    {
        if (g_sounds[num].SoundOwner[0].i == i) S_StopSound(num);
        else if (g_sounds[num].num > 1) S_StopSound(num);
//        else if (A_CheckEnemySprite(&sprite[i]) && sprite[i].extra <= 0) S_StopSound(num);
    }

    sndang = 2048 + ca - getangle(cx-pos->x,cy-pos->y);
    sndang &= 2047;

    if (g_sounds[num].ptr == 0)
    {
        if (S_LoadSound(num) == 0) return 0;
    }
    else
    {
        if (g_sounds[num].lock < 200)
            g_sounds[num].lock = 200;
        else g_sounds[num].lock++;
    }

    if (g_sounds[num].m&16) sndist = 0;

    if (sndist < ((255-LOUDESTVOLUME)<<6))
        sndist = ((255-LOUDESTVOLUME)<<6);

    if (g_sounds[num].m&1)
    {
        if (g_sounds[num].num > 0) return -1;

        voice = FX_PlayLoopedAuto(g_sounds[num].ptr, g_sounds[num].soundsiz, 0, -1,
                                  pitch,sndist>>6,sndist>>6,0,g_sounds[num].pr,num);
    }
    else
    {
        voice = FX_PlayAuto3D(g_sounds[ num ].ptr, g_sounds[num].soundsiz, pitch,sndang>>4,sndist>>6, g_sounds[num].pr, num);
    }

    if (voice >= FX_Ok)
    {
        g_sounds[num].SoundOwner[g_sounds[num].num].i = i;
        g_sounds[num].SoundOwner[g_sounds[num].num].voice = voice;
        g_sounds[num].num++;
    }
    else g_sounds[num].lock--;
    return (voice);
}

void S_PlaySound(int32_t num)
{
    int32_t pitch,pitche,pitchs,cx;
    int32_t voice;

    if (!SM32_havesound) return;
    if (SoundToggle==0) return;
//    if ((g_sounds[num].m&8) && ud.lockout) return;
    if (FX_VoiceAvailable(g_sounds[num].pr) == 0) return;
    if (num < 0 || num > MAXSOUNDS-1 || !g_sounds[num].filename)
    {
        OSD_Printf("WARNING: invalid sound #%d\n",num);
        return;
    }

    pitchs = g_sounds[num].ps;
    pitche = g_sounds[num].pe;
    cx = klabs(pitche-pitchs);

    if (cx)
    {
        if (pitchs < pitche)
            pitch = pitchs + (rand()%cx);
        else pitch = pitche + (rand()%cx);
    }
    else pitch = pitchs;

    if (g_sounds[num].ptr == 0)
    {
        if (S_LoadSound(num) == 0) return;
    }
    else
    {
        if (g_sounds[num].lock < 200)
            g_sounds[num].lock = 200;
        else g_sounds[num].lock++;
    }

    if (g_sounds[num].m&1)
    {
        voice = FX_PlayLoopedAuto(g_sounds[num].ptr, g_sounds[num].soundsiz, 0, -1,
                                  pitch,LOUDESTVOLUME,LOUDESTVOLUME,LOUDESTVOLUME,g_sounds[num].soundsiz,num);
    }
    else
    {
        voice = FX_PlayAuto3D(g_sounds[ num ].ptr, g_sounds[num].soundsiz, pitch,0,255-LOUDESTVOLUME,g_sounds[num].pr, num);
    }

    if (voice >= FX_Ok)// return;
    {
        g_sounds[num].SoundOwner[g_sounds[num].num].voice = voice;
        g_sounds[num].num++;
        return;
    }
    g_sounds[num].lock--;
}

int32_t A_PlaySound(uint32_t num, int32_t i)
{
    if (num >= MAXSOUNDS) return -1;
    if (i < 0)
    {
        S_PlaySound(num);
        return 0;
    }

    return S_PlaySound3D(num,i, (vec3_t *)&sprite[i]);
}

void S_StopSound(int32_t num)
{
    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0)
        {
            FX_StopSound(g_sounds[num].SoundOwner[g_sounds[num].num-1].voice);
            S_Callback(num);
        }
}

void S_StopEnvSound(int32_t num,int32_t i)
{
    int32_t j, k;

    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0)
        {
            k = g_sounds[num].num;
            for (j=0; j<k; j++)
                if (g_sounds[num].SoundOwner[j].i == i)
                {
                    FX_StopSound(g_sounds[num].SoundOwner[j].voice);
                    break;
                }
        }
}

void S_Update(void)
{
    int32_t sndist, sx, sy, sz, cx, cy, cz;
    int32_t sndang,ca,j,k,i,cs;

    g_numEnvSoundsPlaying = 0;

    cx = pos.x;
    cy = pos.y;
    cz = pos.z;
    cs = cursectnum;
    ca = ang;

    for (j=0; j<MAXSOUNDS; j++)
        for (k=0; k<g_sounds[j].num; k++)
        {
            i = g_sounds[j].SoundOwner[k].i;

            sx = sprite[i].x;
            sy = sprite[i].y;
            sz = sprite[i].z;

            sndang = 2048 + ca - getangle(cx-sx,cy-sy);
            sndang &= 2047;
            sndist = FindDistance3D((cx-sx),(cy-sy),(cz-sz)>>4);
            if (i >= 0 && (g_sounds[j].m&16) == 0 && PN == MUSICANDSFX && SLT < 999 && (sector[SECT].lotag&0xff) < 9)
                sndist = divscale14(sndist,(SHT+1));

            sndist += g_sounds[j].vo;
            if (sndist < 0) sndist = 0;

            if (cs > -1 && sndist && PN != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,sx,sy,sz-(24<<8),SECT))
                sndist += sndist>>5;

            if (PN == MUSICANDSFX && SLT < 999)
                g_numEnvSoundsPlaying++;
            /*
                        switch (j)
                        {
                        case PIPEBOMB_EXPLODE:
                        case LASERTRIP_EXPLODE:
                        case RPG_EXPLODE:
                            if (sndist > (6144)) sndist = (6144);
                            break;
                        default:
            */
            if (sndist > 31444 && PN != MUSICANDSFX)
            {
                S_StopSound(j);
                continue;
            }
//            }

            if (g_sounds[j].ptr == 0 && S_LoadSound(j) == 0) continue;
            if (g_sounds[j].m&16) sndist = 0;

            if (sndist < ((255-LOUDESTVOLUME)<<6))
                sndist = ((255-LOUDESTVOLUME)<<6);

            FX_Pan3D(g_sounds[j].SoundOwner[k].voice,sndang>>4,sndist>>6);
        }
}

void S_Callback(uint32_t num)
{
    int32_t i,j,k;

    k = g_sounds[num].num;

    if (k > 0)
    {
        if ((g_sounds[num].m&16) == 0)
            for (j=0; j<k; j++)
            {
                i = g_sounds[num].SoundOwner[j].i;
                if (sprite[i].picnum == MUSICANDSFX && sector[sprite[i].sectnum].lotag < 3 && sprite[i].lotag < 999)
                {
//                    ActorExtra[i].temp_data[0] = 0;
                    sprite[i].filler &= (~1);
                    if ((j + 1) < k)
                    {
                        g_sounds[num].SoundOwner[j].voice = g_sounds[num].SoundOwner[k-1].voice;
                        g_sounds[num].SoundOwner[j].i     = g_sounds[num].SoundOwner[k-1].i;
                    }
                    break;
                }
            }

        g_sounds[num].num--;
        g_sounds[num].SoundOwner[k-1].i = -1;
    }

    g_sounds[num].lock--;
}

void S_ClearSoundLocks(void)
{
    int32_t i;

    for (i=0; i<MAXSOUNDS; i++)
        if (g_sounds[i].lock >= 200)
            g_sounds[i].lock = 199;
}

int32_t A_CheckSoundPlaying(int32_t i, int32_t num)
{
    UNREFERENCED_PARAMETER(i);
    if (num < 0) num=0;	// FIXME
    return (g_sounds[num].num > 0);
}

int32_t S_CheckSoundPlaying(int32_t i, int32_t num)
{
    if (i == -1)
    {
        if (g_sounds[num].lock >= 200)
            return 1;
        return 0;
    }
    return(g_sounds[num].num);
}
