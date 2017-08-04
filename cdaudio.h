#pragma once
int CDAudio_Init();
void CDAudio_Play(byte track, bool looping);
void CDAudio_Stop();
void CDAudio_Pause();
void CDAudio_Resume();
void CDAudio_Shutdown();
void CDAudio_Update();
