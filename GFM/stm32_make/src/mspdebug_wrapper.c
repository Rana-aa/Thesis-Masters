
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "output.h"
#include "jtaglib.h"

#include "sr_global.h"
#include "gpio_helpers.h"
#include "mspdebug_wrapper.h"
#include "con_usart.h"

#include <stm32f407xx.h>

#define BLOCK_SIZE 512 /* bytes */


static void sr_delay_inst(void);

static struct jtdev sr_jtdev;
static struct jtdev sr_jtdev_default;

enum sr_gpio_types {
	SR_GPIO_TCK,
	SR_GPIO_TMS,
	SR_GPIO_TDI,
	SR_GPIO_RST,
	SR_GPIO_TST,
	SR_GPIO_TDO,
	SR_NUM_GPIOS
};

struct {
    GPIO_TypeDef *gpio;
	int pin;
    int mode; 
} gpios[SR_NUM_GPIOS] = {
	[SR_GPIO_TCK] = {GPIOE, 10, 1},
	[SR_GPIO_TMS] = {GPIOE, 11, 1},
	[SR_GPIO_TDI] = {GPIOE, 12, 1},
	[SR_GPIO_RST] = {GPIOE, 9, 1},
	[SR_GPIO_TST] = {GPIOE, 14, 1},
	[SR_GPIO_TDO] = {GPIOE, 13, 0},
};

void sr_delay_inst() {
    for (int i=0; i<10; i++)
        asm volatile("nop");
}

void mspd_jtag_init() {
    for (int i=0; i<SR_NUM_GPIOS; i++)
        gpio_pin_setup(gpios[i].gpio, gpios[i].pin, gpios[i].mode, 3, 0, 0);
}

static void sr_gpio_write(int num, int out) {
	if (out)
        gpios[num].gpio->BSRR = 1<<gpios[num].pin;
	else
        gpios[num].gpio->BSRR = 1<<gpios[num].pin<<16;
}

static void sr_jtdev_rst(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_RST, out);
}

int mspd_jtag_flash_and_reset(size_t img_start, size_t img_len, ssize_t (*read_block)(void *usr, int addr, size_t len, uint8_t *out), void *usr)
{
	union {
		uint8_t bytes[BLOCK_SIZE];
		uint16_t words[BLOCK_SIZE/2];
	} block;

    memcpy(&sr_jtdev, &sr_jtdev_default, sizeof(sr_jtdev));
	/* Initialize JTAG connection */
	unsigned int jtag_id = jtag_init(&sr_jtdev);

	if (sr_jtdev.failed) {
        con_printf("Couldn't initialize device\r\n");
		return -EPIPE;
    }

    con_printf("JTAG device ID: 0x%02x\r\n", jtag_id);
	if (jtag_id != 0x89 && jtag_id != 0x91)
		return -EINVAL;

#if 0
    con_printf("Memory dump:\r\n");
    for (size_t i=0x1000; i<=0x10ff;) {
        con_printf("%04x: ", i);
        for (size_t j=0; j<16; i+=1, j+=1) {
            con_printf("%02x ", jtag_read_mem(&sr_jtdev, 8, i));
        }
        con_printf("\r\n");
    }
    return 0;
#endif

	/* Clear flash */
	jtag_erase_flash(&sr_jtdev, JTAG_ERASE_MAIN, 0);
	if (sr_jtdev.failed)
		return -EPIPE;

	/* Write flash */
	for (size_t p = img_start; p < img_start + img_len; p += BLOCK_SIZE) {
        con_printf("Writing block %04zx\r\n", p);
		ssize_t nin = read_block(usr, p, BLOCK_SIZE, block.bytes);

		if (nin < 0)
			return nin;

		if (nin & 1) { /* pad unaligned */
			block.bytes[nin] = 0;
			nin ++;
		}
		
		/* Convert to little-endian */
		for (ssize_t i=0; i<nin/2; i++)
			block.words[i] = htole(block.words[i]);

		jtag_write_flash(&sr_jtdev, p, nin/2, block.words);
		if (sr_jtdev.failed)
			return -EPIPE;
	}

    /* TODO: Verify flash here. */

	/* Execute power on reset */
	jtag_execute_puc(&sr_jtdev);
	if (sr_jtdev.failed)
		return -EPIPE;

    jtag_release_device(&sr_jtdev, 0xfffe);

	return 0;
}

/* mspdebug HAL shim */

int printc_err(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int rc = usart_printf_blocking_va(&con_usart, fmt, va);
    if (rc)
        return rc;

    size_t i;
    for (i=0; fmt[i]; i++)
        ;
    if (i > 0 && fmt[i-1] == '\n')
        usart_putc_nonblocking(&con_usart, '\r');
    return rc;
}


static void sr_jtdev_power_on(struct jtdev *p) {
    UNUSED(p);
	/* ignore */
}

static void sr_jtdev_connect(struct jtdev *p) {
    UNUSED(p);
	/* ignore */
}

static void sr_jtdev_tck(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_TCK, out);
}

static void sr_jtdev_tms(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_TMS, out);
}

static void sr_jtdev_tdi(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_TDI, out);
}

static void sr_jtdev_tst(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_TST, out);
}

static int sr_jtdev_tdo_get(struct jtdev *p) {
    UNUSED(p);
	return !!(gpios[SR_GPIO_TDO].gpio->IDR & (1<<gpios[SR_GPIO_TDO].pin));
}

static void sr_jtdev_tclk(struct jtdev *p, int out) {
	UNUSED(p);
	sr_gpio_write(SR_GPIO_TDI, out);
}

static int sr_jtdev_tclk_get(struct jtdev *p) {
    UNUSED(p);
	return !!(gpios[SR_GPIO_TDI].gpio->ODR & (1<<gpios[SR_GPIO_TDI].pin));
}

static void sr_jtdev_tclk_strobe(struct jtdev *p, unsigned int count) {
    UNUSED(p);
	while (count--) {
        gpios[SR_GPIO_TDI].gpio->BSRR = 1<<gpios[SR_GPIO_TDI].pin;
        sr_delay_inst();
        gpios[SR_GPIO_TDI].gpio->BSRR = 1<<gpios[SR_GPIO_TDI].pin<<16;
	}
}

static void sr_jtdev_led_green(struct jtdev *p, int out) {
	UNUSED(p);
	UNUSED(out);
	/* ignore */
}

static void sr_jtdev_led_red(struct jtdev *p, int out) {
	UNUSED(p);
	UNUSED(out);
	/* ignore */
}


static struct jtdev_func sr_jtdev_vtable = {
	.jtdev_open = NULL,
	.jtdev_close = NULL,

	.jtdev_power_off = NULL,
	.jtdev_release = NULL,

	.jtdev_power_on = sr_jtdev_power_on,
	.jtdev_connect = sr_jtdev_connect,

	.jtdev_tck = sr_jtdev_tck,
	.jtdev_tms = sr_jtdev_tms,
	.jtdev_tdi = sr_jtdev_tdi,
	.jtdev_rst = sr_jtdev_rst,
	.jtdev_tst = sr_jtdev_tst,
	.jtdev_tdo_get = sr_jtdev_tdo_get,

	.jtdev_tclk = sr_jtdev_tclk,
	.jtdev_tclk_get = sr_jtdev_tclk_get,
	.jtdev_tclk_strobe = sr_jtdev_tclk_strobe,

	.jtdev_led_green = sr_jtdev_led_green,
	.jtdev_led_red = sr_jtdev_led_red,

};

static struct jtdev sr_jtdev = {
	0,
	.f = &sr_jtdev_vtable
};

static struct jtdev sr_jtdev_default = {
	0,
	.f = &sr_jtdev_vtable
};


