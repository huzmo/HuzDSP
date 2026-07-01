/*
 * AudioPath.h
 *
 *  Created on: May 31, 2026
 *      Author: Huzaifa
 */

#ifndef SRC_AUDIOPATH_H_
#define SRC_AUDIOPATH_H_

#include "pedal_logic.h"
#include "eq.h"

#define AUDIO_BLOCK_SIZE 512

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 48000U
#endif

typedef struct Effect_t Effect_t;

typedef float (*EffectFunc)(float, Effect_t*); // input, effect

typedef struct Effect_t {
    EffectFunc process;
    float params[6];
    float state[1024];
    bool enabled;
} Effect_t;

typedef struct {
	/*
	 * Represents the signal path for a channel
	 */
	Effect_t chain[6];
	eq_t eq;

	uint8_t chainLen;

	float* pInBuf;
	float* pOutBuf;
} AudioChannel_t;

typedef struct {
    char name[16];
    AudioChannel_t Channel1;
    AudioChannel_t Channel2;
} preset_t;

extern const Effect_t effectList[NUM_EFFECTS];

void audioPathInit(AudioChannel_t* ch1, AudioChannel_t* ch2);

float effectsRouter(AudioChannel_t* ch, float inputSample);


float bypassProcess(float in, Effect_t* effect);
float overdriveProcess(float in, Effect_t* effect);
float chorusProcess(float in, Effect_t* effect);
float noiseGateProcess(float in, Effect_t* effect);
float flangerProcess(float in, Effect_t* effect);
float compressorProcess(float in, Effect_t* effect);
float tremoloProcess(float in, Effect_t* effect);

extern preset_t presets[MAX_PRESET_SIZE];

void syncChannelFromDisplay(AudioChannel_t* channel);


#endif /* SRC_AUDIOPATH_H_ */
