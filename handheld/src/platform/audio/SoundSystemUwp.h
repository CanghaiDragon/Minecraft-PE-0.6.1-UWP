#ifndef SoundSystemUwp_H__
#define SoundSystemUwp_H__

#include "SoundSystem.h"

#include <xaudio2.h>
#include <vector>

class SoundSystemUwp : public SoundSystem
{
public:
	SoundSystemUwp();
	~SoundSystemUwp();

	virtual void init();
	virtual void destroy();
	virtual void enable(bool status);

	virtual void setListenerPos(float x, float y, float z);
	virtual void setListenerAngle(float deg);
	virtual void playAt(const SoundDesc& sound, float x, float y, float z,
		float volume, float pitch);

private:
	void removeStoppedSounds();
	void stopAll();

	bool available;
	bool enabled;
	IXAudio2* engine;
	IXAudio2MasteringVoice* masteringVoice;
	std::vector<IXAudio2SourceVoice*> activeVoices;
};

#endif /* SoundSystemUwp_H__ */
