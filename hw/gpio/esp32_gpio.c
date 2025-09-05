/*
 * ESP32 GPIO emulation
 *
 * Copyright (c) 2019 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/gpio/esp32_gpio.h"
#include "hw/ssi/st7789.h"


static uint64_t esp32_gpio_read(void *opaque, hwaddr addr, unsigned int size)
{
    Esp32GpioState *s = ESP32_GPIO(opaque);
    uint64_t r = 0;
    switch (addr) {
    case A_GPIO_STRAP:
        r = s->strap_mode;
        break;

    default:
        break;
    }
    return r;
}
#define GPIO_OUT_REG       0x00
#define GPIO_DIR_REG       0x04
#define GPIO_IN_REG        0x08
#define GPIO_SET_REG       0x0C
#define GPIO_CLEAR_REG     0x10
#define GPIO_ENABLE_REG        0x20
#define GPIO_OUT1_REG          0x30
#define GPIO_OUT1_W1TS_REG     0x58
#define GPIO_OUT1_W1TC_REG     0x5C


#define GPIO_OUT_W1TS_REG   0x14
#define GPIO_OUT_W1TC_REG   0x18

#define GPIO_REG_NAME(addr) \
(addr == GPIO_OUT1_W1TS_REG ? "OUT1_W1TS" : \
addr == GPIO_OUT1_W1TC_REG ? "OUT1_W1TC" : \
addr == GPIO_OUT1_REG ? "OUT1" : \
addr == GPIO_OUT_W1TS_REG ? "OUT_W1TS" : \
addr == GPIO_OUT_W1TC_REG ? "OUT_W1TC" : \
addr == GPIO_ENABLE_REG ? "ENABLE" : \
"UNKNOWN")

//static void esp32_gpio_write(void *opaque, hwaddr addr,
//                             uint64_t value, unsigned int size)
//{
//    Esp32GpioState *s = ESP32_GPIO(opaque);
//    qemu_log("GPIO WRITE: addr=0x%" HWADDR_PRIx " (%s), value=0x%" PRIx64 "\n",
//             addr, GPIO_REG_NAME(addr), value);
//
///**
//    switch (addr) {
//        case GPIO_OUT_W1TS_REG: {
//            uint64_t prev = s->gpio_out;
//            s->gpio_out |= value;
//
//            uint64_t changed = prev ^ s->gpio_out;
//            if (changed & (1ULL << 33)) {
//                qemu_log("GPIO33 set to 1 via W1TS\n");
//            }
//            break;
//        }
//
//        case GPIO_OUT_W1TC_REG: {
//            uint64_t prev = s->gpio_out;
//            s->gpio_out &= ~value;
//
//            uint64_t changed = prev ^ s->gpio_out;
//            if (changed & (1ULL << 33)) {
//                qemu_log("GPIO33 cleared to 0 via W1TC\n");
//            }
//            break;
//        }
//        case GPIO_ENABLE_REG:
//            s->gpio_enable = value;
//            break;
//
//        case GPIO_OUT1_REG:
//            s->gpio_out1 = value;
//            break;
//        case GPIO_OUT1_W1TS_REG: {
//            uint64_t prev = s->gpio_out1;
//            s->gpio_out1 |= value;
//            qemu_log("OUT1_W1TS_REG write: val=0x%" PRIx64 ", prev=0x%" PRIx64 ", new=0x%" PRIx64 "\n", value, prev, s->gpio_out1);
//            if (value & (1ULL << 1)) {
//                qemu_log("GPIO33 SET (via OUT1_W1TS_REG)\n");
//                if (s->st7789_ssi) {
//                    st7789_set_cs(s->st7789_ssi, false); // CS inactive (HIGH)
//                }
//            }
//            break;
//        }
//
//        case GPIO_OUT1_W1TC_REG: {
//            uint64_t prev = s->gpio_out1;
//            s->gpio_out1 &= ~value;
//            qemu_log("OUT1_W1TC_REG write: val=0x%" PRIx64 ", prev=0x%" PRIx64 ", new=0x%" PRIx64 "\n", value, prev, s->gpio_out1);
//            if (value & (1ULL << 1)) { // GPIO33 CLEAR
//                qemu_log("GPIO33 CLEAR (via OUT1_W1TC_REG)\n");
//                if (s->st7789_ssi) {
//                    st7789_set_cs(s->st7789_ssi, true); // CS active (LOW)
//                }
//            }
//            break;
//        }
//
//
//
//        default:
//            qemu_log("GPIO write to unhandled addr: 0x%"HWADDR_PRIx", value: 0x%"PRIx64"\n", addr, value);
//            break;
//    } **/
//    switch (addr) {
//        case 0x24:  // Possibly GPIO_OUT_W1TS_REG (set bits)
//            s->gpio_out |= value;
//            qemu_log("GPIO OUT W1TS write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0x28:  // Possibly GPIO_OUT_W1TC_REG (clear bits)
//            s->gpio_out &= ~value;
//            qemu_log("GPIO OUT W1TC write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0x4c:  // Possibly GPIO_ENABLE_REG
//            s->gpio_enable = value;
//            qemu_log("GPIO ENABLE write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0x8:   // Another GPIO register (possibly GPIO_OUT_REG)
//            s->gpio_out = value;
//            qemu_log("GPIO OUT write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0xc:   // Another GPIO register
//            // ...
//            break;
//
//            // Higher GPIO pins (32+)
//        case 0x554: // GPIO_OUT1_W1TS_REG ?
//            s->gpio_out1 |= value;
//            qemu_log("GPIO OUT1 W1TS write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0x558: // GPIO_OUT1_W1TC_REG ?
//            s->gpio_out1 &= ~value;
//            qemu_log("GPIO OUT1 W1TC write: val=0x%" PRIx64 "\n", value);
//            break;
//        case 0x55c: // GPIO_ENABLE1_REG ?
//            s->gpio_enable1 = value;
//            qemu_log("GPIO ENABLE1 write: val=0x%" PRIx64 "\n", value);
//            break;
//
//            // ... add more as you confirm
//
//        default:
//            qemu_log("GPIO write to unhandled addr: 0x%" HWADDR_PRIx ", value: 0x%" PRIx64 "\n", addr, value);
//            break;
//    }
//
//}

static void esp32_gpio_write(void *opaque, hwaddr addr, uint64_t value, unsigned int size)
{
    Esp32GpioState *s = opaque;

    uint32_t new_val;

    switch (addr) {
        case 0x08: // GPIO_OUT_REG
            s->gpio_out = (uint32_t)value;
            //qemu_log("GPIO OUT write: val=0x%08x\n", s->gpio_out);
            break;

        case 0x0C: // GPIO_OUT_W1TC_REG
            s->gpio_out &= ~(uint32_t)value;
            //qemu_log("GPIO OUT W1TC write: val=0x%08x\n", (uint32_t)value);
            break;

        case 0x14: // GPIO_OUT1_W1TS_REG (Set GPIO32–39 HIGH)
            new_val = s->gpio_out1 | (uint32_t)value;
            if ((new_val ^ s->gpio_out1) & (1 << 1)) {  // GPIO33 changed
                bool cs = !(new_val & (1 << 1)); // CS active LOW
                if (s->st7789_ssi) {
                    st7789_set_cs(s->st7789_ssi, cs);
                    //qemu_log("GPIO33 -> ST7789 CS %s\n", cs ? "LOW (ACTIVE)" : "HIGH (INACTIVE)");
                }
            }
            if (value & (1 << (37 - 32))) {
                //qemu_log("GPIO37 (DC) set HIGH\n");
                if (s->st7789_ssi) {
                    st7789_set_dc(s->st7789_ssi, true);
                }
            }
            s->gpio_out1 = new_val;
            //qemu_log("GPIO OUT1 W1TS write: val=0x%08x\n", (uint32_t)value);
            break;

        case 0x18: // GPIO_OUT1_W1TC_REG (Clear GPIO32–39 LOW)
            new_val = s->gpio_out1 & ~(uint32_t)value;
            if ((new_val ^ s->gpio_out1) & (1 << 1)) {  // GPIO33 changed
                bool cs = !(new_val & (1 << 1)); // CS active LOW
                if (s->st7789_ssi) {
                    st7789_set_cs(s->st7789_ssi, cs);
                    //qemu_log("GPIO33 -> ST7789 CS %s\n", cs ? "LOW (ACTIVE)" : "HIGH (INACTIVE)");
                }
            }
            if (value & (1 << (37 - 32))) {
                //qemu_log("GPIO37 (DC) set LOW\n");
                if (s->st7789_ssi) {
                    st7789_set_dc(s->st7789_ssi, false);
                }
            }
            s->gpio_out1 = new_val;
            //qemu_log("GPIO OUT1 W1TC write: val=0x%08x\n", (uint32_t)value);
            break;

        case 0x24: // GPIO_OUT_W1TS_REG
            s->gpio_out |= (uint32_t)value;
            //qemu_log("GPIO OUT W1TS write: val=0x%08x\n", (uint32_t)value);
            break;

        case 0x28: // GPIO_OUT_W1TC_REG
            s->gpio_out &= ~(uint32_t)value;
            //qemu_log("GPIO OUT W1TC write: val=0x%08x\n", (uint32_t)value);
            break;

        case 0x4C: // GPIO_ENABLE_REG
            s->gpio_enable = (uint32_t)value;
            qemu_log("GPIO ENABLE write: val=0x%08x\n", s->gpio_enable);
            break;

        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                "GPIO WRITE: addr=0x%" HWADDR_PRIx " (UNKNOWN), value=0x%" PRIx64 "\n",
                addr, value);
            break;
    }
}


static const MemoryRegionOps uart_ops = {
    .read =  esp32_gpio_read,
    .write = esp32_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void esp32_gpio_reset_hold(Object *obj, ResetType type)
{
}

static void esp32_gpio_realize(DeviceState *dev, Error **errp)
{
}

static void esp32_gpio_init(Object *obj)
{
    Esp32GpioState *s = ESP32_GPIO(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    /* Set the default value for the strap_mode property */
    object_property_set_int(obj, "strap_mode", ESP32_STRAP_MODE_FLASH_BOOT, &error_fatal);

    memory_region_init_io(&s->iomem, obj, &uart_ops, s,
                          TYPE_ESP32_GPIO, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static Property esp32_gpio_properties[] = {
    /* The strap_mode needs to be explicitly set in the instance init, thus, set
     * the default value to 0. */
    DEFINE_PROP_UINT32("strap_mode", Esp32GpioState, strap_mode, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void esp32_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = esp32_gpio_reset_hold;
    dc->realize = esp32_gpio_realize;
    device_class_set_props(dc, esp32_gpio_properties);
}

static const TypeInfo esp32_gpio_info = {
    .name = TYPE_ESP32_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Esp32GpioState),
    .instance_init = esp32_gpio_init,
    .class_init = esp32_gpio_class_init,
    .class_size = sizeof(Esp32GpioClass),
};

static void esp32_gpio_register_types(void)
{
    type_register_static(&esp32_gpio_info);
}

type_init(esp32_gpio_register_types)
