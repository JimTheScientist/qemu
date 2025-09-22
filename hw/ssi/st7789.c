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

#define TYPE_ST7789 "st7789"
OBJECT_DECLARE_SIMPLE_TYPE(ST7789State, ST7789)

typedef struct ST7789Class {
    SSIPeripheralClass parent_class;
} ST7789Class;


#include "ui/console.h"

typedef enum {
    STATE_EXPECT_CMD,
    STATE_EXPECT_DATA
} ST7789TransferState;


typedef struct ST7789State {
    SSIPeripheral parent_obj;

    uint16_t *fb;
    int width;
    int height;
    bool expecting_command;
    uint8_t current_command;

    int x, y;
    bool cs_active;
    bool dc_level;


    DisplaySurface *ds;
    QemuConsole *con;
    pixman_image_t *img;  // add this
    ST7789TransferState transfer_state;
    uint8_t param_buf[16];  // big enough for commands like CASET (4 bytes)
    int param_len;
    int param_expected;
    int col_start, col_end;
    int row_start, row_end;
    uint8_t madctl;

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

    //qemu_log("ST7789 CS %s\n", cs_active ? "asserted" : "deasserted");

    return cs_active;
}

int st7789_set_dc(SSIPeripheral *dev, bool dc_level) {
    ST7789State *s = ST7789(dev);
    s->dc_level = dc_level;

    //qemu_log("GPIO37 (DC) set %s\n", dc_level ? "HIGH" : "LOW");
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

static void st7789_transform_coords(ST7789State *s, int in_x, int in_y, int *out_x, int *out_y)
{
    int x = in_x;
    int y = in_y;

    if (s->madctl & 0x20) {  // MV: row/col swap (rotate 90)
        int tmp = x;
        x = y;
        y = tmp;
    }

    if (s->madctl & 0x40) {  // MX: mirror x
        x = s->width - 1 - x;
    }

    if (s->madctl & 0x80) {  // MY: mirror y
        y = s->height - 1 - y;
    }

    *out_x = x;
    *out_y = y;
}
//static uint32_t st7789_transfer_raw(SSIPeripheral *dev, uint32_t value)
//{
//    ST7789State *s = ST7789(dev);
//    if (!s->cs_active) return 0;
//    uint8_t byte = value & 0xFF;
//    qemu_log("ST7789 SPI: 0x%X\n", value);
//    return 0;
//    bool is_command = !s->dc_level;
//
//    if (is_command) {
//        qemu_log("ST7789 CMD: 0x%02X\n", byte);
//        s->current_command = byte;
//        s->param_len = 0;
//
//        switch (byte) {
//            case 0x2A: case 0x2B:
//                s->param_expected = 4;
//                break;
//            case 0x36:
//                s->param_expected = 1;
//                break;
//            case 0x3A:
//            case 0x20:
//            case 0x21:
//            case 0x29:
//                s->param_expected = 1;  // Some of these may be 0
//                break;
//            case 0x2C:
//                s->param_expected = -1; // Start pixel stream
//                s->param_len = 0;
//                s->x = s->col_start;
//                s->y = s->row_start;
//                break;
//            default:
//                s->param_expected = 0; // One-byte command, no params
//                break;
//        }
//
//    } else {
//        // DATA byte
//        if (s->current_command == 0x00 && s->param_expected == 0) {
//            qemu_log("ST7789: Ignoring DATA 0x%02X with no active command\n", byte);
//            return 0;
//        }
//
//        if (s->param_expected > 0) {
//            s->param_buf[s->param_len++] = byte;
//            if (s->param_len == s->param_expected) {
//                switch (s->current_command) {
//                    case 0x2A:
//                        s->col_start = (s->param_buf[0] << 8) | s->param_buf[1];
//                        s->col_end   = (s->param_buf[2] << 8) | s->param_buf[3];
//                        break;
//                    case 0x2B:
//                        s->row_start = (s->param_buf[0] << 8) | s->param_buf[1];
//                        s->row_end   = (s->param_buf[2] << 8) | s->param_buf[3];
//                        break;
//                    case 0x36:
//                        s->madctl = s->param_buf[0];
//                        qemu_log("ST7789 MADCTL = 0x%02X\n", s->madctl);
//                        break;
//                }
//                s->param_expected = 0;
//                s->param_len = 0;
//            }
//
//        } else if (s->param_expected == -1 && s->current_command == 0x2C) {
//            // Streaming RGB565 pixel data
//            s->param_buf[s->param_len++] = byte;
//            if (s->param_len == 2) {
//                uint16_t color = (s->param_buf[0] << 8) | s->param_buf[1];
//                s->param_len = 0;
//
//                int draw_x, draw_y;
//                st7789_transform_coords(s, s->x, s->y, &draw_x, &draw_y);
//
//                if (draw_x >= 0 && draw_x < s->width &&
//                    draw_y >= 0 && draw_y < s->height) {
//                    s->fb[draw_y * s->width + draw_x] = color;
//                }
//
//                s->x++;
//                if (s->x > s->col_end) {
//                    s->x = s->col_start;
//                    s->y++;
//                    if (s->y > s->row_end) {
//                        s->y = s->row_start;
//                    }
//                }
//
//                qemu_console_resize(s->con, s->width, s->height);
//                dpy_gfx_update(s->con, 0, 0, s->width, s->height);
//            }
//
//        } else {
//            // Invalid data
//            qemu_log("ST7789 Unexpected DATA 0x%02x for CMD 0x%02x\n", byte, s->current_command);
//        }
//    }
//
//    return 0;
//}




static uint32_t st7789_transfer(SSIPeripheral *dev, uint32_t value)
{
    ST7789State *s = ST7789(dev);
    bool is_command = !s->dc_level;
    if (!(s->cs_active)) return 0;
    uint8_t byte = value & 0xFF;
    //qemu_log("ST7789 value: 0x%X\n", value);

    if (is_command) {
        s->current_command = byte;
        s->param_len = 0;
        s->param_expected = 0;
        s->expecting_command = false;


        switch (byte) {
            case 0x2A: // CASET
            case 0x2B: // RASET
                s->param_expected = 4;
                break;
            case 0x36:  // MADCTL
                s->param_expected = 1;
                break;
            case 0x2C: // RAMWR
                s->param_expected = -1;  // variable length
                s->x = s->col_start;
                s->y = s->row_start;
                break;
            case 0x3A:
            case 0x20:
            case 0x21:
            case 0x29:
                s->param_expected = 1;  // or 0 depending on command
                break;
            default:
                s->expecting_command = true;  // one-byte command
                break;
        }

    } else {
        // DATA byte
        if (s->param_expected > 0) {
            s->param_buf[s->param_len++] = byte;
            if (s->param_len == s->param_expected) {
                //qemu_log("ST7789 CMD 0x%02X params ready\n", s->current_command);

                switch (s->current_command) {
                    case 0x2A:  // CASET: set column range
                        s->col_start = (s->param_buf[0] << 8) | s->param_buf[1];
                        s->col_end   = (s->param_buf[2] << 8) | s->param_buf[3];
                        break;

                    case 0x2B:  // RASET: set row range
                        s->row_start = (s->param_buf[0] << 8) | s->param_buf[1];
                        s->row_end   = (s->param_buf[2] << 8) | s->param_buf[3];
                        break;

                    case 0x36:  // MADCTL
                        s->madctl = s->param_buf[0];
                        qemu_log("ST7789 MADCTL = 0x%02X\n", s->madctl);
                        break;

                }

                s->expecting_command = true;
            }
        } else if (s->current_command == 0x2C) {
            //qemu_log("writing pixel data");
            // Write RGB565 pixel data
            s->param_buf[s->param_len++] = byte;
            if (s->param_len == 2) {
                uint16_t color = (s->param_buf[0] << 8) | s->param_buf[1];
                s->param_len = 0;

                // Draw pixel
                int draw_x, draw_y;
                st7789_transform_coords(s, s->x, s->y, &draw_x, &draw_y);
                //qemu_log("DRAW at (%d, %d): color=0x%04x\n", draw_x, draw_y, color);

                if (draw_x >= 0 && draw_x < s->width &&
                    draw_y >= 0 && draw_y < s->height) {
                    s->fb[draw_y * s->width + draw_x] = color;
                    }

                s->x++;
                if (s->x > s->col_end) {
                    s->x = s->col_start;
                    s->y++;
                    if (s->y > s->row_end) {
                        s->y = s->row_start;  // wraparound
                    }
                }
                //dpy_gfx_update(s->con, 0, 0, s->width, s->height);
            }
        } else {
            // Unknown data phase
            qemu_log("ST7789 Unexpected DATA 0x%02x for CMD 0x%02x\n", byte, s->current_command);
        }
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
    pixman_format_code_t format = PIXMAN_r5g6b5;
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
    dpy_gfx_replace_surface(s->con, s->ds);
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
    k->transfer = st7789_transfer;  // use transfer_raw now
    k->realize = st7789_realize;
    k->set_cs = st7789_set_cs;
    k->cs_polarity = SSI_CS_LOW;
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
