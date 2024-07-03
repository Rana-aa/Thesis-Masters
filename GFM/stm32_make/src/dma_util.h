#ifndef __DMA_UTIL_H__
#define __DMA_UTIL_H__

#include <stdint.h>

#include "sr_global.h"

uint8_t dma_get_isr_and_clear(DMA_TypeDef *dma, int ch);

#endif /* __DMA_UTIL_H__ */
