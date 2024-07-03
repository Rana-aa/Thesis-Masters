#ifndef __SR_GLOBAL_H__
#define __SR_GLOBAL_H__

#include <stdint.h>
#include <sys/types.h>
#define ARM_MATH_CM4

#ifndef SIMULATION
#include <stm32f407xx.h>
#include <stm32f4_isr.h>
#endif

#define UNUSED(x) ((void) x)
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

#define unused_a __attribute__((unused))

extern unsigned int sysclk_speed;
extern unsigned int apb1_speed;
extern unsigned int apb2_speed;
extern unsigned int auxclk_speed;
extern unsigned int apb1_timer_speed;
extern unsigned int apb2_timer_speed;

extern struct leds {
    unsigned int comm_tx;
} leds;
static inline uint16_t htole(uint16_t val) { return val; }

void __libc_init_array(void);

static inline void panic(void) {
    asm volatile ("bkpt");
}

#endif /* __SR_GLOBAL_H__ */
