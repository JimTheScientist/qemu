#include "qemu/osdep.h"
#include "hw/ssi/ssi.h"
#include "ui/console.h"
#include "qemu/module.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/qdev-core.h"

#define TYPE_ST7789 "st7789"
OBJECT_DECLARE_SIMPLE_TYPE(ST7789State, ST7789)

typedef struct ST7789Class {
    SSIPeripheralClass parent_class;
} ST7789Class;


struct ST7789State {
    SSIPeripheral parent_obj;

    uint16_t *fb;
    int width;
    int height;
    bool expecting_command;
    uint8_t current_command;

    int x, y;

    bool cs_active;  // Add this member
};


int st7789_set_cs(SSIPeripheral *dev, bool cs_active)
{
    ST7789State *s = ST7789(dev);
    s->expecting_command = cs_active; // reset on CS assert
    s->x = 0;
    s->y = 0;

    // Store cs_active in state to track it in transfer_raw
    s->cs_active = cs_active;

    qemu_log("ST7789 CS %s\n", cs_active ? "asserted" : "deasserted");

    return 0;
}


static void st7789_reset(DeviceState *dev)
{
    ST7789State *s = ST7789(dev);
    s->expecting_command = true;
    s->x = s->y = 0;
}
//static uint32_t st7789_transfer_raw(SSIPeripheral *dev, uint32_t value)
//{
//    ST7789State *s = ST7789(dev);
//
//    qemu_log("ST7789 got SPI byte: 0x%02x\n", value & 0xFF);
//
//    // Keep current toggling or DC logic for later verification
//    // For now, just log raw bytes
//
//    return 0;
//}

static uint32_t st7789_transfer_raw(SSIPeripheral *dev, uint32_t value)
{
    ST7789State *s = ST7789(dev);

    // Only process if CS is active (manually track CS)
    //if (!s->cs_active) {
    //    // Ignore data when CS inactive
    //    return 0;
    //}

    if (s->expecting_command) {
        s->current_command = value & 0xFF;  // cast to 8-bit
        s->expecting_command = false;
        qemu_log("ST7789 got CMD: 0x%02x\n", s->current_command);
    } else {
        qemu_log("ST7789 got DATA: 0x%02x for CMD 0x%02x\n", value & 0xFF, s->current_command);
        // TODO: Handle memory writes or commands here
        s->expecting_command = true;
    }

    return 0;
}

static void st7789_init(Object *obj)
{
    ST7789State *s = ST7789(obj);
    s->width = 240;
    s->height = 320;
    s->fb = g_malloc0(sizeof(uint16_t) * s->width * s->height);
}

static void st7789_realize(SSIPeripheral *dev, Error **errp)
{
    ObjectClass *klass = object_get_class(OBJECT(dev));
    ObjectClass *parent_klass = object_class_get_parent(klass);

    if (!klass) {
        qemu_log("st7789_realize: object_get_class returned NULL\n");
    } else if (!parent_klass) {
        qemu_log("st7789_realize: parent class is NULL\n");
    } else {
        const char *parent_name = object_class_get_name(parent_klass);
        if (!parent_name) {
            qemu_log("st7789_realize: parent class name is NULL\n");
        } else {
            qemu_log("st7789_realize: parent type: %s\n", parent_name);
        }
    }

    // Use OBJECT_CLASS_CHECK safely only if parent_klass is valid:
    ST7789Class *st7789_klass = OBJECT_CLASS_CHECK(ST7789Class, klass, TYPE_ST7789);
    SSIPeripheralClass *parent_class = NULL;
    if (parent_klass) {
        parent_class = SSI_PERIPHERAL_CLASS(parent_klass);
    }

    if (parent_class && parent_class->realize) {
        parent_class->realize(dev, errp);
    }

    st7789_reset(DEVICE(dev));
}


static void st7789_finalize(Object *obj)
{
    ST7789State *s = ST7789(obj);
    g_free(s->fb);
}




static void st7789_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);

    dc->desc = "ST7789 SPI LCD Display (Virtual)";
    k->transfer_raw = st7789_transfer_raw;  // use transfer_raw now
    k->realize = st7789_realize;
    k->set_cs = st7789_set_cs;
}




static const TypeInfo st7789_info = {
    .name = TYPE_ST7789,
    .parent = TYPE_SSI_PERIPHERAL,
    .instance_size = sizeof(ST7789State),
    .instance_init = st7789_init,
    .class_init = st7789_class_init,
    .instance_finalize = st7789_finalize,
};

static void st7789_register_types(void)
{
    type_register_static(&st7789_info);
}

type_init(st7789_register_types)
