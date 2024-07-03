/*
 * This file is part of the libusbhost library
 * hosted at http://github.com/libusbhost/libusbhost
 *
 * Copyright (C) 2015 Amir Hammad <amir.hammad@hotmail.com>
 *
 *
 * libusbhost is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>

#include "sr_global.h"

struct dma_tx_buf {
    /* The following fields are accessed only from DMA ISR */
    ssize_t xfr_start; /* Start index of running DMA transfer */
    ssize_t xfr_end; /* End index of running DMA transfer plus one */
    bool wraparound;
    ssize_t xfr_next;

    /* The following fields are written only from non-interrupt code */
    ssize_t wr_pos; /* Next index to be written */
    ssize_t wr_idx;
    ssize_t chunk_end[8];

/* Make GCC shut up about the zero-size array member. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    /* Written outside ISR by usart_send_chunk_nonblocking, read via DMA */
    uint8_t data[0];
#pragma GCC diagnostic pop
};

struct usart_desc {
    struct dma_tx_buf tx_buf;
    uint8_t data[512];

    uint32_t tx_chunk_overruns, tx_byte_overruns;
    uint32_t tx_errors;

    volatile uint8_t rx_buf[32];

    int comm_led;

    USART_TypeDef *le_usart;
    int le_usart_irqn;
    DMA_Stream_TypeDef *tx_dmas;
    int tx_dma_sn;
    int tx_dma_ch;
    DMA_TypeDef *tx_dma;
    int tx_dma_irqn;
};

void usart_dma_init(volatile struct usart_desc *us, unsigned int baudrate);
int usart_send_chunk_nonblocking(volatile struct usart_desc *us, const char *chunk, size_t chunk_len);
int usart_putc_nonblocking(volatile struct usart_desc *us, char c);
int usart_putc_blocking(volatile struct usart_desc *us, char c);

void usart_dma_stream_irq(volatile struct usart_desc *us);
int usart_flush(volatile struct usart_desc *us);
int usart_printf(volatile struct usart_desc *us, const char *fmt, ...);
int usart_printf_blocking(volatile struct usart_desc *us, const char *fmt, ...);
int usart_printf_blocking_va(volatile struct usart_desc *us, const char *fmt, va_list va);

#endif // __SERIAL_H__
