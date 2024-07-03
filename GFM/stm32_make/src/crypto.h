#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <stdint.h>

/* Presig message length: 15 byte = 120 bit ^= 20 * 6-bit symbols of 5-bit bipolar DSSS */
#define PRESIG_MSG_LEN 15
#define OOB_TRIGGER_LEN PRESIG_MSG_LEN

enum trigger_domain {
    TRIGGER_DOMAIN_ALL,
    TRIGGER_DOMAIN_VENDOR,
    TRIGGER_DOMAIN_SERIES,
    TRIGGER_DOMAIN_COUNTRY,
    TRIGGER_DOMAIN_REGION,
    _TRIGGER_DOMAIN_COUNT
};

extern const char *presig_domain_strings[_TRIGGER_DOMAIN_COUNT];
extern uint8_t presig_keys[_TRIGGER_DOMAIN_COUNT][PRESIG_MSG_LEN];
extern int presig_height;
extern uint8_t presig_bundle_id[16];
extern uint64_t bundle_timestamp;

extern void oob_trigger_activated(enum trigger_domain domain, int height);

int oob_message_received(uint8_t msg[static OOB_TRIGGER_LEN]);
int verify_trigger(const uint8_t inkey[PRESIG_MSG_LEN], int *height_out, int *domain_out);

#endif /* __CRYPTO_H__ */
