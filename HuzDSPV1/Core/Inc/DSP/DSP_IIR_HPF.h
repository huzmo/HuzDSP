/*
 * DSP_IIR_HPF.h
 *
 *  Created on: Jan 6, 2026
 *      Author: Huzaifa
 */

#ifndef INC_DSP_DSP_IIR_HPF_H_
#define INC_DSP_DSP_IIR_HPF_H_

#pragma once
#include <stdint.h>

typedef struct {
    float a[3];   // feedback coefficients (a0, a1, a2) where a2 = 1
    float b[3];   // feedforward coefficients (b0, b1, b2)
    float x[3];   // input history
    float y[3];   // output history
    float gain;
} DSP_HPF;

void  DSP_HPF_Init(DSP_HPF *f);
float DSP_HPF_Update(DSP_HPF *f, float in);


#endif /* INC_DSP_DSP_IIR_HPF_H_ */
