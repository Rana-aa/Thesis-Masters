
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "adc.h"
#include "sr_global.h"

static unsigned int adc_overruns = 0;
uint16_t adc_fft_buf[2][FMEAS_FFT_LEN];
volatile int adc_fft_buf_ready_idx = -1;

static DMA_TypeDef *const adc_dma = DMA2;
static DMA_Stream_TypeDef *const adc_stream = DMA2_Stream0;
static const int dma_adc_channel = 0;
static const int adc_channel = 10;

/* Configure ADC1 to sample channel 0. Trigger from TIM1 CC0 every 1ms. Transfer readings into alternating buffers
 * throug DMA. Enable DMA interrupts.
 *
 * We have two full FFT buffers. We always transfer data from the ADC to the back half of the active one, while a
 * DMA memcpy'es the latter half of the inactive one to the front half of the active one. This means at the end of the
 * ADC's DMA transfer, in the now-inactive buffer that the ADC results were just written to we have last half-period's
 * data sitting in front of this half-period's data like so: [old_adc_data, new_adc_data]
 *
 * This means we can immediately start running an FFT on ADC DMA transfer complete interrupt.
 */
void adc_init() {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM1EN;

    /* PC0 -> ADC1.ch10 */
    GPIOC->MODER &= ~GPIO_MODER_MODER0_Msk;
    GPIOC->MODER |= (3<<GPIO_MODER_MODER0_Pos);

    adc_dma->LIFCR |= 0x3f;
    adc_stream->CR = 0; /* disable */
    while (adc_stream->CR & DMA_SxCR_EN)
        ; /* wait for stream to become available */
    adc_stream->NDTR = FMEAS_FFT_LEN/2; 
    adc_stream->PAR = (uint32_t) &(ADC1->DR);
    adc_stream->M0AR = (uint32_t) (adc_fft_buf[0] + FMEAS_FFT_LEN/2);
    adc_stream->M1AR = (uint32_t) (adc_fft_buf[1] + FMEAS_FFT_LEN/2);
    adc_stream->CR = (dma_adc_channel<<DMA_SxCR_CHSEL_Pos) | DMA_SxCR_DBM | (1<<DMA_SxCR_MSIZE_Pos)
        | (1<<DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC | (2<<DMA_SxCR_PL_Pos)
        | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    adc_stream->CR |= DMA_SxCR_EN;

    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    NVIC_SetPriority(DMA2_Stream0_IRQn, 128);

    ADC1->CR1 = (0<<ADC_CR1_RES_Pos) | (0<<ADC_CR1_DISCNUM_Pos) | ADC_CR1_DISCEN | (0<<ADC_CR1_AWDCH_Pos);
    ADC1->CR2 = (1<<ADC_CR2_EXTEN_Pos) | (0<<ADC_CR2_EXTSEL_Pos) | ADC_CR2_DMA | ADC_CR2_ADON | ADC_CR2_DDS;
    ADC1->SQR3 = (adc_channel<<ADC_SQR3_SQ1_Pos);
    ADC1->SQR1 = (0<<ADC_SQR1_L_Pos);
    ADC1->SMPR2 = (7<<ADC_SMPR2_SMP0_Pos);

    TIM1->CR2 = (2<<TIM_CR2_MMS_Pos); /* Enable update event on TRGO to provide a 1ms reference to rest of system */
    TIM1->CCMR1 = (6<<TIM_CCMR1_OC1M_Pos) | (0<<TIM_CCMR1_CC1S_Pos);
    TIM1->CCER = TIM_CCER_CC1E;
    assert(apb2_timer_speed%1000000 == 0);
    TIM1->PSC = 1000-1;
    TIM1->ARR = (apb2_timer_speed/1000000)-1; /* 1ms period */
    TIM1->CCR1 = 1;
    TIM1->BDTR = TIM_BDTR_MOE;
    
    TIM1->CR1 = TIM_CR1_CEN;
    TIM1->EGR = TIM_EGR_UG;
}

void DMA2_Stream0_IRQHandler(void) {
    uint8_t isr = (DMA2->LISR >> DMA_LISR_FEIF0_Pos) & 0x3f;
    GPIOA->BSRR = 1<<11;

    if (isr & DMA_LISR_TCIF0) { /* Transfer complete */
        /* Check we're done processing the old buffer */
        if (adc_fft_buf_ready_idx != -1) { /* FIXME DEBUG */
            GPIOA->BSRR = 1<<11<<16;
            /* clear all flags */
            adc_dma->LIFCR = isr<<DMA_LISR_FEIF0_Pos;
            adc_overruns++;
            return;
            panic();
        }

        /* Kickoff FFT */
        int ct = !!(adc_stream->CR & DMA_SxCR_CT);
        adc_fft_buf_ready_idx = !ct;
    }

    if (isr & DMA_LISR_DMEIF0) /* Direct mode error */
        panic();

    if (isr & DMA_LISR_TEIF0) /* Transfer error */
        panic();

    GPIOA->BSRR = 1<<11<<16;
    /* clear all flags */
    adc_dma->LIFCR = isr<<DMA_LISR_FEIF0_Pos;
}
