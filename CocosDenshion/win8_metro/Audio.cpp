//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "DirectXHelper.h"
#include "Audio.h"
#ifdef _WINPHONE
#include "SoundFileReader.h"
#else
#include "MediaStreamer.h"
#endif
#include "CCCommon.h"
#include <mmreg.h>

void AudioEngineCallbacks::Initialize(Audio *audio)
{
    m_audio = audio;
}

// Called in the event of a critical system error which requires XAudio2
// to be closed down and restarted.  The error code is given in error.
void  _stdcall AudioEngineCallbacks::OnCriticalError(HRESULT Error)
{
    m_audio->SetEngineExperiencedCriticalError();
};

Audio::Audio() :
    m_backgroundID(0)
{
}

void Audio::Initialize()
{
    m_engineExperiencedCriticalError = false;

	m_musicEngine = nullptr;
	m_soundEffectEngine = nullptr;
	m_musicMasteringVoice = nullptr;
	m_soundEffectMasteringVoice = nullptr;
}

void Audio::CreateResources()
{
    try
    {	
        DX::ThrowIfFailed(
            XAudio2Create(&m_musicEngine)
            );

#if defined(_DEBUG)
        XAUDIO2_DEBUG_CONFIGURATION debugConfig = {0};
        debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
        debugConfig.TraceMask = XAUDIO2_LOG_ERRORS;
        m_musicEngine->SetDebugConfiguration(&debugConfig);
#endif

        m_musicEngineCallback.Initialize(this);
        m_musicEngine->RegisterForCallbacks(&m_musicEngineCallback);

	    // This sample plays the equivalent of background music, which we tag on the mastering voice as AudioCategory_GameMedia.
	    // In ordinary usage, if we were playing the music track with no effects, we could route it entirely through
	    // Media Foundation. Here we are using XAudio2 to apply a reverb effect to the music, so we use Media Foundation to
	    // decode the data then we feed it through the XAudio2 pipeline as a separate Mastering Voice, so that we can tag it
	    // as Game Media.
        // We default the mastering voice to 2 channels to simplify the reverb logic.
	    DX::ThrowIfFailed(
		    m_musicEngine->CreateMasteringVoice(&m_musicMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr, nullptr, AudioCategory_GameMedia)
        );

        // Create a separate engine and mastering voice for sound effects in the sample
	    // Games will use many voices in a complex graph for audio, mixing all effects down to a
	    // single mastering voice.
	    // We are creating an entirely new engine instance and mastering voice in order to tag
	    // our sound effects with the audio category AudioCategory_GameEffects.
	    DX::ThrowIfFailed(
		    XAudio2Create(&m_soundEffectEngine)
		    );
    
        m_soundEffectEngineCallback.Initialize(this);
        m_soundEffectEngine->RegisterForCallbacks(&m_soundEffectEngineCallback);

        // We default the mastering voice to 2 channels to simplify the reverb logic.
	    DX::ThrowIfFailed(
		    m_soundEffectEngine->CreateMasteringVoice(&m_soundEffectMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr, nullptr, AudioCategory_GameEffects)
		    );
    }
    catch (...)
    {
        m_engineExperiencedCriticalError = true;
    }
}

unsigned int Audio::Hash(const char *key)
{
    unsigned int len = strlen(key);
    const char *end=key+len;
    unsigned int hash;

    for (hash = 0; key < end; key++)
    {
        hash *= 16777619;
        hash ^= (unsigned int) (unsigned char) toupper(*key);
    }
    return (hash);
}

void Audio::ReleaseResources()
{
	if (m_musicMasteringVoice != nullptr) 
    {
        m_musicMasteringVoice->DestroyVoice();
        m_musicMasteringVoice = nullptr;
    }
	if (m_soundEffectMasteringVoice != nullptr) 
    {
        m_soundEffectMasteringVoice->DestroyVoice();
        m_soundEffectMasteringVoice = nullptr;
    }

    EffectList::iterator EffectIter = m_soundEffects.begin();
    for (; EffectIter != m_soundEffects.end(); EffectIter++)
	{
        if (EffectIter->second.m_soundEffectSourceVoice != nullptr) 
        {
            EffectIter->second.m_soundEffectSourceVoice->DestroyVoice();
            EffectIter->second.m_soundEffectSourceVoice = nullptr;
        }
	}
    m_soundEffects.clear();

    m_musicEngine = nullptr;
    m_soundEffectEngine = nullptr;
}

void Audio::Start()
{	 
    if (m_engineExperiencedCriticalError)
    {
        return;
    }

    // 播放背景音乐
    if (! m_backgroundFile.empty())
        PlayBackgroundMusic(m_backgroundFile.c_str(), m_backgroundLoop);
}

// This sample processes audio buffers during the render cycle of the application.
// As long as the sample maintains a high-enough frame rate, this approach should
// not glitch audio. In game code, it is best for audio buffers to be processed
// on a separate thread that is not synced to the main render loop of the game.
void Audio::Render()
{
    if (m_engineExperiencedCriticalError)
    {
        ReleaseResources();
        Initialize();
        CreateResources();
        Start();
        if (m_engineExperiencedCriticalError)
        {
            return;
        }
    }
}

void Audio::PlayBackgroundMusic(const char* pszFilePath, bool bLoop)
{
    m_backgroundFile = pszFilePath;
    m_backgroundLoop = bLoop;

    if (m_engineExperiencedCriticalError) {
        return;
    }

    // 把背景音乐当作普通音效播放
    StopBackgroundMusic(true);
    PlaySoundEffect(pszFilePath, bLoop, m_backgroundID, true);
}

void Audio::StopBackgroundMusic(bool bReleaseData)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    // 停止播放
    StopSoundEffect(m_backgroundID);

    // 释放掉
    if (bReleaseData)
        UnloadSoundEffect(m_backgroundID);
}

void Audio::PauseBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    PauseSoundEffect(m_backgroundID);
}

void Audio::ResumeBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    ResumeSoundEffect(m_backgroundID);
}

void Audio::RewindBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    RewindSoundEffect(m_backgroundID);
}

bool Audio::IsBackgroundMusicPlaying()
{
    return IsSoundEffectStarted(m_backgroundID);
}

void Audio::SetBackgroundVolume(float volume)
{
    m_backgroundMusicVolume = volume;

    if (m_engineExperiencedCriticalError) {
        return;
    }

    // 调整背景音乐音量
    if (m_soundEffects.end() != m_soundEffects.find(m_backgroundID))
    {
        m_soundEffects[m_backgroundID].m_soundEffectSourceVoice->SetVolume(volume);
    }
}

float Audio::GetBackgroundVolume()
{
    return m_backgroundMusicVolume;
}

void Audio::SetSoundEffectVolume(float volume)
{
    m_soundEffctVolume = volume;

    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        // 调整所有音效的音量，背景音乐除外
        if (iter->first != m_backgroundID)
            iter->second.m_soundEffectSourceVoice->SetVolume(m_soundEffctVolume);
	}
}

float Audio::GetSoundEffectVolume()
{
    return m_soundEffctVolume;
}

void Audio::PlaySoundEffect(const char* pszFilePath, bool bLoop, unsigned int& sound, bool isMusic)
{
    sound = Hash(pszFilePath);

    if (m_soundEffects.end() == m_soundEffects.find(sound))
    {
        PreloadSoundEffect(pszFilePath, isMusic);
    }

    // 依然没有资源
    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    m_soundEffects[sound].m_audioBuffer.LoopCount = bLoop ? XAUDIO2_LOOP_INFINITE : 0;

    PlaySoundEffect(sound);
}

void Audio::PlaySoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    StopSoundEffect(sound);

    DX::ThrowIfFailed(
		m_soundEffects[sound].m_soundEffectSourceVoice->SubmitSourceBuffer(&m_soundEffects[sound].m_audioBuffer)
		);

	XAUDIO2_BUFFER buf = {0};
	XAUDIO2_VOICE_STATE state = {0};

    if (m_engineExperiencedCriticalError) {
        // If there's an error, then we'll recreate the engine on the next render pass
        return;
    }

	SoundEffectData* soundEffect = &m_soundEffects[sound];
	HRESULT hr = soundEffect->m_soundEffectSourceVoice->Start();
	if FAILED(hr)
    {
        m_engineExperiencedCriticalError = true;
        return;
    }

	m_soundEffects[sound].m_soundEffectStarted = true;
}

void Audio::StopSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Stop();
    HRESULT hr1 = m_soundEffects[sound].m_soundEffectSourceVoice->FlushSourceBuffers();
    if (FAILED(hr) || FAILED(hr1))
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }

    m_soundEffects[sound].m_soundEffectStarted = false;
}

void Audio::PauseSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Stop();
    if FAILED(hr)
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }
}

void Audio::ResumeSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Start();
    if FAILED(hr)
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }
}

void Audio::RewindSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    // 先停止，再播放
    StopSoundEffect(sound);
    PlaySoundEffect(sound);
}

void Audio::PauseAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        PauseSoundEffect(iter->first);
	}
}

void Audio::ResumeAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        ResumeSoundEffect(iter->first);
	}
}

void Audio::StopAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        StopSoundEffect(iter->first);
	}
}

bool Audio::IsSoundEffectStarted(unsigned int sound)
{
    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return false;

    return m_soundEffects[sound].m_soundEffectStarted;
}

void Audio::PreloadSoundEffect(const char* pszFilePath, bool isMusic)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    int sound = Hash(pszFilePath);

	WAVEFORMATEX waveFormat;
#ifdef _WINPHONE
	SoundFileReader soundFile(ref new Platform::String((cocos2d::CCUtf8ToUnicode(pszFilePath).c_str())));
	Platform::Array<byte> ^soundData = soundFile.GetSoundData();
	uint32 bufferLength = soundData->Length;
	waveFormat = soundFile.GetSoundFormat();

	m_soundEffects[sound].m_soundEffectBufferData = new byte[soundData->Length];
	CopyMemory(m_soundEffects[sound].m_soundEffectBufferData, soundData->Data, soundData->Length);
	m_soundEffects[sound].m_soundEffectBufferLength = soundData->Length;

#else
	MediaStreamer mediaStreamer;
	mediaStreamer.Initialize(cocos2d::CCUtf8ToUnicode(pszFilePath).c_str());

	uint32 bufferLength = mediaStreamer.GetMaxStreamLengthInBytes();
	m_soundEffects[sound].m_soundEffectBufferData = new byte[bufferLength];
	mediaStreamer.ReadAll(m_soundEffects[sound].m_soundEffectBufferData, bufferLength, &m_soundEffects[sound].m_soundEffectBufferLength);
	waveFormat = mediaStreamer.GetOutputWaveFormatEx();
#endif
	m_soundEffects[sound].m_soundID = sound;	

    if (isMusic)
    {
        XAUDIO2_SEND_DESCRIPTOR descriptors[1];
	    descriptors[0].pOutputVoice = m_musicMasteringVoice;
	    descriptors[0].Flags = 0;
	    XAUDIO2_VOICE_SENDS sends = {0};
	    sends.SendCount = 1;
	    sends.pSends = descriptors;

        DX::ThrowIfFailed(
	    m_musicEngine->CreateSourceVoice(&m_soundEffects[sound].m_soundEffectSourceVoice,
            &waveFormat, 0, 1.0f, &m_voiceContext, &sends)
	    );
		//fix bug: set a initial volume
		m_soundEffects[sound].m_soundEffectSourceVoice->SetVolume(m_backgroundMusicVolume);
    } else
    {
        XAUDIO2_SEND_DESCRIPTOR descriptors[1];
        descriptors[0].pOutputVoice = m_soundEffectMasteringVoice;
	    descriptors[0].Flags = 0;
	    XAUDIO2_VOICE_SENDS sends = {0};
	    sends.SendCount = 1;
	    sends.pSends = descriptors;

        DX::ThrowIfFailed(
	    m_soundEffectEngine->CreateSourceVoice(&m_soundEffects[sound].m_soundEffectSourceVoice,
		    &waveFormat, 0, 1.0f, &m_voiceContext, &sends, nullptr)
        );
		//fix bug: set a initial volume
		m_soundEffects[sound].m_soundEffectSourceVoice->SetVolume(m_soundEffctVolume);
    }

	m_soundEffects[sound].m_soundEffectSampleRate = waveFormat.nSamplesPerSec;

	// Queue in-memory buffer for playback
	ZeroMemory(&m_soundEffects[sound].m_audioBuffer, sizeof(m_soundEffects[sound].m_audioBuffer));

	m_soundEffects[sound].m_audioBuffer.AudioBytes = m_soundEffects[sound].m_soundEffectBufferLength;
	m_soundEffects[sound].m_audioBuffer.pAudioData = m_soundEffects[sound].m_soundEffectBufferData;
	m_soundEffects[sound].m_audioBuffer.pContext = &m_soundEffects[sound];
	m_soundEffects[sound].m_audioBuffer.Flags = XAUDIO2_END_OF_STREAM;
    m_soundEffects[sound].m_audioBuffer.LoopCount = 0;
}

void Audio::UnloadSoundEffect(const char* pszFilePath)
{
    int sound = Hash(pszFilePath);

    UnloadSoundEffect(sound);
}

void Audio::UnloadSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    m_soundEffects[sound].m_soundEffectSourceVoice->DestroyVoice();

    m_soundEffects[sound].m_soundEffectBufferData = nullptr;
	m_soundEffects[sound].m_soundEffectSourceVoice = nullptr;
	m_soundEffects[sound].m_soundEffectStarted = false;
    ZeroMemory(&m_soundEffects[sound].m_audioBuffer, sizeof(m_soundEffects[sound].m_audioBuffer));

    m_soundEffects.erase(sound);
}
