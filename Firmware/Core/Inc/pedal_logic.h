/*
 * pedal_logic.h
 *
 *  Created on: May 31, 2026
 *      Author: Huzaifa
 */

#ifndef INC_PEDAL_LOGIC_H_
#define INC_PEDAL_LOGIC_H_

#include "lvgl.h"
#include <stdint.h>

#define MAX_PARAMS    6
#define NUM_EFFECTS   7
#define MAX_EFFECT_COUNT 9
#define MAX_PRESET_SIZE 3

typedef struct {
    char PedalNames[NUM_EFFECTS][16];
    int activeSlotIdx;
    int activePedalIdx;
    int newPedalIdx;
    int effectsIdx[MAX_EFFECT_COUNT + 1];
    int presetChanged;
    int PresetIdx;
} DisplayManager_t;

typedef struct {
    char name[16];
    char abbreviation[8];
    char description[128];
    char paramNames[MAX_PARAMS][16];
    float paramMin[MAX_PARAMS];
    float paramMax[MAX_PARAMS];
    float paramDefault[MAX_PARAMS];
} EffectMetadata_t;

extern DisplayManager_t g_display_mgr;

extern EffectMetadata_t effect_db[NUM_EFFECTS];
#endif /* INC_PEDAL_LOGIC_H_ */
