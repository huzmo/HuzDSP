/*
 * screen.h
 *
 *  Created on: May 17, 2026
 *      Author: Huzaifa
 */

#ifndef INC_SCREEN_H_
#define INC_SCREEN_H_
#include "lvgl.h"

lv_group_t *ui_get_encoder_group(void);
void ui_rebuild_encoder_group(lv_group_t *g);
void ui_rebuild_pedal_encoder_group(void);
#endif /* INC_SCREEN_H_ */
