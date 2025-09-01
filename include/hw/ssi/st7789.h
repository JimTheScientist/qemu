#ifndef HW_SSI_ST7789_H
#define HW_SSI_ST7789_H

#include "hw/ssi/ssi.h"

#define TYPE_ST7789 "st7789"
OBJECT_DECLARE_SIMPLE_TYPE(ST7789State, ST7789)

int st7789_set_cs(SSIPeripheral *dev, bool cs_active);

#endif
