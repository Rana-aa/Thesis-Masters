#ifndef __RSLIB_H__
#define __RSLIB_H__

/* parity length configuration */
#include "rscode-config.h"

void rslib_encode(int nbits, size_t msglen, char msg[static msglen], char out[msglen + NPAR]);
int rslib_decode(int nbits, size_t msglen, char msg_inout[static msglen]);
int rslib_gexp(int z, int nbits);
size_t rslib_npar(void);

#endif /* __RSLIB_H__ */
