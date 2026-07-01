/*
 * pedal_logic.c
 *
 * Created on: May 31, 2026
 * Author: Huzaifa
 */

#include "pedal_logic.h"

// Allocate the memory here
DisplayManager_t g_display_mgr = { {} };

EffectMetadata_t effect_db[] = {
    {
        .name = "Bypass",
        .abbreviation = "+",
        .description = "Signal passes through unaffected.",
        .paramNames = {"--", "--", "--", "--", "--", "--"},
        .paramMin = {0, 0, 0, 0, 0, 0},
        .paramMax = {0, 0, 0, 0, 0, 0},
        .paramDefault = {0, 0, 0, 0, 0, 0}
    },
    {
        .name = "Overdrive",
        .abbreviation = "DRV",
        .description = "Saturated soft-clipping drive path.",
        .paramNames = {"Drive %", "Level %", "--", "--", "--", "--"},
        .paramMin = {0, 0, 0, 0, 0, 0},
        .paramMax = {100, 100, 0, 0, 0, 0},
        .paramDefault = {50, 60, 0, 0, 0, 0}
    },
    {
        .name = "Chorus",
        .abbreviation = "CHRS",
        .description = "Modulated delay creating a thick, shimmering sound.",
        .paramNames = {"Rate %", "Depth %", "Mix %", "--", "--", "--"},
        .paramMin = {0, 0, 0, 0, 0, 0},
        .paramMax = {100, 100, 100, 0, 0, 0},
        .paramDefault = {40, 55, 50, 0, 0, 0}
    },
    {
        .name = "Noise Gate",
        .abbreviation = "GATE",
        .description = "Silences the signal below an adjustable threshold.",
        .paramNames = {"Thresh %", "Attack ms", "Rel ms", "Hold ms", "--", "--"},
        .paramMin = {0, 1, 10, 0, 0, 0},
        .paramMax = {100, 100, 1000, 500, 0, 0},
        .paramDefault = {12, 10, 100, 50, 0, 0}
    },
    {
        .name = "Flanger",
        .abbreviation = "FLG",
        .description = "Sweeping comb-filter modulation.",
        .paramNames = {"Rate %", "Depth %", "Fback %", "--", "--", "--"},
        .paramMin = {0, 0, 0, 0, 0, 0},
        .paramMax = {100, 100, 100, 0, 0, 0},
        .paramDefault = {30, 70, 30, 0, 0, 0}
    },
    {
        .name = "Compression",
        .abbreviation = "CMP",
        .description = "Dynamic range reduction with soft-knee smoothing.",
        .paramNames = {"Thresh dB", "Ratio :1", "Knee dB", "Attack ms", "Rel ms", "Gain dB"},
        .paramMin = {-60, 1, 0, 1, 10, 0},
        .paramMax = {0, 20, 24, 100, 1000, 24},
        .paramDefault = {-18, 4, 6, 10, 100, 6}
    },
    {
        .name = "Tremolo",
        .abbreviation = "TREM",
        .description = "Periodic amplitude volume modulation.",
        .paramNames = {"Rate %", "Depth %", "--", "--", "--", "--"},
        .paramMin = {0, 0, 0, 0, 0, 0},
        .paramMax = {100, 100, 0, 0, 0, 0},
        .paramDefault = {50, 70, 0, 0, 0, 0}
    }
};
