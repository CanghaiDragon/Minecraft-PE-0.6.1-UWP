#include "SoundSystemUwp.h"

#include "../../client/sound/Sound.h"

#include <cstddef>

namespace
{
	const std::size_t MAX_ACTIVE_VOICES = 32;
}

SoundSystemUwp::SoundSystemUwp()
	: available(false),
	  enabled(true),
	  engine(NULL),
	  masteringVoice(NULL)
{
	init();
}

SoundSystemUwp::~SoundSystemUwp()
{
	destroy();
}

void SoundSystemUwp::init()
{
	if (engine != NULL)
		return;

	if (FAILED(XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR)))
		return;

	if (FAILED(engine->CreateMasteringVoice(&masteringVoice))) {
		engine->Release();
		engine = NULL;
		return;
	}

	available = true;
}

void SoundSystemUwp::destroy()
{
	stopAll();

	if (masteringVoice != NULL) {
		masteringVoice->DestroyVoice();
		masteringVoice = NULL;
	}

	if (engine != NULL) {
		engine->Release();
		engine = NULL;
	}

	available = false;
}

void SoundSystemUwp::enable(bool status)
{
	enabled = status;
	if (!enabled)
		stopAll();
}

void SoundSystemUwp::setListenerPos(float x, float y, float z)
{
	// The original OpenAL implementation translates all sounds into player
	// space and does not update a separate listener position.  Keep the same
	// contract until the original X3DAudio spatialisation path is restored.
	(void)x;
	(void)y;
	(void)z;
}

void SoundSystemUwp::setListenerAngle(float deg)
{
	// SoundSystemAL uses this value for orientation.  The first UWP backend
	// restores reliable PCM playback; positional panning remains a later,
	// independent enhancement.
	(void)deg;
}

void SoundSystemUwp::removeStoppedSounds()
{
	std::vector<IXAudio2SourceVoice*>::iterator it = activeVoices.begin();
	while (it != activeVoices.end()) {
		XAUDIO2_VOICE_STATE state;
		(*it)->GetState(&state, 0);
		if (state.BuffersQueued == 0) {
			(*it)->DestroyVoice();
			it = activeVoices.erase(it);
		}
		else {
			++it;
		}
	}
}

void SoundSystemUwp::stopAll()
{
	for (std::vector<IXAudio2SourceVoice*>::iterator it = activeVoices.begin();
		it != activeVoices.end(); ++it) {
		(*it)->Stop(0);
		(*it)->FlushSourceBuffers();
		(*it)->DestroyVoice();
	}
	activeVoices.clear();
}

void SoundSystemUwp::playAt(const SoundDesc& sound, float x, float y, float z,
	float volume, float pitch)
{
	(void)x;
	(void)y;
	(void)z;

	if (!available || !enabled || masteringVoice == NULL || !sound.isValid() ||
		sound.frames == NULL || sound.size <= 0 || sound.channels <= 0 ||
		sound.byteWidth <= 0 || sound.frameRate <= 0)
		return;

	removeStoppedSounds();
	if (activeVoices.size() >= MAX_ACTIVE_VOICES) {
		activeVoices.front()->Stop(0);
		activeVoices.front()->FlushSourceBuffers();
		activeVoices.front()->DestroyVoice();
		activeVoices.erase(activeVoices.begin());
	}

	WAVEFORMATEX format = {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = static_cast<WORD>(sound.channels);
	format.nSamplesPerSec = static_cast<DWORD>(sound.frameRate);
	format.wBitsPerSample = static_cast<WORD>(sound.byteWidth * 8);
	format.nBlockAlign = static_cast<WORD>(sound.channels * sound.byteWidth);
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	IXAudio2SourceVoice* voice = NULL;
	if (FAILED(engine->CreateSourceVoice(&voice, &format)))
		return;

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = static_cast<UINT32>(sound.size);
	buffer.pAudioData = reinterpret_cast<const BYTE*>(sound.frames);
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	// Windows headers define min/max as macros.  Avoid std::min/std::max here:
	// the macro expansion breaks the expression in the UWP build.
	float safeVolume = volume;
	if (safeVolume < 0.0f) safeVolume = 0.0f;
	if (safeVolume > 4.0f) safeVolume = 4.0f;
	float safePitch = pitch;
	if (safePitch < 0.5f) safePitch = 0.5f;
	if (safePitch > 2.0f) safePitch = 2.0f;
	if (FAILED(voice->SubmitSourceBuffer(&buffer)) ||
		FAILED(voice->SetVolume(safeVolume)) ||
		FAILED(voice->SetFrequencyRatio(safePitch)) ||
		FAILED(voice->Start(0))) {
		voice->DestroyVoice();
		return;
	}

	activeVoices.push_back(voice);
}
