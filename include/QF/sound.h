/*
	sound.h

	Sound headers.

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
// sound.h -- client sound i/o functions

#ifndef _SOUND_H
#define _SOUND_H

#include "QF/mathlib.h"
#include "QF/zone.h"

#define AMBIENT_WATER	0
#define AMBIENT_SKY	1
#define AMBIENT_SLIME	2
#define AMBIENT_LAVA	3
#define NUM_AMBIENTS	4	// automatic ambient sounds

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char 	name[MAX_QPATH];
	cache_user_t	cache;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	int		bytes;
	byte	data[4];		// variable sized
} sfxcache_t;

typedef struct
{
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;				// mono samples in buffer
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	unsigned char	*buffer;
} dma_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int	 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int	master_vol;		// 0-255 master volume
	int	phase;	// phase shift between l-r in samples
	int	oldphase;	// phase shift between l-r in samples
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

struct model_s;
void S_Init (struct model_s **worldmodel, int *viewentity,
			 double *host_frametime);
void S_Init_Cvars (void);
void S_Startup (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
				   float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, const vec3_t origin, float vol,
					float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_ClearBuffer (void);
void S_Update (const vec3_t origin, const vec3_t v_forward,
			   const vec3_t v_right, const vec3_t v_up);
void S_ExtraUpdate (void);
void S_BlockSound (void);
void S_UnblockSound (void);

sfx_t *S_PrecacheSound (const char *sample);
void S_TouchSound (const char *sample);
void S_ClearPrecache (void);
void S_BeginPrecaching (void);
void S_EndPrecaching (void);
void SND_PaintChannels(int endtime);

void SND_Init (void);
void SND_Shutdown (void);
void SND_AmbientOff (void);
void SND_AmbientOn (void);
void SND_TouchSound (const char *sample);
void SND_ClearBuffer (void);
void SND_StaticSound (sfx_t *sfx, const vec3_t origin, float vol,
					  float attenuation);
void SND_StartSound (int entnum, int entchannel, sfx_t *sfx,
					 const vec3_t origin, float fvol,  float attenuation);
void SND_StopSound (int entnum, int entchannel);
sfx_t *SND_PrecacheSound (const char *sample);
void SND_ClearPrecache (void);
void SND_Update (const vec3_t origin, const vec3_t v_forward,
				 const vec3_t v_right, const vec3_t v_up);
void SND_StopAllSounds (qboolean clear);
void SND_BeginPrecaching (void);
void SND_EndPrecaching (void);
void SND_ExtraUpdate (void);
void SND_LocalSound (const char *s);
void SND_BlockSound (void);
void SND_UnblockSound (void);

void SND_ResampleSfx (sfxcache_t *sc, byte * data);
sfxcache_t *SND_GetCache (long samples, int rate, int inwidth, int channels,
						  sfx_t *sfx, cache_allocator_t allocator);

void SND_InitScaletable (void);
// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(int entnum, int entchannel);
// spatializes a channel
void SND_Spatialize(channel_t *ch);

// initializes cycling through a DMA buffer and returns information on it
qboolean S_O_Init(void);

// gets the current DMA position
int S_O_GetDMAPos(void);

// shutdown the DMA xfer.
void S_O_Shutdown(void);

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			256
#define	MAX_DYNAMIC_CHANNELS	8

extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern	int			total_channels;

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern qboolean 		fakedma;
extern int 			fakedma_updates;
extern int		paintedtime;
extern int		soundtime;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern volatile dma_t *shm;
extern volatile dma_t sn;
extern vec_t sound_nominal_clip_dist;

extern	struct cvar_s *snd_loadas8bit;
extern	struct cvar_s *bgmvolume;
extern	struct cvar_s *volume;

extern	struct cvar_s	*snd_device;
extern	struct cvar_s	*snd_rate;
extern	struct cvar_s	*snd_bits;
extern	struct cvar_s	*snd_stereo;
extern	struct cvar_s	*snd_interp;
extern	struct cvar_s *snd_stereo_phase_separation;

extern qboolean	snd_initialized;

extern int		snd_blocked;

void S_LocalSound (const char *s);
sfxcache_t *S_LoadSound (sfx_t *s);

wavinfo_t GetWavinfo (const char *name, byte *wav, int wavlength);

void S_O_Submit(void);
void S_O_BlockSound (void);
void S_O_UnblockSound (void);

void S_AmbientOff (void);
void S_AmbientOn (void);

extern struct model_s **snd_worldmodel;

void SND_CallbackLoad (void *object, cache_allocator_t allocator);
sfxcache_t *SND_LoadOgg (QFile *file, sfx_t *sfx, cache_allocator_t allocator);
wavinfo_t SND_GetWavinfo (const char *name, byte * wav, int wavlength);

void SND_WriteLinearBlastStereo16 (void);
void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count);

#endif // _SOUND_H
