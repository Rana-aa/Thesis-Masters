#ifndef __MSPDEBUG_WRAPPER_H__
#define __MSPDEBUG_WRAPPER_H__

void mspd_jtag_init(void);
int mspd_jtag_flash_and_reset(size_t img_start, size_t img_len, ssize_t (*read_block)(void *usr, int addr, size_t len, uint8_t *out), void *usr);

#endif /* __MSPDEBUG_WRAPPER_H__ */
