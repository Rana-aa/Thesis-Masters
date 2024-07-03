
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

#include "crypto.h"
#include "simulation.h"
#include "con_usart.h"

//static uint8_t msg2[OOB_TRIGGER_LEN];
//static uint8_t flag=0;

void debug_hexdump(const char *name, const uint8_t *buf, size_t len);
int verify_trigger_dom(const uint8_t inkey[PRESIG_MSG_LEN],
        const char *domain_string, const uint8_t refkey[PRESIG_MSG_LEN]);


void debug_hexdump(const char *name, const uint8_t *buf, size_t len) {
	DEBUG_PRINTN("%20s: ", name);
    for (size_t i=0; i<len;) {
        for (size_t j=0; j<8 && i<len; i++, j++)
        	DEBUG_PRINTN("%02x ", buf[i]);
        DEBUG_PRINTN(" ");
    }
    DEBUG_PRINTN("\n");
}

/* Returns trigger sig height for correct trigger */
int verify_trigger_dom(const uint8_t inkey[PRESIG_MSG_LEN],
        const char *domain_string, const uint8_t refkey[PRESIG_MSG_LEN]) {
    uint8_t key[crypto_auth_hmacsha512_KEYBYTES];
    uint8_t key_out[crypto_auth_hmacsha512_BYTES];

    static_assert(PRESIG_MSG_LEN <= crypto_auth_hmacsha512_KEYBYTES);
    memcpy(key, inkey, PRESIG_MSG_LEN);
    memset(key + PRESIG_MSG_LEN, 0, sizeof(key) - PRESIG_MSG_LEN);
    //con_printf("ds \"%s\"\n\r", domain_string);
    debug_hexdump("\n\rref", refkey, PRESIG_MSG_LEN);

    for (int i=0; i<presig_height; i++) {
    	DEBUG_PRINTN("Verifying height rel %d abs %d\n\r", i, presig_height-i);
        debug_hexdump("\n\rkey", key, sizeof(key));
        (void)crypto_auth_hmacsha512(key_out, (uint8_t *)domain_string, strlen(domain_string), key);
        //debug_hexdump("\n\rout", key_out, sizeof(key_out));
        memcpy(key, key_out, PRESIG_MSG_LEN);
        memset(key + PRESIG_MSG_LEN, 0, sizeof(key) - PRESIG_MSG_LEN);
        
        if (!memcmp(key, refkey, PRESIG_MSG_LEN))
            return presig_height-i;
    }

    return 0;
}

int verify_trigger(const uint8_t inkey[PRESIG_MSG_LEN], int *height_out, int *domain_out) {
    int res;
    for (int i=0; i<_TRIGGER_DOMAIN_COUNT; i++) {
    	//con_printf("Verifying domain %d\n\r", i);
        if ((res = verify_trigger_dom(inkey, presig_domain_strings[i], presig_keys[i]))) {
        	DEBUG_PRINTN("Match!\n\r");
        	//flag = 55;
            if (height_out)
                *height_out = res - 1;
            if (domain_out)
                *domain_out = i;
            return 1;
        }
    }
    return 0;
}

int oob_message_received(uint8_t msg[static OOB_TRIGGER_LEN]) {
	//con_printf("oob msg receved\n\r");
	 /*for (int i=0; i<OOB_TRIGGER_LEN; i++) {
	        msg2[i] = msg[i];
	    }*/
    int height, domain;
    if (verify_trigger(msg, &height, &domain)) {
        oob_trigger_activated(domain, height);
        return 1;
    }

    return 0;
}
