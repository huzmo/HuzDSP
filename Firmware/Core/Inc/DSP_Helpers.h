/*
 * DSP_Helpers.h
 *
 *  Created on: May 29, 2026
 *      Author: Huzaifa
 */

#ifndef INC_DSP_HELPERS_H_
#define INC_DSP_HELPERS_H_
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef FFT_Length
#define FFT_Length  2048
#endif
void HanningWindow(float* buf);

#endif /* INC_DSP_HELPERS_H_ */
