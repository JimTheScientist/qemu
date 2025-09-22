/*
 * ESP32 SPI controller
 *
 * Copyright (c) 2019 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "sysemu/sysemu.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi.h"
#include "hw/ssi/esp32s3_spi.h"
#include "qemu/error-report.h"

#define SPI1_DEBUG      0
#define SPI1_WARNING    0


#define S3_SPI_CMD_REG 0x00
#define S3_SPI_W0_REG 0x98
#define S3_SPI_W15_REG 0xD4
#define S3_SPI_MISC_REG 0x20
#define S3_SPI_USER_REG 0x10
#define S3_SPI_USER1_REG 0x14
#define S3_SPI_USER2_REG 0x18
#define S3_SPI_CTRL_REG 0x08
#define S3_SPI_CLK_GATE_REG 0xE8
#define S3_SPI_DMA_CONF_REF 0x30
#define S3_SPI_SLAVE_REG 0xE0
#define S3_SPI_CLOCK_REG 0x0C
#define S3_SPI_MS_DLEN_REG 0x1C
#define S3_SPI_SLAVE_TRANS_START (1 << 18)

enum {
    CMD_RES = 0xab,
    CMD_DP = 0xb9,
    CMD_CE = 0x60,
    CMD_BE = 0xd8,
    CMD_SE = 0x20,
    CMD_PP = 0x02,
    CMD_WRSR = 0x1,
    CMD_RDSR = 0x5,
    CMD_RDID = 0x9f,
    CMD_WRDI = 0x4,
    CMD_WREN = 0x6,
    CMD_READ = 0x03,
    CMD_HPM = 0xa3,
};


typedef struct ESP32S3SpiTransaction {
    uint32_t cmd;
    uint32_t cmd_bytes;

    uint32_t addr;
    uint32_t addr_bytes;

    uint32_t dummy_bytes;

    void* data;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
} ESP32S3SpiTransaction;


static uint64_t esp32s3_spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    ESP32S3SpiState *s = ESP32S3_SPI(opaque);

    uint64_t r = 0;
    switch (s->spi_num) {
        case 1: {
            switch (addr) {
                case A_SPI_MEM_CMD:
                    r = 0;
                    break;
                case A_SPI_MEM_ADDR:
                    r = s->mem_addr;
                    break;
                case A_SPI_MEM_CTRL:
                    r = s->mem_ctrl;
                    break;
                case A_SPI_MEM_CTRL1:
                    r = s->mem_ctrl1;
                    break;
                case A_SPI_MEM_CTRL2:
                    r = s->mem_ctrl2;
                    break;
                case A_SPI_MEM_CLOCK:
                    r = s->mem_clock;
                    break;
                case A_SPI_MEM_USER:
                    r = s->mem_user;
                    break;
                case A_SPI_MEM_USER1:
                    r = s->mem_user1;
                    break;
                case A_SPI_MEM_USER2:
                    r = s->mem_user2;
                    break;
                case A_SPI_MEM_MISO_DLEN:
                    r = s->mem_miso_len;
                    break;
                case A_SPI_MEM_MOSI_DLEN:
                    r = s->mem_mosi_len;
                    break;
                case A_SPI_MEM_RD_STATUS:
                    r = s->mem_rd_st;
                    break;
                case A_SPI_MEM_MISC:
                    r = s->misc;
                    break;
                case A_SPI_MEM_CACHE_FCTRL:
                    r = s->cache_fctrl;
                    break;
                case A_SPI_MEM_FSM:
                    r = s->fsm;
                    break;
                case A_SPI_MEM_W0...A_SPI_MEM_W15:
                    r = s->data_reg[(addr - A_SPI_MEM_W0) / sizeof(uint32_t)];
                    break;
                case A_SPI_MEM_SUS_STATUS:
                    r = s->mem_sus_st;
                    break;
                case A_SPI_MEM_DDR_CTRL:
                    r = s->ddr_ctrl;
                    break;
                case A_SPI_MEM_CLOCK_GATE:
                    r = s->clock_gate;
                    break;
                default:
        #if SPI1_WARNING
                    warn_report("[SPI1] Unsupported read to 0x%lx", addr);
        #endif
                    break;
            }

        #if SPI1_DEBUG
            info_report("[SPI1] Reading 0x%lx (0x%lx)", addr, r);
        #endif
        } break;
        case 3:
        {
            switch (addr) {
                case S3_SPI_CMD_REG:
                    r = 0;
                    break;
                case S3_SPI_DMA_CONF_REF:
                    r = s->ddr_ctrl;
                    break;
                case S3_SPI_CLK_GATE_REG:
                    r = s->clock_gate;
                    break;
                case S3_SPI_USER_REG:
                    r = s->mem_user;
                    break;
                case S3_SPI_MISC_REG:
                    r = s->misc;
                    break;
                case S3_SPI_CTRL_REG:
                    r = s->mem_ctrl;
                    break;
                case S3_SPI_MS_DLEN_REG:
                    r = s->mem_miso_len;
                    r = s->mem_mosi_len;
                    break;
                case S3_SPI_CLOCK_REG:
                    r = s->mem_clock;
                    break;
                case S3_SPI_W0_REG...S3_SPI_W15_REG:
                    r = s->data_reg[(addr - S3_SPI_W0_REG) / sizeof(uint32_t)];
                    break;
                default:
//#if SPI1_WARNING
                    warn_report("[SPI3] Unsupported read to 0x%lx", addr);
//#endif
                    break;
            }
        } break;

        default:
            r = 0;
            break;
    }
    return r;
}

/* fixed tx/rx helper: iterate by index and use i to compare against tx_bytes/rx_bytes */
static void esp32s3_spi_txrx_buffer(ESP32S3SpiState *s,
                                    const void *tx, int tx_bytes,
                                    void *rx, int rx_bytes)
{
    const uint8_t *txb = (const uint8_t *)tx;
    uint8_t *rxb = (uint8_t *)rx;

    int bytes = MAX(tx_bytes, rx_bytes);
    for (int i = 0; i < bytes; ++i) {
        uint8_t out = 0;

        /* if we have MOSI data for this index, take it; otherwise send 0 (clock) */
        if (i < tx_bytes && txb) {
            out = txb[i];
        }

        uint32_t res = ssi_transfer(s->spi, out);

        /* ssi_transfer returns a 32-bit result; take bottom byte for MISO */
        uint8_t in = (uint8_t)(res & 0xFF);

        if (i < rx_bytes && rxb) {
            rxb[i] = in;
        }
    }
}


static void esp32s3_spi_dummy_cycles(ESP32S3SpiState *s, uint32_t dummy_bytes) {
    for (int i = 0; i < dummy_bytes; i++) {
        ssi_transfer(s->spi, 0);
    }
}

static void esp32s3_spi_cs_set(ESP32S3SpiState *s, int value)
{
    int cs0_dis = FIELD_EX32(s->misc, SPI_MEM_MISC, CS0_DIS);
    int cs1_dis = FIELD_EX32(s->misc, SPI_MEM_MISC, CS1_DIS);

    if (!cs0_dis) {
        qemu_set_irq(s->cs_gpio[0], value ? 1 : 0);
    } else {
        qemu_set_irq(s->cs_gpio[0], 1);
    }

    if (!cs1_dis) {
        qemu_set_irq(s->cs_gpio[1], value ? 1 : 0);
    } else {
        qemu_set_irq(s->cs_gpio[1], 1);
    }
}


/* helper: copy bytes from data_reg (uint32_t words) into a byte buffer */
static void copy_data_reg_bytes(ESP32S3SpiState *s, uint8_t *dst, unsigned nbytes)
{
    uint8_t *src = (uint8_t *)s->data_reg; /* little-endian host memory layout */
    for (unsigned i = 0; i < nbytes; ++i) {
        dst[i] = src[i];
    }
}

/* perform transaction: sends cmd/addr/dummy/tx (MSB-first for cmd/addr), then reads rx */
static void esp32s3_spi_perform_transaction(ESP32S3SpiState *s, ESP32S3SpiTransaction *t)
{
    /* assert CS active */
    esp32s3_spi_cs_set(s, 0);

    /* --- send command (MSB-first) --- */
    if (t->cmd_bytes > 0) {
        uint8_t cmdbuf[8]; /* sufficient for typical small cmd lengths */
        for (unsigned i = 0; i < t->cmd_bytes; ++i) {
            unsigned shift = (t->cmd_bytes - 1 - i) * 8;
            cmdbuf[i] = (uint8_t)((t->cmd >> shift) & 0xFF);
        }
        esp32s3_spi_txrx_buffer(s, cmdbuf, t->cmd_bytes, NULL, 0);
    }

    /* --- send address (MSB-first) --- */
    if (t->addr_bytes > 0) {
        uint8_t addrbuf[8];
        for (unsigned i = 0; i < t->addr_bytes; ++i) {
            unsigned shift = (t->addr_bytes - 1 - i) * 8;
            addrbuf[i] = (uint8_t)((t->addr >> shift) & 0xFF);
        }
        esp32s3_spi_txrx_buffer(s, addrbuf, t->addr_bytes, NULL, 0);
    }

    /* --- dummy cycles (send zeros for dummy bytes) --- */
    if (t->dummy_bytes > 0) {
        /* allocate on stack if small, otherwise loop with a zero-byte */
        uint8_t zero = 0;
        for (unsigned i = 0; i < t->dummy_bytes; ++i) {
            esp32s3_spi_txrx_buffer(s, &zero, 1, NULL, 0);
        }
    }

    /* --- MOSI data: copy from t->data (if provided) into a byte buffer and send --- */
    if (t->tx_bytes > 0) {
        uint8_t *txbuf = g_malloc0(t->tx_bytes);
        if (t->data) {
            /* expect t->data points into s->data_reg or a similar word-array; copy bytes */
            copy_data_reg_bytes(s, txbuf, t->tx_bytes);
        }
        esp32s3_spi_txrx_buffer(s, txbuf, t->tx_bytes, NULL, 0);
        g_free(txbuf);
    }

    /* --- MISO (rx): clock zeros and capture into t->data (if provided) --- */
    if (t->rx_bytes > 0) {
        /* we send zeros while reading; store received bytes into s->data_reg memory layout */
        uint8_t *rx_target = NULL;
        if (t->data) {
            rx_target = (uint8_t *)t->data; /* typically s->data_reg */
        } else {
            /* if caller didn't supply data pointer but expects rx, use s->data_reg */
            rx_target = (uint8_t *)s->data_reg;
        }
        /* send zeros and fill rx_target */
        uint8_t *clock_buf = g_malloc0(t->rx_bytes);
        esp32s3_spi_txrx_buffer(s, clock_buf, t->rx_bytes, rx_target, t->rx_bytes);
        g_free(clock_buf);
    }

    /* deactivate CS */
    qemu_set_irq(s->cs_gpio[0], 1);
    esp32s3_spi_cs_set(s, 1);
}


static inline void esp32s3_spi_get_addr(ESP32S3SpiState *s, uint32_t* addr, uint32_t* len)
{
    const uint32_t address = FIELD_EX32(s->mem_addr, SPI_MEM_ADDR, USR_ADDR_VALUE);
    /* SPI Flash expects the address to be sent with MSB first. We make the assumption that
     * the host computer uses a little-endian CPU. */
    *addr = bswap32(address);

    const uint32_t address_len = FIELD_EX32(s->mem_user1, SPI_MEM_USER1, USR_ADDR_BITLEN);
    *len = (address_len + 1) / 8;
}

static inline void esp32s3_spi_get_dummy(ESP32S3SpiState *s, uint32_t* len)
{
    const uint32_t dummy_count = FIELD_EX32(s->mem_user1, SPI_MEM_USER1, USR_DUMMY_CYCLELEN);

    /* Dummy cycles are interpreted as bytes by the emulated SPI Flash. As such, we shall convert
     * our dummy cycles count in bytes, rounding it up. For example:
     * 0 cycles = 0 byte
     * 1 cycle = 1 byte
     * ...
     * 8 cycles = 1 byte
     * 9 cycles = 2 bytes
     * etc..
     */
    *len = (dummy_count + 7) / 8;
}

/* Put this helper near other debug helpers in esp32s3_spi.c */
static void debug_print_bytes(const char *prefix, const uint8_t *buf, unsigned len)
{
    if (!len) {
        info_report("%s: <zero length>", prefix);
        return;
    }
    char tmp[256];
    int off = 0;
    off += snprintf(tmp + off, sizeof(tmp) - off, "%s:", prefix);
    for (unsigned i = 0; i < len && off < (int)sizeof(tmp) - 4; ++i) {
        off += snprintf(tmp + off, sizeof(tmp) - off, " %02x", buf[i]);
    }
    info_report("%s", tmp);
}

/* Drop-in replacement: explicit builder that special-cases SPI1 (flash)
 * but for SPI2/SPI3 streams W0..W15 (data_reg) for MOSI. */
static void esp32s3_spi_begin_transaction(ESP32S3SpiState *s)
{
    // SPI1 = flash: keep old logic
    if (s->spi_num == 1) {
        ESP32S3SpiTransaction t = {0};

        t.data = s->data_reg;
        if (s->mem_user & R_SPI_MEM_USER_USR_MOSI_MASK) {
            t.tx_bytes = (FIELD_EX32(s->mem_mosi_len, SPI_MEM_MOSI_DLEN, USR_MOSI_DBITLEN) + 1) / 8;
        }
        if (s->mem_user & R_SPI_MEM_USER_USR_MISO_MASK) {
            t.rx_bytes = (FIELD_EX32(s->mem_miso_len, SPI_MEM_MISO_DLEN, USR_MISO_DBITLEN) + 1) / 8;
        }

        t.cmd = FIELD_EX32(s->mem_user2, SPI_MEM_USER2, USR_COMMAND_VALUE);
        t.cmd_bytes = (FIELD_EX32(s->mem_user2, SPI_MEM_USER2, USR_COMMAND_BITLEN) + 1) / 8;

        if (s->mem_user & R_SPI_MEM_USER_USR_ADDR_MASK) {
            esp32s3_spi_get_addr(s, &t.addr, &t.addr_bytes);
            if (s->mem_user & R_SPI_MEM_USER_USR_DUMMY_MASK) {
                esp32s3_spi_get_dummy(s, &t.dummy_bytes);
            }
        }

        esp32s3_spi_perform_transaction(s, &t);
        return;
    }

    // SPI2/SPI3 = peripheral buses (ST7789, etc.)
    const int max_bytes = ESP32S3_SPI_BUF_WORDS * 4;
    uint8_t *data_bytes = (uint8_t *)s->data_reg;

    int tx_bytes = 0;
    if (s->mem_user & R_SPI_MEM_USER_USR_MOSI_MASK) {
        tx_bytes = (FIELD_EX32(s->mem_mosi_len, SPI_MEM_MOSI_DLEN, USR_MOSI_DBITLEN) + 1) / 8;
    }
    if (tx_bytes == 0) {
        // fallback: scan for last non-zero word
        for (int i = max_bytes - 1; i >= 0; --i) {
            if (data_bytes[i] != 0) {
                tx_bytes = i + 1;
                break;
            }
        }
    }

    if (tx_bytes == 0) {
        return; // nothing to send
    }

    // assert CS
    esp32s3_spi_cs_set(s, 0);

    // send all TX bytes in order (LSB-first per word)
    for (int i = 0; i < tx_bytes; i++) {
        ssi_transfer(s->spi, data_bytes[i]);
    }

    // deassert CS
    esp32s3_spi_cs_set(s, 1);
}


static void esp32s3_spi_special_command(ESP32S3SpiState *s, uint32_t command)
{
    ESP32S3SpiTransaction t= {
        .cmd_bytes = 1
    };

    switch (command >> 19 << 19) {
        case R_SPI_MEM_CMD_FLASH_READ_MASK:
            t.cmd = CMD_READ;
            esp32s3_spi_get_addr(s, &t.addr, &t.addr_bytes);
            t.addr = t.addr >> (32 - t.addr_bytes * 8);
            t.data = s->data_reg;
            t.rx_bytes = (FIELD_EX32(s->mem_miso_len, SPI_MEM_MISO_DLEN, USR_MISO_DBITLEN) + 1) / 8;
            break;

        case R_SPI_MEM_CMD_FLASH_WREN_MASK:
            t.cmd = CMD_WREN;
            break;

        case R_SPI_MEM_CMD_FLASH_WRDI_MASK:
            t.cmd = CMD_WRDI;
            break;

        case R_SPI_MEM_CMD_FLASH_RDID_MASK:
            t.cmd = CMD_RDID;
            t.data = s->data_reg;
            t.rx_bytes = 3;
            break;

        case R_SPI_MEM_CMD_FLASH_RDSR_MASK:
            t.cmd = CMD_RDSR;
            t.data = &s->mem_rd_st;
            t.rx_bytes = 1;
            break;

        case R_SPI_MEM_CMD_FLASH_WRSR_MASK:
            t.cmd = CMD_WRSR;
            t.data = &s->mem_rd_st;
            t.tx_bytes = 1;
            break;

        case R_SPI_MEM_CMD_FLASH_PP_MASK:
            t.cmd = CMD_PP;
            t.data = s->data_reg;
            esp32s3_spi_get_addr(s, &t.addr, &t.addr_bytes);
            /* The number of bytes to process is in the upper-byte of address */
            t.tx_bytes = (s->mem_addr >> 24) & 0xff;
            /**
             * Page program expects a 24-bit page address, if the one written in mem_addr was
             * 0xNN_33_00_02 (where is "do not care"), after calling `esp32s3_spi_get_addr`, the
             * address becomes 0x02_00_33_NN. Thus, if we cast it to a byte array, arr[0] would give
             * `NN`, instead of `33`. We need to adjust the value in address.
             */
            t.addr = t.addr >> 8;
            break;

        case R_SPI_MEM_CMD_FLASH_SE_MASK:
            t.cmd = CMD_SE;
            esp32s3_spi_get_addr(s, &t.addr, &t.addr_bytes);
            /* For the same reasons as explained above, we need to adjust `t.addr`, but here, the shift
             * to perform is not fixed and depends on the address length */
            t.addr = t.addr >> (32 - t.addr_bytes * 8);
            break;

        case R_SPI_MEM_CMD_FLASH_BE_MASK:
            t.cmd = CMD_BE;
            esp32s3_spi_get_addr(s, &t.addr, &t.addr_bytes);
            t.addr = t.addr >> (32 - t.addr_bytes * 8);
            break;

        case R_SPI_MEM_CMD_FLASH_CE_MASK:
            t.cmd = CMD_CE;
            break;

        case R_SPI_MEM_CMD_FLASH_DP_MASK:
            t.cmd = CMD_DP;
            break;

        case R_SPI_MEM_CMD_FLASH_RES_MASK:
            t.cmd = CMD_RES;
            t.data = s->data_reg;
            t.rx_bytes = 3;
            break;

        case R_SPI_MEM_CMD_FLASH_HPM_MASK:
            t.cmd = CMD_HPM;
            /* HPM needs 24 dummy cycles, so sent 3 random bytes */
            t.data = s->data_reg;
            t.rx_bytes = 3;
            break;

        default:
#if SPI1_WARNING
            warn_report("[SPI1] Unsupported special command %x", command);
#endif
            return;
    }
    esp32s3_spi_perform_transaction(s, &t);
}

static void esp32s3_spi_write(void *opaque, hwaddr addr,
                       uint64_t value, unsigned int size)
{
    ESP32S3SpiState *s = ESP32S3_SPI(opaque);
    uint32_t wvalue = (uint32_t) value;


    switch (s->spi_num) {
        case 1: {
            #if SPI1_DEBUG
                info_report("[SPI1] Writing 0x%lx = %08lx", addr, value);
            #endif

            switch (addr) {
                case A_SPI_MEM_CMD:
                    if(wvalue & R_SPI_MEM_CMD_USR_MASK) {
                        esp32s3_spi_begin_transaction(s);
                    } else {
                        esp32s3_spi_special_command(s, wvalue);
                    }
                    break;
                case A_SPI_MEM_ADDR:
                    s->mem_addr = wvalue;
                    break;
                case A_SPI_MEM_CTRL:
                    s->mem_ctrl = wvalue;
                    break;
                case A_SPI_MEM_CTRL1:
                    s->mem_ctrl1 = wvalue;
                    break;
                case A_SPI_MEM_CTRL2:
                    s->mem_ctrl2 = wvalue;
                    break;
                case A_SPI_MEM_CLOCK:
                    s->mem_clock = wvalue;
                    break;
                case A_SPI_MEM_USER:
                    s->mem_user = wvalue;
                    break;
                case A_SPI_MEM_USER1:
                    s->mem_user1 = wvalue;
                    break;
                case A_SPI_MEM_USER2:
                    s->mem_user2 = wvalue;
                    break;
                case A_SPI_MEM_MISO_DLEN:
                    s->mem_miso_len = wvalue;
                    break;
                case A_SPI_MEM_MOSI_DLEN:
                    s->mem_mosi_len = wvalue;
                    break;
                case A_SPI_MEM_RD_STATUS:
                    s->mem_rd_st = wvalue;
                    break;
                case A_SPI_MEM_MISC:
                    s->misc = wvalue;
                    break;
                case A_SPI_MEM_CACHE_FCTRL:
                    s->cache_fctrl = wvalue;
                    break;
                case A_SPI_MEM_W0...A_SPI_MEM_W15:
                    s->data_reg[(addr - A_SPI_MEM_W0) / sizeof(uint32_t)] = wvalue;
                    break;
                case A_SPI_MEM_SUS_STATUS:
                    s->mem_sus_st = wvalue;
                    break;
                case A_SPI_MEM_DDR_CTRL:
                    s->ddr_ctrl = wvalue;
                    break;
                case A_SPI_MEM_CLOCK_GATE:
                    s->clock_gate = wvalue;
                    break;
                default:
        #if SPI1_WARNING
                    warn_report("[SPI1] Unsupported write to 0x%lx (%08lx)", addr, value);
        #endif
                    break;
            }
        } break;
        case 3: {
            //warn_report("[SPI3] detected");
            //info_report("[SPI3] Writing 0x%lx = %08lx", addr, value);

            switch (addr) {
                case S3_SPI_CMD_REG:
                    esp32s3_spi_begin_transaction(s);
                    break;
                case S3_SPI_MISC_REG:
                    s->misc = wvalue;
                    break;
                case S3_SPI_USER_REG:
                    s->mem_user = wvalue;
                    break;
                case S3_SPI_USER1_REG:
                    s->mem_user1 = wvalue;
                    break;
                case S3_SPI_USER2_REG:
                    s->mem_user2 = wvalue;
                    break;
                case S3_SPI_CTRL_REG:
                    s->mem_ctrl = wvalue;
                    break;
                case S3_SPI_CLK_GATE_REG:
                    s->clock_gate = wvalue;
                    break;
                case S3_SPI_DMA_CONF_REF:
                    s->ddr_ctrl = wvalue;
                    break;
                case S3_SPI_SLAVE_REG:
                    s->slave_reg = wvalue;
                    if (wvalue & S3_SPI_SLAVE_TRANS_START) {
                        esp32s3_spi_begin_transaction(s);
                    }
                    break;
                case S3_SPI_MS_DLEN_REG:
                    s->mem_miso_len = wvalue;
                    s->mem_mosi_len = wvalue;
                    break;
                case S3_SPI_CLOCK_REG:
                    s->mem_clock = wvalue;
                    break;
                case S3_SPI_W0_REG...S3_SPI_W15_REG:
                    s->data_reg[(addr - S3_SPI_W0_REG) / sizeof(uint32_t)] = wvalue;
                    //ssi_transfer(s->spi, wvalue);
                    break;
                default:
        //#if SPI1_WARNING
                    warn_report("[SPI3] Unsupported write to 0x%lx (%08lx)", addr, value);
        //#endif
                    break;
            }
        } break;
        default:
            break;

    }

}


static const MemoryRegionOps esp32s3_spi_ops = {
    .read =  esp32s3_spi_read,
    .write = esp32s3_spi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void esp32s3_spi_reset_hold(Object *obj, ResetType type)
{
    ESP32S3SpiState *s = ESP32S3_SPI(obj);
    memset(s->data_reg, 0, ESP32S3_SPI_BUF_WORDS * sizeof(uint32_t));
    s->mem_ctrl1 = FIELD_DP32(s->mem_ctrl1, SPI_MEM_CTRL1, CS_HOLD_DLY_RES, 0x3ff);
    s->mem_clock = FIELD_DP32(s->mem_clock, SPI_MEM_CLOCK, CLKCNT_N, 3);
    s->mem_clock = FIELD_DP32(s->mem_clock, SPI_MEM_CLOCK, CLKCNT_H, 1);
    s->mem_clock = FIELD_DP32(s->mem_clock, SPI_MEM_CLOCK, CLKCNT_L, 3);

    s->mem_user = FIELD_DP32(s->mem_user, SPI_MEM_USER, USR_COMMAND, 1);

    s->mem_user1 = FIELD_DP32(s->mem_user1, SPI_MEM_USER1, USR_ADDR_BITLEN, 23);
    s->mem_user1 = FIELD_DP32(s->mem_user1, SPI_MEM_USER1, USR_DUMMY_CYCLELEN, 7);

    s->mem_user2 = FIELD_DP32(s->mem_user2, SPI_MEM_USER2, USR_COMMAND_BITLEN, 7);
    s->slave_reg = 0;
    /* In case more registers are supported in the future (MEM_MISC, MEM_TX_CRC, ...)
     * update this function with their default values */
}

static void esp32s3_spi_realize(DeviceState *dev, Error **errp)
{
    ESP32S3SpiState *s = ESP32S3_SPI(dev);

    /* Make sure XTS_AES was set or issue an error */
    if (s->xts_aes == NULL) {
        // error_report("[SPI1] XTS_AES controller must be set!");
    }

}

static void esp32s3_spi_init(Object *obj)
{
    ESP32S3SpiState *s = ESP32S3_SPI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &esp32s3_spi_ops, s,
                          TYPE_ESP32S3_SPI, ESP32S3_SPI_IO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    // sysbus_init_irq(sbd, &s->irq);

    esp32s3_spi_reset_hold(obj, RESET_TYPE_COLD);

    s->spi = ssi_create_bus(DEVICE(s), "spi");
    qdev_init_gpio_out_named(DEVICE(s), &s->cs_gpio[0], SSI_GPIO_CS, ESP32S3_SPI_CS_COUNT);
}

static Property esp32s3_spi_properties[] = {
    DEFINE_PROP_UINT32("spi-num", ESP32S3SpiState, spi_num, 1),
    DEFINE_PROP_END_OF_LIST(),
};

static void esp32s3_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = esp32s3_spi_reset_hold;
    dc->realize = esp32s3_spi_realize;
    device_class_set_props(dc, esp32s3_spi_properties);
}

static const TypeInfo esp32s3_spi_info = {
    .name = TYPE_ESP32S3_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ESP32S3SpiState),
    .instance_init = esp32s3_spi_init,
    .class_init = esp32s3_spi_class_init
};

static void esp32s3_spi_register_types(void)
{
    type_register_static(&esp32s3_spi_info);
}

type_init(esp32s3_spi_register_types)
