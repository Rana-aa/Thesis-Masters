#include "uart.h"

#define GPIOAEN  (1U<<0)
#define UART1EN  (1U<<4)
#define CR1_TE    (1U<<3)
#define CR1_UE    (1U<<13)
#define SR_TXE    (1U<<7)

#define SYS_FREQ   16000000
#define APB1_CLK   SYS_FREQ

#define UART_BAUDRATE 115200

static void uart1_set_baudrate(uint32_t periph_clk, uint32_t baudrate);
void uart1_write(int ch);

int __io_putchar(int ch)
{
	uart1_write(ch);

	return ch;
}

void uart1_tx_init(void)
{
	/***** Configure UART GPIO Pin *****/
	/*Enable clock access to GPIOA*/
	RCC-> AHB1ENR |= GPIOAEN;

	/*Set PA9 mode to alternate function mode*/
	GPIOA->MODER &=~(1U<<18);
	GPIOA->MODER |= (1U<<19);

	/*Set PA9 alternate function type to UART_TX(AF07)*/
	GPIOA->AFR[1] |= (1U<<4);
	GPIOA->AFR[1] |= (1U<<5);
	GPIOA->AFR[1] |= (1U<<6);
	GPIOA->AFR[1] &=~(1U<<7);

	/***** Configure UART *****/
	/*Enable clock access to UART1*/
	RCC-> APB2ENR |= UART1EN;


	/*Configure BaudRate*/
	uart1_set_baudrate(APB1_CLK,UART_BAUDRATE);


	/*Configure the transfer direction*/
	USART1->CR1 = CR1_TE;


	/*Enable UART module*/
	USART1->CR1 |= CR1_UE;

}

void uart1_write(int ch)
{
	/*Make sure transmit data register is empty*/
	while(!(USART1->SR & SR_TXE)){}

	/*Write to the transmit data register*/
	USART1->DR = (ch &0xFF);
}

static uint16_t compute_uart_bd(uint32_t periph_clk, uint32_t baudrate)
{
	return ((periph_clk + (baudrate/2U))/baudrate);
}

static void uart1_set_baudrate(uint32_t periph_clk, uint32_t baudrate)
{
   USART1->BRR = compute_uart_bd(periph_clk,baudrate);
}

