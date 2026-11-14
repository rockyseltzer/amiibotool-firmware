#ifndef MUI_U8G2_H
#define MUI_U8G2_H

#include "u8g2.h"

#include "hal_spi_bus.h"
#include "mui_defines.h"

void mui_u8g2_init(u8g2_t *p_u8g2);
void mui_u8g2_deinit(u8g2_t *p_u8g2);

void mui_u8g2_set_lcd_default_contrast_level(void);

void mui_u8g2_set_contrast_level(uint8_t value);

#endif /* MUI_U8G2_H */
