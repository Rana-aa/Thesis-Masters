#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "dma_util.h"
#include "sr_global.h"
#include "serial.h"

#include <tinyprintf.h>

static void usart_schedule_dma(volatile struct usart_desc *us);
static void usart_dma_reset(volatile struct usart_desc *us);
static void usart_putc_nonblocking_tpf(void *us, char c);
static void usart_wait_chunk_free(volatile struct usart_desc *us);
static void usart_putc_blocking_tpf(void *us, char c);

void usart_dma_reset(volatile struct usart_desc *us) {
    us->tx_buf.xfr_start = -1;
    us->tx_buf.xfr_end = 0;
    us->tx_buf.wr_pos = 0;
    us->tx_buf.wr_idx = 0;
    us->tx_buf.xfr_next = 0;
    us->tx_buf.wraparound = false;

    for (size_t i=0; i<ARRAY_LENGTH(us->tx_buf.chunk_end); i++)
        us->tx_buf.chunk_end[i] = -1;
}

void usart_dma_init(volatile struct usart_desc *us, unsigned int baudrate) {
    usart_dma_reset(us);

    /* Configure DMA 1 Channel 2 to handle uart transmission */
    us->tx_dmas->PAR = (uint32_t)&(us->le_usart->DR);
    us->tx_dmas->CR =
          (us->tx_dma_ch<<DMA_SxCR_CHSEL_Pos)
        | (0<<DMA_SxCR_PL_Pos)
        | (1<<DMA_SxCR_DIR_Pos)
        | (0<<DMA_SxCR_MSIZE_Pos) /* 8 bit */
        | (0<<DMA_SxCR_PSIZE_Pos) /* 8 bit */
        | DMA_SxCR_MINC
        | DMA_SxCR_TCIE; /* Enable transfer complete interrupt. */

    /* triggered on transfer completion. We use this to process the ADC data */
    NVIC_EnableIRQ(us->tx_dma_irqn);
    NVIC_SetPriority(us->tx_dma_irqn, 30);

    us->le_usart->CR1 = USART_CR1_TE;
    /* Set divider for 115.2kBd @48MHz system clock. */
    us->le_usart->BRR = apb2_speed * 16 / baudrate / 16; /* 250kBd */
    us->le_usart->CR3 |= USART_CR3_DMAT; /* TX DMA enable */

    /* And... go! */
    us->le_usart->CR1 |= USART_CR1_UE;
}

void usart_schedule_dma(volatile struct usart_desc *us) {
    volatile struct dma_tx_buf *buf = &us->tx_buf;

    ssize_t xfr_start, xfr_end, xfr_len;
    if (buf->wraparound) {
        buf->wraparound = false;
        xfr_start = 0;
        xfr_len = buf->xfr_end;
        xfr_end = buf->xfr_end;

    } else {
        if (buf->chunk_end[buf->xfr_next] == -1)
            return; /* Nothing to trasnmit */

        xfr_start = buf->xfr_end;
        xfr_end = buf->chunk_end[buf->xfr_next];
        buf->chunk_end[buf->xfr_next] = -1;
        buf->xfr_next = (buf->xfr_next + 1) % ARRAY_LENGTH(buf->chunk_end);

        if (xfr_end > xfr_start) { /* no wraparound */
            xfr_len = xfr_end - xfr_start;

        } else { /* wraparound */
            if (xfr_end != 0)
                buf->wraparound = true;
            xfr_len = sizeof(us->data) - xfr_start;
        }
    }

    buf->xfr_start = xfr_start;
    buf->xfr_end = xfr_end;

    us->comm_led = 100;

    /* initiate transmission of new buffer */
    us->tx_dmas->M0AR = (uint32_t)(us->data + xfr_start);
    us->tx_dmas->NDTR = xfr_len;
    us->tx_dmas->CR |= DMA_SxCR_EN;
}

void usart_dma_stream_irq(volatile struct usart_desc *us) {
    uint8_t iflags = dma_get_isr_and_clear(us->tx_dma, us->tx_dma_sn);

    if (iflags & DMA_LISR_TCIF0) { /* Transfer complete */
        us->tx_dmas->CR &= ~DMA_SxCR_EN;
        //if (us->tx_buf.wraparound)
        usart_schedule_dma(us);
    }

    if (iflags & DMA_LISR_FEIF0)
        us->tx_errors++;
}

int usart_putc_nonblocking(volatile struct usart_desc *us, char c) {
    volatile struct dma_tx_buf *buf = &us->tx_buf;

    if (buf->wr_pos == buf->xfr_start) {
        us->tx_byte_overruns++;
        return -EBUSY;
    }

    buf->data[buf->wr_pos] = c;
    buf->wr_pos = (buf->wr_pos + 1) % sizeof(us->data);
    return 0;
}

int usart_putc_blocking(volatile struct usart_desc *us, char c) {
    volatile struct dma_tx_buf *buf = &us->tx_buf;

    while (buf->wr_pos == buf->xfr_start)
        ;

    buf->data[buf->wr_pos] = c;
    buf->wr_pos = (buf->wr_pos + 1) % sizeof(us->data);
    return 0;
}

void usart_putc_nonblocking_tpf(void *us, char c) {
    usart_putc_nonblocking((struct usart_desc *)us, c);
}

void usart_putc_blocking_tpf(void *us, char c) {
    usart_putc_blocking((struct usart_desc *)us, c);
}

int usart_send_chunk_nonblocking(volatile struct usart_desc *us, const char *chunk, size_t chunk_len) {
    for (size_t i=0; i<chunk_len; i++)
        usart_putc_nonblocking(us, chunk[i]);

    return usart_flush(us);
}

void usart_wait_chunk_free(volatile struct usart_desc *us) {
    while (us->tx_buf.chunk_end[us->tx_buf.wr_idx] != -1)
        ;
}

int usart_flush(volatile struct usart_desc *us) {
    /* Find a free slot for this chunk */
    if (us->tx_buf.chunk_end[us->tx_buf.wr_idx] != -1) {
        us->tx_chunk_overruns++;
        return -EBUSY;
    }

    us->tx_buf.chunk_end[us->tx_buf.wr_idx] = us->tx_buf.wr_pos;
    us->tx_buf.wr_idx = (us->tx_buf.wr_idx + 1) % ARRAY_LENGTH(us->tx_buf.chunk_end);

    if (!(us->tx_dmas->CR & DMA_SxCR_EN))
        usart_schedule_dma(us);
    return 0;
}

int usart_printf(volatile struct usart_desc *us, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    tfp_format((void *)us, usart_putc_nonblocking_tpf, fmt, va);
    return usart_flush(us);
}

int usart_printf_blocking_va(volatile struct usart_desc *us, const char *fmt, va_list va) {
    tfp_format((void *)us, usart_putc_blocking_tpf, fmt, va);
    usart_wait_chunk_free(us);
    return usart_flush(us);
}

int usart_printf_blocking(volatile struct usart_desc *us, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    return usart_printf_blocking_va(us, fmt, va);
}

