
#include <assert.h>

#include "dma_util.h"

uint8_t dma_get_isr_and_clear(DMA_TypeDef *dma, int ch) {
    uint8_t isr_val;
    switch(ch) {
        case 0:
            isr_val = dma->LISR & 0x3f; 
            dma->LIFCR = isr_val;
            return isr_val;
        case 1:
            isr_val = dma->LISR>>6 & 0x3f; 
            dma->LIFCR = isr_val<<6;
            return isr_val;
        case 2:
            isr_val = dma->LISR>>16 & 0x3f; 
            dma->LIFCR = isr_val<<16;
            return isr_val;
        case 3:
            isr_val = dma->LISR>>6>>16 & 0x3f; 
            dma->LIFCR = isr_val<<6<<16;
            return isr_val;
        case 4:
            isr_val = dma->HISR & 0x3f; 
            dma->HIFCR = isr_val;
            return isr_val;
        case 5:
            isr_val = dma->HISR>>6 & 0x3f; 
            dma->HIFCR = isr_val<<6;
            return isr_val;
        case 6:
            isr_val = dma->HISR>>16 & 0x3f; 
            dma->HIFCR = isr_val<<16;
            return isr_val;
        case 7:
            isr_val = dma->HISR>>6>>16 & 0x3f; 
            dma->HIFCR = isr_val<<6<<16;
            return isr_val;
        default:
            assert(0);
            return 0;
    }
}

