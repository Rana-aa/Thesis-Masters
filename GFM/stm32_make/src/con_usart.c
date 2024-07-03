
#include <stm32f4_isr.h>
#include "con_usart.h"

volatile struct usart_desc con_usart = {
    .le_usart = USART1,
    .le_usart_irqn = USART1_IRQn,
    .tx_dmas = DMA2_Stream7,
    .tx_dma_sn = 7,
    .tx_dma_ch = 4,
    .tx_dma = DMA2,
    .tx_dma_irqn = DMA2_Stream7_IRQn,
};

void con_usart_init() {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* GPIO config: A9 (TX), A10 (RX) */
    GPIOA->MODER &= ~GPIO_MODER_MODER9_Msk & ~GPIO_MODER_MODER10_Msk;
    GPIOA->MODER |= (2<<GPIO_MODER_MODER9_Pos) | (2<<GPIO_MODER_MODER10_Pos);
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED9_Msk & ~GPIO_OSPEEDR_OSPEED10_Msk;
    GPIOA->OSPEEDR |= (2<<GPIO_OSPEEDR_OSPEED9_Pos) | (2<<GPIO_OSPEEDR_OSPEED10_Pos);
    GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL9_Msk & ~GPIO_AFRH_AFSEL10_Msk;
    GPIOA->AFR[1] |= (7<<GPIO_AFRH_AFSEL9_Pos) | (7<<GPIO_AFRH_AFSEL10_Pos);

    usart_dma_init(&con_usart, CON_USART_BAUDRATE);
}

void DMA2_Stream7_IRQHandler(void) {
    usart_dma_stream_irq(&con_usart);
}

