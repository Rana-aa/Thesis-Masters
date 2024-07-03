#include <stdint.h>
#include <string.h>

#include "rscode-config.h"
#include <ecc.h>

#include "rslib.h"

static struct rscode_driver driver;

void rslib_encode(int nbits, size_t msglen, char msg[static msglen], char out[msglen + NPAR]) {
    rscode_init(&driver);
    rscode_encode(&driver, (unsigned char *)msg, msglen, (unsigned char *)out);
}

int rslib_decode(int nbits, size_t msglen, char msg_inout[static msglen]) {
    rscode_init(&driver);
    return rscode_decode(&driver, (unsigned char *)msg_inout, msglen);
}

int rslib_gexp(int z, int nbits) {
    rscode_init(&driver);
    return gexp(&driver, z);
}

size_t rslib_npar() {
    return NPAR;
}
