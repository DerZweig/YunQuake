#include "quakedef.h"
#include "winquake.h"

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR * lpGUID, LPDIRECTSOUND FAR * lplpDS, IUnknown FAR * pUnkOuter);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

enum class sndinitstat
{
	SIS_SUCCESS,
	SIS_FAILURE,
	SIS_NOTAVAIL
};

static qboolean wavonly;
static qboolean dsound_init;
static qboolean wav_init;
static qboolean snd_firsttime = qtrue, snd_isdirect, snd_iswave;
static qboolean primary_format_set;

static int sample16;
static int snd_sent, snd_completed;


/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */

HANDLE hData;
HPSTR  lpData, lpData2;

HGLOBAL   hWaveHdr;
LPWAVEHDR lpWaveHdr;

HWAVEOUT hWaveOut;

WAVEOUTCAPS wavecaps;

DWORD gSndBufSize;

MMTIME mmstarttime;

LPDIRECTSOUND       pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

sndinitstat SNDDMA_InitDirect();
qboolean    SNDDMA_InitWav();


/*
==================
S_BlockSound
==================
*/
void S_BlockSound()
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		snd_blocked++;

		if (snd_blocked == 1)
		{
			waveOutReset(hWaveOut);
		}
	}
}


/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound()
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		snd_blocked--;
	}
}


/*
==================
FreeSound
==================
*/
void FreeSound()
{
	if (pDSBuf)
	{
		pDSBuf->Stop();
		pDSBuf->Release();
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && pDSBuf != pDSPBuf)
	{
		pDSPBuf->Release();
	}

	if (pDS)
	{
		pDS->SetCooperativeLevel(mainwindow, DSSCL_NORMAL);
		pDS->Release();
	}

	if (hWaveOut)
	{
		waveOutReset(hWaveOut);

		if (lpWaveHdr)
		{
			for (auto i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader(hWaveOut, lpWaveHdr + i, sizeof(WAVEHDR));
		}

		waveOutClose(hWaveOut);

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}
	}

	pDS         = nullptr;
	pDSBuf      = nullptr;
	pDSPBuf     = nullptr;
	hWaveOut    = nullptr;
	hData       = nullptr;
	hWaveHdr    = nullptr;
	lpData      = nullptr;
	lpWaveHdr   = nullptr;
	dsound_init = qfalse;
	wav_init    = qfalse;
}


/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect()
{
	DSBUFFERDESC dsbuf;
	DSBCAPS      dsbcaps;
	DWORD        dwSize, dwWrite;
	DSCAPS       dscaps;
	WAVEFORMATEX format;
	HRESULT      hresult;

	// ReSharper disable once CppCStyleCast
	memset((void *)&sn, 0, sizeof sn);

	shm = &sn;

	shm->channels   = 2;
	shm->samplebits = 16;
	shm->speed      = 11025;

	memset(&format, 0, sizeof format);
	format.wFormatTag     = WAVE_FORMAT_PCM;
	format.nChannels      = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign    = format.nChannels
		* format.wBitsPerSample / 8;
	format.cbSize          = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
		* format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == nullptr)
		{
			Con_SafePrintf("Couldn't load dsound.dll\n");
			return sndinitstat::SIS_FAILURE;
		}

		pDirectSoundCreate = reinterpret_cast<decltype(pDirectSoundCreate)>(GetProcAddress(hInstDS, "DirectSoundCreate"));

		if (!pDirectSoundCreate)
		{
			Con_SafePrintf("Couldn't get DS proc addr\n");
			return sndinitstat::SIS_FAILURE;
		}
	}

	while ((hresult = iDirectSoundCreate(NULL, &pDS, NULL)) != DS_OK)
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Con_SafePrintf("DirectSound create failed\n");
			return sndinitstat::SIS_FAILURE;
		}

		if (MessageBox(nullptr,
		               "The sound hardware is in use by another app.\n\n"
		               "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
		               "Sound not available",
		               MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("DirectSoundCreate failure\n"
			               "  hardware already in use\n");
			return sndinitstat::SIS_NOTAVAIL;
		}
	}

	dscaps.dwSize = sizeof dscaps;

	if (DS_OK != pDS->GetCaps(&dscaps))
	{
		Con_SafePrintf("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf("No DirectSound driver installed\n");
		FreeSound();
		return sndinitstat::SIS_FAILURE;
	}

	if (DS_OK != pDS->SetCooperativeLevel(mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf("Set coop level failed\n");
		FreeSound();
		return sndinitstat::SIS_FAILURE;
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	memset(&dsbuf, 0, sizeof dsbuf);
	dsbuf.dwSize        = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags       = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat   = nullptr;

	memset(&dsbcaps, 0, sizeof dsbcaps);
	dsbcaps.dwSize     = sizeof dsbcaps;
	primary_format_set = qfalse;

	if (!COM_CheckParm("-snoforceformat"))
	{
		if (DS_OK == pDS->CreateSoundBuffer(&dsbuf, &pDSPBuf, nullptr))
		{
			auto pformat = format;

			if (DS_OK != pDSPBuf->SetFormat(&pformat))
			{
				if (snd_firsttime)
					Con_SafePrintf("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_SafePrintf("Set primary sound buffer format: yes\n");

				primary_format_set = qtrue;
			}
		}
	}

	if (!primary_format_set || !COM_CheckParm("-primarysound"))
	{
		// create the secondary buffer we'll actually work with
		memset(&dsbuf, 0, sizeof dsbuf);
		dsbuf.dwSize        = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags       = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat   = &format;

		memset(&dsbcaps, 0, sizeof dsbcaps);
		dsbcaps.dwSize = sizeof dsbcaps;

		if (DS_OK != pDS->CreateSoundBuffer(&dsbuf, &pDSBuf, nullptr))
		{
			Con_SafePrintf("DS:CreateSoundBuffer Failed");
			FreeSound();
			return sndinitstat::SIS_FAILURE;
		}

		shm->channels   = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed      = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->GetCaps(&dsbcaps))
		{
			Con_SafePrintf("DS:GetCaps failed\n");
			FreeSound();
			return sndinitstat::SIS_FAILURE;
		}

		if (snd_firsttime)
			Con_SafePrintf("Using secondary sound buffer\n");
	}
	else
	{
		if (DS_OK != pDS->SetCooperativeLevel(mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf("Set coop level failed\n");
			FreeSound();
			return sndinitstat::SIS_FAILURE;
		}

		if (DS_OK != pDSPBuf->GetCaps(&dsbcaps))
		{
			Con_Printf("DS:GetCaps failed\n");
			return sndinitstat::SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Con_SafePrintf("   %d channel(s)\n"
		               "   %d bits/sample\n"
		               "   %d bytes/sec\n",
		               shm->channels, shm->samplebits, shm->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
	auto reps = 0;

	while ((hresult = pDSBuf->Lock(0, gSndBufSize, &lpData, &dwSize, nullptr, nullptr, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound();
			return sndinitstat::SIS_FAILURE;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound();
			return sndinitstat::SIS_FAILURE;
		}
	}

	memset(lpData, 0, dwSize);
	//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	pDSBuf->Unlock(lpData, dwSize, nullptr, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = nullptr;

	pDSBuf->Stop();
	pDSBuf->GetCurrentPosition(&mmstarttime.u.sample, &dwWrite);
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	shm->soundalive       = qtrue;
	shm->splitbuffer      = qfalse;
	shm->samples          = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos        = 0;
	shm->submission_chunk = 1;
	shm->buffer           = reinterpret_cast<unsigned char *>(lpData);
	sample16              = shm->samplebits / 8 - 1;

	dsound_init = qtrue;

	return sndinitstat::SIS_SUCCESS;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
qboolean SNDDMA_InitWav()
{
	WAVEFORMATEX format;
	HRESULT      hr;

	snd_sent      = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels   = 2;
	shm->samplebits = 16;
	shm->speed      = 11025;

	memset(&format, 0, sizeof format);
	format.wFormatTag     = WAVE_FORMAT_PCM;
	format.nChannels      = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign    = format.nChannels
		* format.wBitsPerSample / 8;
	format.cbSize          = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
		* format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	while ((hr = waveOutOpen(static_cast<LPHWAVEOUT>(&hWaveOut), WAVE_MAPPER, &format, 0, 0L, CALLBACK_NULL)) != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			Con_SafePrintf("waveOutOpen failed\n");
			return qfalse;
		}

		if (MessageBox(nullptr,
		               "The sound hardware is in use by another app.\n\n"
		               "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
		               "Sound not available",
		               MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("waveOutOpen failure;\n"
			               "  hardware already in use\n");
			return qfalse;
		}
	}

	/* 
	 * Allocate and lock memory for the waveform data. The memory 
	 * for waveform data must be globally allocated with 
	 * GMEM_MOVEABLE and GMEM_SHARE flags. 

	*/
	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData       = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_SafePrintf("Sound: Out of memory.\n");
		FreeSound();
		return qfalse;
	}

	lpData = static_cast<HPSTR>(GlobalLock(hData));
	if (!lpData)
	{
		Con_SafePrintf("Sound: Failed to lock.\n");
		FreeSound();
		return qfalse;
	}
	memset(lpData, 0, gSndBufSize);

	/* 
	 * Allocate and lock memory for the header. This memory must 
	 * also be globally allocated with GMEM_MOVEABLE and 
	 * GMEM_SHARE flags. 
	 */
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, static_cast<DWORD>(sizeof(WAVEHDR)) * WAV_BUFFERS);

	if (hWaveHdr == nullptr)
	{
		Con_SafePrintf("Sound: Failed to Alloc header.\n");
		FreeSound();
		return qfalse;
	}

	lpWaveHdr = static_cast<LPWAVEHDR>(GlobalLock(hWaveHdr));

	if (lpWaveHdr == nullptr)
	{
		Con_SafePrintf("Sound: Failed to lock header.\n");
		FreeSound();
		return qfalse;
	}

	memset(lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (auto i = 0; i < WAV_BUFFERS; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData         = lpData + i * WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr + i, sizeof(WAVEHDR)) !=
			MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Sound: failed to prepare wave headers\n");
			FreeSound();
			return qfalse;
		}
	}

	shm->soundalive       = qtrue;
	shm->splitbuffer      = qfalse;
	shm->samples          = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos        = 0;
	shm->submission_chunk = 1;
	shm->buffer           = reinterpret_cast<unsigned char *>(lpData);
	sample16              = shm->samplebits / 8 - 1;

	wav_init = qtrue;

	return qtrue;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns qfalse if nothing is found.
==================
*/

qboolean SNDDMA_Init()
{
	if (COM_CheckParm("-wavonly"))
		wavonly = qtrue;

	dsound_init = wav_init = 0;

	auto stat = sndinitstat::SIS_FAILURE; // assume DirectSound won't initialize

	/* Init DirectSound */
	if (!wavonly)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect();

			if (stat == sndinitstat::SIS_SUCCESS)
			{
				snd_isdirect = qtrue;

				if (snd_firsttime)
					Con_SafePrintf("DirectSound initialized\n");
			}
			else
			{
				snd_isdirect = qfalse;
				Con_SafePrintf("DirectSound failed to init\n");
			}
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if (!dsound_init && stat != sndinitstat::SIS_NOTAVAIL)
	{
		if (snd_firsttime || snd_iswave)
		{
			snd_iswave = SNDDMA_InitWav();

			if (snd_iswave)
			{
				if (snd_firsttime)
					Con_SafePrintf("Wave sound initialized\n");
			}
			else
			{
				Con_SafePrintf("Wave sound failed to init\n");
			}
		}
	}

	snd_firsttime = qfalse;

	if (!dsound_init && !wav_init)
	{
		if (snd_firsttime)
			Con_SafePrintf("No sound device initialized\n");

		return qfalse;
	}

	return qtrue;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos()
{
	MMTIME mmtime;
	auto   s = 0;
	DWORD  dwWrite;

	if (dsound_init)
	{
		// ReSharper disable once CppAssignedValueIsNeverUsed
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->GetCurrentPosition(&mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if (wav_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;

	s &= shm->samples - 1;

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit()
{
	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (true)
	{
		if (snd_completed == snd_sent)
		{
			Con_DPrintf("Sound overrun\n");
			break;
		}

		if (! (lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
		{
			break;
		}

		snd_completed++; // this buffer has been played
	}

	//
	// submit two new sound blocks
	//
	while (snd_sent - snd_completed >> sample16 < 4)
	{
		auto h = lpWaveHdr + (snd_sent & WAV_MASK);

		snd_sent++;
		/* 
		 * Now the data block can be sent to the output device. The 
		 * waveOutWrite function returns immediately and waveform 
		 * data is sent to the output device in the background. 
		 */
		int wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Failed to write block to device\n");
			FreeSound();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown()
{
	FreeSound();
}
