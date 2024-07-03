/* Library for SPI flash 25* devices.
 * Copyright (c) 2014 Multi-Tech Systems
 * Copyright (c) 2020 Jan Goette <ma@jaseg.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "spi_flash.h"

enum {
    WRITE_ENABLE                = 0x06,
    WRITE_DISABLE               = 0x04,
    READ_IDENTIFICATION         = 0x9F,
    READ_STATUS                 = 0x05,
    WRITE_STATUS                = 0x01,
    READ_DATA                   = 0x03,
    READ_DATA_FAST              = 0x0B,
    PAGE_PROGRAM                = 0x02,
    SECTOR_ERASE                = 0xD8,
    BULK_ERASE                  = 0xC7,
    DEEP_POWER_DOWN             = 0xB9,
    DEEP_POWER_DOWN_RELEASE     = 0xAB,
};

enum {
    STATUS_SRWD                 = 0x80,     // 0b 1000 0000
    STATUS_BP2                  = 0x10,     // 0b 0001 0000
    STATUS_BP1                  = 0x08,     // 0b 0000 1000
    STATUS_BP0                  = 0x04,     // 0b 0000 0100
    STATUS_WEL                  = 0x02,     // 0b 0000 0010
    STATUS_WIP                  = 0x01,     // 0b 0000 0001
};


static uint8_t spi_xfer(volatile SPI_TypeDef *spi, uint8_t b);
static uint8_t spi_read(struct spi_flash_if *spif);
static void spi_write(struct spi_flash_if *spif, uint8_t b);

static void spif_write_page(struct spi_flash_if *spif, size_t addr, size_t len, const char* data);
static uint8_t spif_read_status(struct spi_flash_if *spif);
static void spif_enable_write(struct spi_flash_if *spif);
static void spif_wait_for_write(struct spi_flash_if *spif);

#define low_byte(x) (x&0xff)
#define mid_byte(x) ((x>>8)&0xff)
#define high_byte(x) ((x>>16)&0xff)

uint8_t spi_xfer(volatile SPI_TypeDef *spi, uint8_t b) {
    while (!(spi->SR & SPI_SR_TXE))
        ;
    (void) spi->DR; /* perform dummy read to clear RXNE flag */
    spi->DR = b;
    while (!(spi->SR & SPI_SR_RXNE))
        ;
    return spi->DR;
}

uint8_t spi_read(struct spi_flash_if *spif) {
    return spi_xfer(spif->spi, 0);
}

void spi_write(struct spi_flash_if *spif, uint8_t b) {
    (void)spi_xfer(spif->spi, b);
}

void spif_init(struct spi_flash_if *spif, size_t page_size, SPI_TypeDef *spi, void (*cs)(bool val)) {

    spif->spi = spi;
    spif->page_size = page_size;
    spif->cs = cs;
    spif->cs(1);

    spi->CR1 = (0<<SPI_CR1_BR_Pos) | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_MSTR;

    spif->cs(0);
    spi_write(spif, READ_IDENTIFICATION);
    spif->id.mfg_id = spi_read(spif);
    spif->id.type = spi_read(spif);
    spif->id.size = 1<<spi_read(spif);
    spif->cs(1);
}

ssize_t spif_read(struct spi_flash_if *spif, size_t addr, size_t len, char* data) {
    spif_enable_write(spif);

    spif->cs(0);
    spi_write(spif, READ_DATA);
    spi_write(spif, high_byte(addr));
    spi_write(spif, mid_byte(addr));
    spi_write(spif, low_byte(addr));

    for (size_t i = 0; i < len; i++)
        data[i] = spi_read(spif);

    spif->cs(1);
    return len;
}

void spif_write(struct spi_flash_if *spif, size_t addr, size_t len, const char* data) {
    size_t written = 0, write_size = 0;

    while (written < len) {
        write_size = spif->page_size - ((addr + written) % spif->page_size);
        if (written + write_size > len)
            write_size = len - written;

        spif_write_page(spif, addr + written, write_size, data + written);
        written += write_size;
    }
}

static uint8_t spif_read_status(struct spi_flash_if *spif) {
    spif->cs(0);
    spi_write(spif, READ_STATUS);
    uint8_t status = spi_read(spif);
    spif->cs(1);

    return status;
}

void spif_clear_sector(struct spi_flash_if *spif, size_t addr) {
    spif_enable_write(spif);

    spif->cs(0);
    spi_write(spif, SECTOR_ERASE);
    spi_write(spif, high_byte(addr));
    spi_write(spif, mid_byte(addr));
    spi_write(spif, low_byte(addr));
    spif->cs(1);

    spif_wait_for_write(spif);
}

void spif_clear_mem(struct spi_flash_if *spif) {
    spif_enable_write(spif);

    spif->cs(0);
    spi_write(spif, BULK_ERASE);
    spif->cs(1);

    spif_wait_for_write(spif);
}

static void spif_write_page(struct spi_flash_if *spif, size_t addr, size_t len, const char* data) {
    spif_enable_write(spif);

    spif->cs(0);
    spi_write(spif, PAGE_PROGRAM);
    spi_write(spif, high_byte(addr));
    spi_write(spif, mid_byte(addr));
    spi_write(spif, low_byte(addr));

    for (size_t i = 0; i < len; i++) {
        spi_write(spif, data[i]);
    }

    spif->cs(1);
    spif_wait_for_write(spif);
}

static void spif_enable_write(struct spi_flash_if *spif) {
    spif->cs(0);
    spi_write(spif, WRITE_ENABLE);
    spif->cs(1);
}

static void spif_wait_for_write(struct spi_flash_if *spif) {
    while (spif_read_status(spif) & STATUS_WIP)
        for (int i = 0; i < 800; i++)
            ;
}

void spif_deep_power_down(struct spi_flash_if *spif) {
    spif->cs(0);
    spi_write(spif, DEEP_POWER_DOWN);
    spif->cs(1);
}

void spif_wakeup(struct spi_flash_if *spif) {
    spif->cs(0);
    spi_write(spif, DEEP_POWER_DOWN_RELEASE);
    spif->cs(1);
}

