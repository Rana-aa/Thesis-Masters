
#include "gpio_helpers.h"

void gpio_pin_mode(GPIO_TypeDef *gpio, int pin, int mode) {
    gpio->MODER &= ~(3 << (2*pin));
    gpio->MODER |= mode << (2*pin);
}

void gpio_pin_setup(GPIO_TypeDef *gpio, int pin, int mode, int speed, int pullups, int afsel) {
    int gpio_idx = ((uint32_t)gpio>>10) & 0xf;
    RCC->AHB1ENR |= 1<<gpio_idx;
    
    gpio->MODER &= ~(3 << (2*pin));
    gpio->MODER |= mode << (2*pin);
    gpio->OSPEEDR &= ~(3 << (2*pin));
    gpio->OSPEEDR |= speed << (2*pin);
    gpio->PUPDR &= ~(3 << (2*pin));
    gpio->PUPDR |= pullups << (2*pin);
    gpio->AFR[pin>>3] &= ~(0xf << (4*(pin&7)));
    gpio->AFR[pin>>3] |= afsel << (4*(pin&7));
    gpio->BSRR = 1<<pin<<16;
}

void gpio_pin_output(GPIO_TypeDef *gpio, int pin, int speed) {
    gpio_pin_setup(gpio, pin, 1, speed, 0, 0);
}

void gpio_pin_tristate(GPIO_TypeDef *gpio, int pin, int tristate) {
    if (tristate)
        gpio->MODER &= ~(3 << (2*pin));
    else
        gpio->MODER |= 1 << (2*pin);
}

void gpio_pin_input(GPIO_TypeDef *gpio, int pin, int pullups) {
    gpio_pin_setup(gpio, pin, 0, 0, pullups, 0);
}

void gpio_pin_af(GPIO_TypeDef *gpio, int pin, int speed, int pullups, int afsel) {
    gpio_pin_setup(gpio, pin, 2, speed, pullups, afsel);
}

void gpio_pin_analog(GPIO_TypeDef *gpio, int pin) {
    gpio_pin_setup(gpio, pin, 3, 0, 0, 0);
}

