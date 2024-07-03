#ifndef __GPIO_HELPERS_H__
#define __GPIO_HELPERS_H__

#include <stm32f407xx.h>

void gpio_pin_mode(GPIO_TypeDef *gpio, int pin, int mode);
void gpio_pin_setup(GPIO_TypeDef *gpio, int pin, int mode, int speed, int pullups, int afsel);
void gpio_pin_output(GPIO_TypeDef *gpio, int pin, int speed);
void gpio_pin_input(GPIO_TypeDef *gpio, int pin, int pullups);
void gpio_pin_af(GPIO_TypeDef *gpio, int pin, int speed, int pullups, int afsel);
void gpio_pin_analog(GPIO_TypeDef *gpio, int pin);
void gpio_pin_tristate(GPIO_TypeDef *gpio, int pin, int tristate);

#endif /* __GPIO_HELPERS_H__ */
