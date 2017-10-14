#ifndef __ModuleAudio_H__
#define __ModuleAudio_H__

#include "Module.h"
#include "SDL_mixer\include\SDL_mixer.h"
#include "parson.h"

#define DEFAULT_MUSIC_FADE_TIME 2.0f

class ModuleAudio : public Module
{
public:

	ModuleAudio(bool start_enabled = true);
	~ModuleAudio();

	bool Init(JSON_Object* node);
	bool SaveConfig(JSON_Object* node);
	bool CleanUp();

	// Play a music file
	bool PlayMusic(const char* path, float fade_time = DEFAULT_MUSIC_FADE_TIME);
	bool StopMusic();
	bool ResumeMusic();
	void VolumeMusic(int volume);
	void Mute(bool mute);
	void FadeMusic(int ms);

	// Load a WAV in memory
	unsigned int LoadFx(const char* path);

	// Play a previously loaded WAV
	bool PlayFx(unsigned int fx, int repeat = 0);

	update_status UpdateConfig(float dt);

private:

	Mix_Music*			music;
	p2List<Mix_Chunk*>	fx;

	int volume = 0;
	bool mute = false;
};

#endif // __ModuleAudio_H__