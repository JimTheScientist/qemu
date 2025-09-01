#include "qemu/osdep.h"
#include "hw/ssi/ssi.h"
#include "ui/console.h"
#include "qemu/module.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/qdev-core.h"
#include "ui/console.h"
#include "ui/qemu-pixman.h"

#include "ui/console.h"        // already included — good
#include <pixman.h>

#define PIXMAN_RGB565 0x3 /* pixel format for 16bpp RGB565 */
#define TYPE_ST7789 "st7789"
OBJECT_DECLARE_SIMPLE_TYPE(ST7789State, ST7789)

typedef struct ST7789Class {
    SSIPeripheralClass parent_class;
} ST7789Class;


#include "ui/console.h"



typedef struct ST7789State {
    SSIPeripheral parent_obj;

    uint16_t *fb;
    int width;
    int height;
    bool expecting_command;
    uint8_t current_command;

    int x, y;
    bool cs_active;

    DisplaySurface *ds;
    QemuConsole *con;
    pixman_image_t *img;  // add this
} ST7789State;

static void st7789_update_display(void *opaque)
{
    ST7789State *s = opaque;
    dpy_gfx_update(s->con, 0, 0, s->width, s->height);
}

static const GraphicHwOps st7789_ops = {
    .gfx_update = st7789_update_display,
    // You can optionally add:
    // .invalidate = st7789_invalidate_display,
    // .text_update = st7789_text_console,
};

static void st7789_invalidate_display(void *opaque)
{
    // Currently unused, but required
}

static void st7789_text_console(void *opaque, console_ch_t *chardata)
{
    // Optional if you want to show text in the console tab
}

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

    if (s->expecting_command) {
        s->current_command = value & 0xFF;
        s->expecting_command = false;
        qemu_log("ST7789 got CMD: 0x%02x\n", s->current_command);
    } else {
        qemu_log("ST7789 got DATA: 0x%02x for CMD 0x%02x\n", value & 0xFF, s->current_command);

        // Dummy: Draw a pixel to test rendering
        if (s->x < s->width && s->y < s->height) {
            s->fb[s->y * s->width + s->x] = 0xF800;  // Red in RGB565
            s->x++;
            if (s->x >= s->width) {
                s->x = 0;
                s->y++;
                if (s->y >= s->height)
                    s->y = 0;
            }
        }

        qemu_console_resize(s->con, s->width, s->height);
        dpy_gfx_update(s->con, 0, 0, s->width, s->height);
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
#include "ui/pixel_ops.h"  // for PIXMAN_FORMAT_RGB565

static void st7789_realize(SSIPeripheral *dev, Error **errp)
{
    ST7789State *s = ST7789(dev);

    int linesize = s->width * 2; // 2 bytes per pixel for RGB565
    s->fb = g_malloc0(sizeof(uint16_t) * s->width * s->height);
    qemu_log("Creating display surface w=%d h=%d depth=15 linesize=%d\n", s->width, s->height, linesize);
    pixman_format_code_t format;
    format = qemu_default_pixman_format(16, 1);
    s->ds = qemu_create_displaysurface_from(
                s->width,
                s->height,
                format,        // depth = 15 bits (matches PIXMAN_FORMAT_RGB565)
                linesize,
                (uint8_t *)s->fb);

    if (!s->ds) {
        error_setg(errp, "Failed to create display surface");
        return;
    }

    s->con = graphic_console_init(DEVICE(dev), 0, &st7789_ops, s);

    //SSIPeripheralClass *pc = SSI_PERIPHERAL_GET_CLASS(dev);
    //if (pc->realize) {
    //    pc->realize(dev, errp);
    //}

    st7789_reset(DEVICE(dev));
}






static void st7789_finalize(Object *obj)
{
    ST7789State *s = ST7789(obj);
    g_free(s->fb);
    if (s->img) {
        pixman_image_unref(s->img);
    }
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
