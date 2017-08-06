#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#else
#define DWORD	unsigned long
#endif

#define	PAINTBUFFER_SIZE	512
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int snd_scaletable[32][256];
int *snd_p, snd_linear_count, snd_vol;
short* snd_out;

void Snd_WriteLinearBlastStereo16(void)
{
	for (auto i = 0; i < snd_linear_count; i += 2)
	{
		auto val = snd_p[i] * snd_vol >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < static_cast<short>(0x8000))
			snd_out[i] = static_cast<short>(0x8000);
		else
			snd_out[i] = val;

		val = snd_p[i + 1] * snd_vol >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < static_cast<short>(0x8000))
			snd_out[i + 1] = static_cast<short>(0x8000);
		else
			snd_out[i + 1] = val;
	}
}

void S_TransferStereo16(int endtime)
{
	DWORD* pbuf;
#ifdef _WIN32
	DWORD dwSize, dwSize2;
	DWORD* pbuf2;
	HRESULT hresult;
#endif

	snd_vol = volume.value * 256;

	snd_p = reinterpret_cast<int *>(paintbuffer);
	auto lpaintedtime = paintedtime;

#ifdef _WIN32
	if (pDSBuf)
	{
		auto reps = 0;

		while ((hresult = pDSBuf->Lock(0, gSndBufSize, &pbuf, &dwSize, &pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf("S_TransferStereo16: DS: couldn't restore buffer\n");
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = reinterpret_cast<DWORD *>(shm->buffer);
	}

	while (lpaintedtime < endtime)
	{
		// handle recirculating buffer issues
		auto lpos = lpaintedtime & (shm->samples >> 1) - 1;

		snd_out = reinterpret_cast<short *>(pbuf) + (lpos << 1);

		snd_linear_count = (shm->samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		Snd_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += snd_linear_count >> 1;
	}

#ifdef _WIN32
	if (pDSBuf)
		pDSBuf->Unlock(pbuf, dwSize, nullptr, 0);
#endif
}

void S_TransferPaintBuffer(int endtime)
{
	int val;
	DWORD* pbuf;
#ifdef _WIN32
	DWORD dwSize, dwSize2;
	DWORD* pbuf2;
	HRESULT hresult;
#endif

	if (shm->samplebits == 16 && shm->channels == 2)
	{
		S_TransferStereo16(endtime);
		return;
	}

	auto p = reinterpret_cast<int *>(paintbuffer);
	auto count = (endtime - paintedtime) * shm->channels;
	auto out_mask = shm->samples - 1;
	auto out_idx = paintedtime * shm->channels & out_mask;
	auto step = 3 - shm->channels;
	int snd_vol = volume.value * 256;

#ifdef _WIN32
	if (pDSBuf)
	{
		auto reps = 0;

		while ((hresult = pDSBuf->Lock(0, gSndBufSize, &pbuf, &dwSize, &pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf("S_TransferPaintBuffer: DS: couldn't restore buffer\n");
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = reinterpret_cast<DWORD *>(shm->buffer);
	}

	if (shm->samplebits == 16)
	{
		auto out = reinterpret_cast<short *>(pbuf);
		while (count--)
		{
			val = *p * snd_vol >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < static_cast<short>(0x8000))
				val = static_cast<short>(0x8000);
			out[out_idx] = val;
			out_idx = out_idx + 1 & out_mask;
		}
	}
	else if (shm->samplebits == 8)
	{
		auto out = reinterpret_cast<unsigned char *>(pbuf);
		while (count--)
		{
			val = *p * snd_vol >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < static_cast<short>(0x8000))
				val = static_cast<short>(0x8000);
			out[out_idx] = (val >> 8) + 128;
			out_idx = out_idx + 1 & out_mask;
		}
	}

#ifdef _WIN32
	if (pDSBuf)
	{
		DWORD dwNewpos, dwWrite;
		pDSBuf->Unlock(pbuf, dwSize, nullptr, 0);
		pDSBuf->GetCurrentPosition(&dwNewpos, &dwWrite);

		//		if ((dwNewpos >= il) && (dwNewpos <= ir))
		//			Con_Printf("%d-%d p %d c\n", il, ir, dwNewpos);
	}
#endif
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void SND_PaintChannelFrom8(channel_t* ch, sfxcache_t* sc, int endtime);
void SND_PaintChannelFrom16(channel_t* ch, sfxcache_t* sc, int endtime);

void S_PaintChannels(int endtime)
{
	int count;

	while (paintedtime < endtime)
	{
		// if paintbuffer is smaller than DMA buffer
		auto end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		Q_memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

		// paint in the channels.
		auto ch = channels;
		for (auto i = 0; i < total_channels; i++ , ch++)
		{
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			auto sc = S_LoadSound(ch->sfx);
			if (!sc)
				continue;

			auto ltime = paintedtime;

			while (ltime < end)
			{ // paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);

					ltime += count;
				}

				// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else
					{ // channel just stopped
						ch->sfx = nullptr;
						break;
					}
				}
			}
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void SND_InitScaletable(void)
{
	for (auto i = 0; i < 32; i++)
		for (auto j = 0; j < 256; j++)
			snd_scaletable[i][j] = static_cast<signed char>(j) * i * 8;
}


void SND_PaintChannelFrom8(channel_t* ch, sfxcache_t* sc, int count)
{
	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	auto lscale = snd_scaletable[ch->leftvol >> 3];
	auto rscale = snd_scaletable[ch->rightvol >> 3];
	auto sfx = static_cast<unsigned char *>(sc->data) + ch->pos;

	for (auto i = 0; i < count; i++)
	{
		int data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}

	ch->pos += count;
}


void SND_PaintChannelFrom16(channel_t* ch, sfxcache_t* sc, int count)
{
	auto leftvol = ch->leftvol;
	auto rightvol = ch->rightvol;
	auto sfx = reinterpret_cast<signed short *>(sc->data) + ch->pos;

	for (auto i = 0; i < count; i++)
	{
		int data = sfx[i];
		auto left = data * leftvol >> 8;
		auto right = data * rightvol >> 8;
		paintbuffer[i].left += left;
		paintbuffer[i].right += right;
	}

	ch->pos += count;
}
