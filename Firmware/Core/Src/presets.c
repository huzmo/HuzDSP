/*
 * presets.c
 *
 *  Created on: Jun 24, 2026
 *      Author: Huzaifa
 */

#include "AudioPath.h"

preset_t presets[MAX_PRESET_SIZE] = {
    {
        .name = "Empty",
        .Channel1 = {
            .chainLen = 0,
            .chain = {
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
            },
            .eq = {0},
            .pInBuf = NULL, .pOutBuf = NULL,
        },
        .Channel2 = {
            .chainLen = 0,
            .chain = {
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
            },
            .eq = {0},
            .pInBuf = NULL, .pOutBuf = NULL,
        },
    },

    {
        .name = "Clean",
        .Channel1 = {
            .chainLen = 2,
            .chain = {
                {
                  .process = noiseGateProcess,
                  .params  = {20.0f, 0.005f, 0.2f, 2400.0f, 0, 0},
                  .state = {0}, .enabled = true
                },
                {
                  .process = compressorProcess,
                  .params  = {-12.0f, 2.0f, 3.0f, 0.01f, 0.1f, 3.0f},
                  .state = {0}, .enabled = true
                },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
            },
            .eq = {0},
            .pInBuf = NULL, .pOutBuf = NULL,
        },
        .Channel2 = {
			.chainLen = 2,
			.chain = {
				{
				  .process = noiseGateProcess,
				  .params  = {10.0f, 0.005f, 0.2f, 2400.0f, 0, 0},
				  .state = {0}, .enabled = true
				},
				{
				  .process = compressorProcess,
				  .params  = {-10.0f, 2.0f, 3.0f, 0.01f, 0.1f, 3.0f},
				  .state = {0}, .enabled = true
				},
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
			},
			.eq = {0},
			.pInBuf = NULL, .pOutBuf = NULL,
        },
    },

    {
        .name = "Metal",
        .Channel1 = {
            .chainLen = 3,
            .chain = {
				{
				  .process = noiseGateProcess,
				  .params  = {5.0f, 0.005f, 0.2f, 2400.0f, 0, 0},
				  .state = {0}, .enabled = true
				},
				{
				  .process = compressorProcess,
				  .params  = {-12.0f, 2.0f, 3.0f, 0.01f, 0.1f, 3.0f},
				  .state = {0}, .enabled = true
				},
				{
				  .process = overdriveProcess,
				  .params  = {20.0f, 15.0f, 0, 0, 0, 0},
				  .state = {0}, .enabled = true
				},
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
                { .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
            },
            .eq = {0},
            .pInBuf = NULL, .pOutBuf = NULL,
        },
        .Channel2 = {
			.chainLen = 3,
			.chain = {
				{
				  .process = noiseGateProcess,
				  .params  = {2.0f, 0.01f, 0.2f, 2400.0f, 0, 0},
				  .state = {0}, .enabled = true
				},
				{
				  .process = compressorProcess,
				  .params  = {-24.0f, 3.0f, 3.0f, 0.01f, 0.15f, 6.0f},
				  .state = {0}, .enabled = true
				},
				{
				  .process = overdriveProcess,
				  .params  = {20.0f, 15.0f, 0, 0, 0, 0},
				  .state = {0}, .enabled = true
				},
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
				{ .process = bypassProcess, .params = {0,0,0,0,0,0}, .state = {0}, .enabled = false },
			},
			.eq = {0},
			.pInBuf = NULL, .pOutBuf = NULL,
        },
    },
};
