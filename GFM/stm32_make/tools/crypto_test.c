
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "crypto.h"

void oob_trigger_activated(enum trigger_domain domain, int serial) {
    printf("oob_trigger_activated(%d, %d)\n", domain, serial);
    fflush(stdout);
}

void print_usage() {
    fprintf(stderr, "Usage: crypto_test [auth_key_hex]\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Error: Invalid arguments.\n");
        print_usage();
        return 1;
    }

    uint8_t auth_key[16];

    for (size_t i=0; argv[1][i+0] != '\0' && argv[1][i+1] != '\0' && i/2<sizeof(auth_key); i+= 2) {
        char buf[3] = { argv[1][i+0], argv[1][i+1], 0};
        char *endptr;
        auth_key[i/2] = strtoul(buf, &endptr, 16);
        if (!endptr || *endptr != '\0') {
            fprintf(stderr, "Invalid authkey\n");
            return 1;
        }
    }

    printf("rc=%d\n", oob_message_received(auth_key));

    return 0;
}
