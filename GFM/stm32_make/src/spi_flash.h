#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include <stdbool.h>
#include <unistd.h>

#include <stm32f407xx.h>

struct spi_mem_id {
    size_t size;
    uint8_t mfg_id;
    uint8_t type;
};

struct spi_flash_if {
    struct spi_mem_id id;
    volatile SPI_TypeDef *spi;
    size_t page_size;
    void (*cs)(bool val);
};

void spif_init(struct spi_flash_if *spif, size_t page_size, SPI_TypeDef *spi, void (*cs)(bool val));

void spif_write(struct spi_flash_if *spif, size_t addr, size_t len, const char* data);
ssize_t spif_read(struct spi_flash_if *spif, size_t addr, size_t len, char* data);

void spif_clear_mem(struct spi_flash_if *spif);
void spif_clear_sector(struct spi_flash_if *spif, size_t addr);

void spif_deep_power_down(struct spi_flash_if *spif);
void spif_wakeup(struct spi_flash_if *spif);

#endif /* __SPI_FLASH_H__ */
