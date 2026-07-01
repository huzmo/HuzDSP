/*
 * AudioPath.c
 *
 * Created on: May 31, 2026
 * Author: Huzaifa
 */
#include "AudioPath.h"
#include "string.h"
#include "arm_math.h"

#define INV_SAMPLE_RATE (1.0f / SAMPLE_RATE)
#define LN_9_NEG        (-2.19722457f)

AudioChannel_t Channel1 = { 0 };
AudioChannel_t Channel2 = { 0 };

void audioPathInit(AudioChannel_t* ch1, AudioChannel_t* ch2) {
    memset(ch1, 0, sizeof(AudioChannel_t));
    memset(ch2, 0, sizeof(AudioChannel_t));
    ch1->chainLen = 1;
    ch1->chain[0] = effectList[0];
    ch1->chain[0].enabled = true;
    ch2->chainLen = 1;
    ch2->chain[0] = effectList[0];
    ch2->chain[0].enabled = true;
}

const Effect_t effectList[NUM_EFFECTS] = {
	{ .process = bypassProcess,     .params = {0,    0,   0,   0,   0,  0}, .state = {0}, .enabled = false },
	{ .process = overdriveProcess,  .params = {50,  60,   0,   0,   0,  0}, .state = {0}, .enabled = false },
	{ .process = chorusProcess,     .params = {40,  55,  50,   0,   0,  0}, .state = {0}, .enabled = false },
	{ .process = noiseGateProcess,  .params = {12,  10, 100,  50,   0,  0}, .state = {0}, .enabled = false },
	{ .process = flangerProcess,    .params = {30,  70,  30,   0,   0,  0}, .state = {0}, .enabled = false },
	{ .process = compressorProcess, .params = {-18,  4,   6,  10, 100,  6}, .state = {0}, .enabled = false },
	{ .process = tremoloProcess,    .params = {50,  70,   0,   0,   0,  0}, .state = {0}, .enabled = false },
};

float bypassProcess(float in, Effect_t* effect) {
	return in;
}

/*
* STATES
  None
	PARAMS
	0: Pre-Gain
	1: Post-Gain
*/
float overdriveProcess(float in, Effect_t* effect) {
	float preGain  = effect->params[0] * 0.01f;
	float postGain = effect->params[1] * 0.01f;
	in = 15.0f * preGain * in;
	float thresh1 = 0.4f;
	if      (in >  thresh1) in =  thresh1;
	else if (in < -thresh1) in = -thresh1;
	in *= 4.0f;
	float thresh2 = 0.3f;
	float output;
	if      (in >  thresh2) output =  thresh2;
	else if (in < -thresh2) output = -thresh2;
	else                    output =  in;
	return postGain * (output / thresh2);
}

/*
* STATES
	0 - 959: delay-line samples (960 = 20ms @ 48kHz)
	960: LFO phase accumulator
	961: write index
	PARAMS
	0: Rate
	1: Depth
	2: Mix
*/
float chorusProcess(float in, Effect_t* effect) {
	float* buf      = effect->state;
	float    phase    = effect->state[960];
	uint32_t writeIdx = (uint32_t)effect->state[961];

	float rate  = (effect->params[0] * 0.01f) * 2.5f;
	float depth = (effect->params[1] * 0.01f) * 350.0f;
	float mix   = effect->params[2] * 0.01f;

	float delayLength = 576.0f + depth * arm_sin_f32(2.0f * PI * phase);
	float readPos = (float)writeIdx - delayLength;

	if (readPos < 0.0f) readPos += 960.0f;

	uint32_t idx0 = (uint32_t)readPos % 960;
	float    frac = readPos - floorf(readPos);
	uint32_t idx1 = (idx0 + 1) % 960;

	float delayed = buf[idx0] * (1.0f - frac) + buf[idx1] * frac;

	buf[writeIdx] = in;
	float out = (1.0f - mix) * in + mix * delayed;

	writeIdx = (writeIdx + 1) % 960;

	phase += rate * INV_SAMPLE_RATE;
	if (phase >= 1.0f) phase -= 1.0f;

	effect->state[960] = phase;
	effect->state[961] = (float)writeIdx;

	return out;
}

/*
* STATES
	0 - 479: delay-line samples (480 = 10ms @ 48kHz)
	480: LFO phase accumulator
	481: write index
	PARAMS
	0: Rate
	1: Depth
	2: Feedback
*/
float flangerProcess(float in, Effect_t* effect) {
	float* buf      = effect->state;
	float    phase    = effect->state[480];
	uint32_t writeIdx = (uint32_t)effect->state[481];

	float rate      = (effect->params[0] * 0.01f) * 4.0f;
	float depthGain = effect->params[1] * 0.01f;
	float feedback  = (effect->params[2] * 0.01f) * 0.9f;

	float delayLength = 240.0f * (1.0f + 0.8f * arm_sin_f32(2.0f * PI * phase));
	float readPos = (float)writeIdx - delayLength;

	if (readPos < 0.0f) readPos += 480.0f;

	uint32_t idx0 = (uint32_t)readPos % 480;
	float    frac = readPos - floorf(readPos);
	uint32_t idx1 = (idx0 + 1) % 480;

	float delayed = buf[idx0] * (1.0f - frac) + buf[idx1] * frac;

	buf[writeIdx] = in + feedback * delayed;
	float out = in + depthGain * delayed;

	writeIdx = (writeIdx + 1) % 480;

	phase += rate * INV_SAMPLE_RATE;
	if (phase >= 1.0f) phase -= 1.0f;

	effect->state[480] = phase;
	effect->state[481] = (float)writeIdx;

	return out;
}

/*
   STATES
	0: Prev sample smooth gain
	1: Cached Attack Coefficient
	2: Cached Release Coefficient
	3: Last tracked Attack Parameter value
	4: Last tracked Release Parameter value
	5: Caching Init Status flag
   PARAMS
	0: Threshold
	1: Ratio
	2: Knee width
	3: Attack
	4: Release
	5: Makeup gain
*/
float compressorProcess(float in, Effect_t* effect) {
	float absIn = fabsf(in);
	if (absIn < 1e-6f) return in;

	float inDecibels = 20.0f * log10f(absIn);

	float staticChar;
	if (inDecibels <= effect->params[0]) staticChar = inDecibels;
	else staticChar = effect->params[0] + (inDecibels - effect->params[0]) / effect->params[1];
	float gain = staticChar - inDecibels;

	if (effect->state[5] == 0.0f || effect->params[3] != effect->state[3] || effect->params[4] != effect->state[4]) {
		effect->state[1] = expf(LN_9_NEG / (SAMPLE_RATE * (effect->params[3] * 0.001f + 1e-6f)));
		effect->state[2] = expf(LN_9_NEG / (SAMPLE_RATE * (effect->params[4] * 0.001f + 1e-6f)));
		effect->state[3] = effect->params[3];
		effect->state[4] = effect->params[4];
		effect->state[5] = 1.0f;
	}

	float coeffAttack  = effect->state[1];
	float coeffRelease = effect->state[2];

	float smoothGain;
	if (gain < effect->state[0])
		smoothGain = coeffAttack  * effect->state[0] + (1.0f - coeffAttack)  * gain;
	else
		smoothGain = coeffRelease * effect->state[0] + (1.0f - coeffRelease) * gain;
	effect->state[0] = smoothGain;

	smoothGain += effect->params[5];
	float linGain = powf(10.0f, smoothGain * 0.05f);
	return in * linGain;
}

/*
* STATES
	0: Previous sample gain
	1: Hold counter
	2: Cached Attack Coefficient
	3: Cached Release Coefficient
	4: Last tracked Attack Parameter value
	5: Last tracked Release Parameter value
	6: Caching Init Status flag
	PARAMS
	0: Threshold
	1: Attack Time
	2: Release Time
	3: Hold Time
*/
float noiseGateProcess(float in, Effect_t* effect) {
	float magIn = fabsf(in);
	float threshold = effect->params[0] * 0.01f;

	float gain;
	if (magIn < threshold) gain = 0.0f;
	else gain = 1.0f;

	// Recomputed only when parameters change
	if (effect->state[6] == 0.0f || effect->params[1] != effect->state[4] || effect->params[2] != effect->state[5]) {
		effect->state[2] = expf(LN_9_NEG / (SAMPLE_RATE * (effect->params[1] * 0.001f + 1e-6f)));
		effect->state[3] = expf(LN_9_NEG / (SAMPLE_RATE * (effect->params[2] * 0.001f + 1e-6f)));
		effect->state[4] = effect->params[1];
		effect->state[5] = effect->params[2];
		effect->state[6] = 1.0f;
	}

	float coeffAttack  = effect->state[2];
	float coeffRelease = effect->state[3];
	float holdSamples  = effect->params[3] * (SAMPLE_RATE * 0.001f);

	float smoothGain;
	if (gain > effect->state[0]) {
		smoothGain = coeffAttack * effect->state[0] + (1.0f - coeffAttack) * gain;
		effect->state[1] = 0.0f;
	}
	else if (effect->state[1] < holdSamples) {
		smoothGain = effect->state[0];
		effect->state[1]++;
	}
	else {
		smoothGain = coeffRelease * effect->state[0] + (1.0f - coeffRelease) * gain;
	}

	effect->state[0] = smoothGain;

	return smoothGain * in;
}

/*
* STATES
	0: LFO phase accumulator
	PARAMS
	0: Rate
	1: Depth
*/
float tremoloProcess(float in, Effect_t* effect) {
	float phase = effect->state[0];

	float rate  = (effect->params[0] * 0.01f) * 8.0f;
	float depth = effect->params[1] * 0.01f;

	float lfo = 1.0f - depth * (0.5f * (1.0f + arm_sin_f32(2.0f * PI * phase)));

	phase += rate * INV_SAMPLE_RATE;
	if (phase >= 1.0f) phase -= 1.0f;
	effect->state[0] = phase;

	return in * lfo;
}

float effectsRouter(AudioChannel_t* ch, float inputSample) {
	float currentSample = inputSample;
	for (uint8_t i = 0; i < ch->chainLen; i++) {
		if (ch->chain[i].enabled && ch->chain[i].process != NULL) {
			currentSample = ch->chain[i].process(currentSample, &ch->chain[i]);
		}
	}
	return currentSample;
}

void syncChannelFromDisplay(AudioChannel_t* channel) {
    if (!channel) return;

    int isChannel2 = (channel == &Channel2);
    int startIdx = isChannel2 ? 5 : 0;

    uint32_t current_preset = g_display_mgr.PresetIdx;
    if (current_preset >= MAX_PRESET_SIZE) {
        current_preset = 0;
    }

    for (int i = 0; i < 5; i++) {
        int global_box_idx = startIdx + i;
        int chosenEffectId = g_display_mgr.effectsIdx[global_box_idx];

        if (chosenEffectId > 0 && chosenEffectId < NUM_EFFECTS) {
            if (channel->chain[i].process != effectList[chosenEffectId].process) {
                channel->chain[i] = effectList[chosenEffectId];
                memset(channel->chain[i].state, 0, sizeof(channel->chain[i].state));
                channel->chain[i].enabled = true;

                Effect_t* preset_src = isChannel2
                    ? &presets[current_preset].Channel2.chain[i]
                    : &presets[current_preset].Channel1.chain[i];

                memcpy(channel->chain[i].params, preset_src->params, sizeof(channel->chain[i].params));
            }
            else {
                channel->chain[i].enabled = true;
            }
        }
        else {
            channel->chain[i].process = bypassProcess;
            channel->chain[i].enabled = false;
        }
    }

    channel->chainLen = 5;

    if (g_display_mgr.presetChanged == 1) {
        g_display_mgr.presetChanged = 0;
    }
}
