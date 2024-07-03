#ifndef __CON_USART_H__
#define __CON_USART_H__

#include "serial.h"

extern volatile struct usart_desc con_usart;

#define con_printf(...) usart_printf(&con_usart, __VA_ARGS__)
#define con_printf_blocking(...) usart_printf_blocking(&con_usart, __VA_ARGS__)

#ifndef CON_USART_BAUDRATE
#define CON_USART_BAUDRATE 500000
#endif

void con_usart_init(void);

#endif /* __CON_USART_H__ */
