/*
 * DSP_Init.c
 *
 *  Created on: Dec 15, 2025
 *      Author: Huzaifa
 */

#include "DSP/DSP_Init.h"

DSP_LPF LPF;

void DSP_Init(void){
	DSP_IIR_LPF_Init(&LPF);
}

