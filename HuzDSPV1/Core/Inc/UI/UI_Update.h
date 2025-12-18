/*
 * UI_Update.h
 *
 *  Created on: Dec 15, 2025
 *      Author: Huzaifa
 */

#ifndef INC_UI_UI_UPDATE_H_
#define INC_UI_UI_UPDATE_H_

#include "UI/UI_Init.h"
#include "ssd1306.h"


#define SCREEN_W   128
#define FONT_W     6
#define PARAM_LEN  6
#define PARAM_W    (FONT_W * PARAM_LEN)
#define PARAM_GAP  6
#define TOTAL_W    (3 * PARAM_W + 2 * PARAM_GAP)
#define START_X    ((SCREEN_W - TOTAL_W) / 2)


void UI_SwitchStates(UI_state_t *state);
const unsigned char* UI_GetActiveBitmap(UI_page_t page);
void UI_UpdateSelection(UI_state_t *state, int16_t delta);
void UI_DrawSelectedOption(const UI_state_t *state);
void UI_WriteText(const UI_state_t *state);
void UI_UpdateOLED(UI_state_t *state);

#endif /* INC_UI_UI_UPDATE_H_ */
