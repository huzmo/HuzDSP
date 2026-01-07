/*
 * DSP_Init.h
 *
 *  Created on: Dec 15, 2025
 *      Author: Huzaifa
 */

#ifndef __DSP_INIT_H
#define __DSP_INIT_H

#include "stm32h7xx_hal.h"
#include "DSP/DSP_IIR_LPF.h"
#include "DSP/DSP_Shifter.h"
#include "DSP/DSP_IIR_HPF.h"

typedef struct {
	uint8_t Config_Mask; // stores a bit for every effect enabled
} DSP_Stream_t;

extern DSP_Stream_t Stream; // global struct

extern DSP_LPF LPF; // low-pass filter parameter struct
extern DSP_Shifter gPitch;
extern DSP_HPF hpf_300;

void DSP_Init(void);

#endif
