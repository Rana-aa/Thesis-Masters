
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <stm32f407xx.h>

#include "sr_global.h"
#include "adc.h"
#include "spi_flash.h"
#include "freq_meas.h"
#include "dsss_demod.h"
#include "con_usart.h"
//#include "uart.h"
#include "mspdebug_wrapper.h"
#include "crypto.h"

static struct spi_flash_if spif;
static int flag2 = 0;

unsigned int sysclk_speed = 0;
unsigned int apb1_speed = 0;
unsigned int apb2_speed = 0;
unsigned int auxclk_speed = 0;
unsigned int apb1_timer_speed = 0;
unsigned int apb2_timer_speed = 0;

struct leds leds;

ssize_t jt_spi_flash_read_block(void *usr, int addr, size_t len, uint8_t *out);
static void update_image_flash_counter(void);


void __libc_init_array(void) { /* we don't need this. */ }
void __assert_func (unused_a const char *file, unused_a int line, unused_a const char *function, unused_a const char *expr) {
    asm volatile ("bkpt");
    while(1) {}
}

static void clock_setup(void)
{
    /* 8MHz HSE clock as PLL source. */
#define HSE_SPEED 8000000
    /* Divide by 8 -> 1 MHz */
#define PLL_M 8
    /* Multiply by 336 -> 336 MHz VCO frequency */
#define PLL_N 336
    /* Divide by 4 -> 84 MHz (max freq for our chip) */
#define PLL_P 2
    /* Aux clock for USB OTG, SDIO, RNG: divide VCO frequency (336 MHz) by 7 -> 48 MHz (required by USB OTG) */
#define PLL_Q 7

    if (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SW_Pos) != 0)
        asm volatile ("bkpt");
    if (RCC->CR & RCC_CR_HSEON)
        asm volatile ("bkpt");

    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY))
        ;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    /* set voltage scale to 1 for max frequency
     * (0b0) scale 2 for fCLK <= 144 Mhz
     * (0b1) scale 1 for 144 Mhz < fCLK <= 168 Mhz
     */
    PWR->CR |= PWR_CR_VOS;

    /* set AHB prescaler to /1 (CFGR:bits 7:4) */
    RCC->CFGR |= (0 << RCC_CFGR_HPRE_Pos);
    /* set ABP1 prescaler to 4 -> 42MHz */
    RCC->CFGR |= (5 << RCC_CFGR_PPRE1_Pos);
    /* set ABP2 prescaler to 2 -> 84MHz */
    RCC->CFGR |= (4 << RCC_CFGR_PPRE2_Pos);

    if (RCC->CR & RCC_CR_PLLON)
        asm volatile ("bkpt");
    /* Configure PLL */
    static_assert(PLL_P % 2 == 0);
    static_assert(PLL_P >= 2 && PLL_P <= 8);
    static_assert(PLL_N >= 50 && PLL_N <= 432);
    static_assert(PLL_M >= 2 && PLL_M <= 63);
    static_assert(PLL_Q >= 2 && PLL_Q <= 15);
    uint32_t old = RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLM_Msk
        | RCC_PLLCFGR_PLLN_Msk
        | RCC_PLLCFGR_PLLP_Msk
        | RCC_PLLCFGR_PLLQ_Msk
        | RCC_PLLCFGR_PLLSRC);
    RCC->PLLCFGR = old | (PLL_M<<RCC_PLLCFGR_PLLM_Pos)
        | (PLL_N << RCC_PLLCFGR_PLLN_Pos)
        | ((PLL_P/2 - 1) << RCC_PLLCFGR_PLLP_Pos)
        | (PLL_Q << RCC_PLLCFGR_PLLQ_Pos)
        | RCC_PLLCFGR_PLLSRC; /* select HSE as PLL source */
    RCC->CR |= RCC_CR_PLLON;

    sysclk_speed = HSE_SPEED / PLL_M * PLL_N / PLL_P;
    auxclk_speed = HSE_SPEED / PLL_M * PLL_N / PLL_Q;
    apb1_speed = sysclk_speed / 4;
    apb1_timer_speed = apb1_speed * 2;
    apb2_speed = sysclk_speed / 2;
    apb2_timer_speed = apb2_speed * 2;

    /* Wait for main PLL */
    while(!(RCC->CR & RCC_CR_PLLRDY))
        ;

    /* Configure Flash: enable prefetch, insn cache, data cache; set latency = 5 wait states
     * See reference manual (RM0090), Section 3.5.1, Table 10 (p. 80)
     */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | (5<<FLASH_ACR_LATENCY_Pos);

    /* Select PLL as system clock source */
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    RCC->CFGR |= 2 << RCC_CFGR_SW_Pos;

    /* Wait for clock to switch over */
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk)>>RCC_CFGR_SWS_Pos != 2)
        ;
}

static void led_setup(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    //external led
    GPIOB->MODER |= (1<<14U);
    GPIOB->MODER &=~ (1<<15U);

    /* onboard leds */
    GPIOA->MODER |= (1<<GPIO_MODER_MODER6_Pos) | (1<<GPIO_MODER_MODER7_Pos);
    GPIOB->MODER |= (1<<GPIO_MODER_MODER11_Pos) | (1<<GPIO_MODER_MODER12_Pos) | (1<<GPIO_MODER_MODER13_Pos)| (1<<GPIO_MODER_MODER14_Pos);
    GPIOB->BSRR = 0xf << 11;
}

static void spi_flash_if_set_cs(bool val) {
    if (val)
        GPIOB->BSRR = 1<<0;
    else
        GPIOB->BSRR = 1<<16;
}

static void spi_flash_setup(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    GPIOB->MODER &= ~GPIO_MODER_MODER3_Msk & ~GPIO_MODER_MODER4_Msk & ~GPIO_MODER_MODER5_Msk & ~GPIO_MODER_MODER0_Msk;
    GPIOB->MODER |= (2<<GPIO_MODER_MODER3_Pos) /* SCK */
        | (2<<GPIO_MODER_MODER4_Pos) /* MISO */
        | (2<<GPIO_MODER_MODER5_Pos) /* MOSI */
        | (1<<GPIO_MODER_MODER0_Pos); /* CS */

    GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED3_Msk & ~GPIO_OSPEEDR_OSPEED4_Msk
        & ~GPIO_OSPEEDR_OSPEED5_Msk & ~GPIO_OSPEEDR_OSPEED0_Msk;
    GPIOB->OSPEEDR |= (2<<GPIO_OSPEEDR_OSPEED3_Pos) /* SCK */
        | (2<<GPIO_OSPEEDR_OSPEED4_Pos) /* MISO */
        | (2<<GPIO_OSPEEDR_OSPEED5_Pos) /* MOSI */
        | (2<<GPIO_OSPEEDR_OSPEED0_Pos); /* CS */

    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL3_Msk & ~GPIO_AFRL_AFSEL4_Msk & ~GPIO_AFRL_AFSEL5_Msk;
    GPIOB->AFR[0] |= (5<<GPIO_AFRL_AFSEL3_Pos) | (5<<GPIO_AFRL_AFSEL4_Pos) | (5<<GPIO_AFRL_AFSEL5_Pos);

    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;

    spif_init(&spif, 256, SPI1, &spi_flash_if_set_cs);
}

/* SPI flash test routine to be called from gdb */
#ifdef SPI_FLASH_TEST
void spi_flash_test(void) {
    spif_clear_mem(&spif);

    uint32_t buf[1024];
    for (size_t addr=0; addr<0x10000; addr += sizeof(buf)) {
        for (size_t i=0; i<sizeof(buf); i+= sizeof(buf[0]))
            buf[i/sizeof(buf[0])] = addr + i;

        spif_write(&spif, addr, sizeof(buf), (char *)buf);
    }

    for (size_t i=0; i<sizeof(buf)/sizeof(buf[0]); i++)
        buf[i] = 0;
    spif_read(&spif, 0x1030, sizeof(buf), (char *)buf);
    asm volatile ("bkpt");
}
#endif

static struct jtag_img_descriptor {
    size_t devmem_img_start;
    size_t spiflash_img_start;
    size_t img_len;
} jtag_img = {
    .devmem_img_start = 0x00c000,
    .spiflash_img_start = 0x000000,
    .img_len = 0x004000,
};

/*
char fw_dump[0x4000] = {
#include "EasyMeter_Q3DA1002_V3.03_fw_dump_0xc000.h"
};
const int fw_dump_offx = 0xc000;
const int row2_offx = 0xf438 - fw_dump_offx; 

ssize_t jt_spi_flash_read_block(void *usr, int addr, size_t len, uint8_t *out) {
    /*
    struct jtag_img_descriptor *desc = (struct jtag_img_descriptor *)usr;
    return spif_read(&spif, desc->spiflash_img_start + addr, len, (char *)out);
    // there was a * and slash

    for (size_t i=0; i<len; i++)
        out[i] = fw_dump[addr - fw_dump_offx + i];

    return len;
}

void update_image_flash_counter() {
    static int flash_counter = 0;
    flash_counter ++;
    fw_dump[row2_offx + 0] = flash_counter/10000 + '0';
    flash_counter %= 10000;
    fw_dump[row2_offx + 1] = flash_counter/1000 + '0';
    flash_counter %= 1000;
    fw_dump[row2_offx + 2] = flash_counter/100 + '0';
    flash_counter %= 100;
    fw_dump[row2_offx + 3] = flash_counter/10 + '0';
    flash_counter %= 10;
    fw_dump[row2_offx + 4] = flash_counter + '0';
}*/

/* Callback from crypto.c:oob_message_received */
void oob_trigger_activated(enum trigger_domain domain, int serial) {
    con_printf("oob_trigger_activated(%d, %d)\r\n", domain, serial);
    con_printf("done\r\n");
    flag2 = 13;
    GPIOB->ODR = 1<<7;

    //con_printf("Attempting to flash meter...\r\n");
    /*update_image_flash_counter();

    int flash_tries = 0;
    while (flash_tries++ < 25) {
        mspd_jtag_init();
        if (!mspd_jtag_flash_and_reset(jtag_img.devmem_img_start, jtag_img.img_len, jt_spi_flash_read_block, &jtag_img))
            break;
        for (int j=0; j<168*1000*5; j++)
            asm volatile ("nop");
    }
    if (flash_tries == 25)
        con_printf("Giving up.\r\n");*/
}

static unsigned int measurement_errors = 0;
static struct dsss_demod_state demod_state;
static uint32_t freq_sample_ts = 0;
static float debug_last_freq = 0;

int main(void)
{
#if DEBUG
    /* PLL clock on MCO2 (pin C9) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~GPIO_MODER_MODER9_Msk;
    GPIOC->MODER |= (2<<GPIO_MODER_MODER9_Pos);
    GPIOC->AFR[1] &= ~GPIO_AFRH_AFSEL9_Msk;
    GPIOC->OSPEEDR |= (3<<GPIO_OSPEEDR_OSPEED9_Pos);
    RCC->CFGR |= (6<<RCC_CFGR_MCO2PRE_Pos) | (3<<RCC_CFGR_MCO2_Pos);
#endif

    if (((SCB->CPACR>>20) & 0xf) != 0xf) {
        asm volatile ("bkpt");
    }

    clock_setup();
    con_usart_init();
    //uart1_tx_init();
    con_printf("\033[0m\033[2J\033[HBooting...\r\n");

    led_setup();
    spi_flash_setup();
    adc_init();



#if DEBUG
    /* TIM1 CC1 (ADC trigger) on pin A8 */
    GPIOA->MODER &= ~GPIO_MODER_MODER8_Msk;
    GPIOA->MODER |= (2<<GPIO_MODER_MODER8_Pos);
    GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8_Msk;
    GPIOA->AFR[1] |= 1<<GPIO_AFRH_AFSEL8_Pos;

    GPIOA->MODER |= (1<<GPIO_MODER_MODER11_Pos) | (1<<GPIO_MODER_MODER12_Pos) | (1<<GPIO_MODER_MODER15_Pos);
#endif

    dsss_demod_init(&demod_state);

    con_printf("Booted.\r\n");

   // con_printf("Test 1\r\n");
    //handle_dsss_received(test_data3);


    /* FIXME DEBUG */
#if 0
    uint8_t test_data[TRANSMISSION_SYMBOLS] = {
        0
    };
    con_printf("Test 0\r\n");
    handle_dsss_received(test_data);
    
    uint8_t test_data2[TRANSMISSION_SYMBOLS] = {
        0x24, 0x0f, 0x3b, 0x10, 0x27, 0x0e, 0x22, 0x30, 0x01, 0x2c, 0x1c, 0x0b, 0x35, 0x0a, 0x12, 0x27, 0x11, 0x20,
        0x0c, 0x10, 0xc0, 0x08, 0xa4, 0x72, 0xa9, 0x9b, 0x7b, 0x27, 0xee, 0xcd
    };
    con_printf("Test 1\r\n");
    handle_dsss_received(test_data2);
#endif
    /* END DEBUG */

    while (23) {
        if (adc_fft_buf_ready_idx != -1) {
            for (int j=0; j<168*1000*2; j++)
                asm volatile ("nop");
            GPIOA->BSRR = 1<<11;
            memcpy(adc_fft_buf[!adc_fft_buf_ready_idx], adc_fft_buf[adc_fft_buf_ready_idx] + FMEAS_FFT_LEN/2, sizeof(adc_fft_buf[0][0]) * FMEAS_FFT_LEN/2);
            GPIOA->BSRR = 1<<11<<16;
            GPIOB->ODR ^= 1<<14;

            bool clip_low=false, clip_high=false;
            const int clip_thresh = 100;
            for (size_t j=FMEAS_FFT_LEN/2; j<FMEAS_FFT_LEN; j++) {
                int val = adc_fft_buf[adc_fft_buf_ready_idx][j];
                if (val < clip_thresh)
                    clip_low = true;
                if (val > FMEAS_ADC_MAX-clip_thresh)
                    clip_high = true;
            }
            GPIOB->ODR = (GPIOB->ODR & ~(3<<11)) | (!clip_low<<11) | (!clip_high<<12);

            for (int j=0; j<168*1000*2; j++)
                asm volatile ("nop");

            GPIOA->BSRR = 1<<11;
            float out;
            if (adc_buf_measure_freq(adc_fft_buf[adc_fft_buf_ready_idx], &out)) {
                //con_printf("%012d: measurement error\r\n", freq_sample_ts);
                //con_printf("measurement error\r\n");
                measurement_errors++;
                GPIOB->BSRR = 1<<13;
                debug_last_freq = NAN;

            } else {
                debug_last_freq = out;
                con_printf("%012d: %2d.%03d Hz\r\n", freq_sample_ts, (int)out, (int)(out * 1000) % 1000);
                //con_printf("%2d.%03d\r\n", (int)out, (int)(out * 1000) % 1000);

                // frequency ok led
                if (48 < out && out < 52)
                    GPIOB->BSRR = 1<<13<<16;
                else
                    GPIOB->BSRR = 1<<13;

                GPIOA->BSRR = 1<<12;
                dsss_demod_step(&demod_state, out, freq_sample_ts);
                GPIOA->BSRR = 1<<12<<16;
            }
            GPIOA->BSRR = 1<<11<<16;

            freq_sample_ts++; // TODO: also increase in case of freq measurement error?
            adc_fft_buf_ready_idx = -1;
        }
    }

    return 0;
}


void NMI_Handler(void) {
    asm volatile ("bkpt #1");
}

void HardFault_Handler(void) {
    asm volatile ("bkpt #2");
}

void MemManage_Handler(void) {
    asm volatile ("bkpt #3");
}

void BusFault_Handler(void) {
    asm volatile ("bkpt #4");
}

void UsageFault_Handler(void) {
    asm volatile ("bkpt #5");
}

void SVC_Handler(void) {
    asm volatile ("bkpt #6");
}

void DebugMon_Handler(void) {
    asm volatile ("bkpt #7");
}

void PendSV_Handler(void) {
    asm volatile ("bkpt #8");
}

void SysTick_Handler(void) {
    asm volatile ("bkpt #9");
}

