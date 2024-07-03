
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

#include "dsss_demod.h"

void handle_dsss_received(symbol_t data[static TRANSMISSION_SYMBOLS]) {
    printf("data sequence received: [ ");
    for (size_t i=0; i<TRANSMISSION_SYMBOLS; i++) {
        //printf("%+3d", ((data[i]&1) ? 1 : -1) * (data[i]>>1));
        printf("%2d", data[i]);
        if (i+1 < TRANSMISSION_SYMBOLS)
            printf(", ");
    }
    printf(" ]\n");
}

void print_usage() {
    fprintf(stderr, "Usage: dsss_demod_test [test_data.bin] [optional recording channel number]\n");
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Error: Invalid arguments.\n");
        print_usage();
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    if (fstat(fd, &st)) {
        fprintf(stderr, "Error querying test data file size: %s\n", strerror(errno));
        return 2;
    }

    if (st.st_size < 0 || st.st_size > 10000000) {
        fprintf(stderr, "Error reading test data: too much test data (size=%zd)\n", st.st_size);
        return 2;
    }

    if (st.st_size % sizeof(float) != 0) {
        fprintf(stderr, "Error reading test data: file size is not divisible by %zd (size=%zd)\n", sizeof(float), st.st_size);
        return 2;
    }

    char *buf = malloc(st.st_size);
    if (!buf) {
        fprintf(stderr, "Error allocating memory");
        return 2;
    }

    int record_channel = -1;
    if (argc == 3) {
        char *endptr;
        record_channel = strtoul(argv[2], &endptr, 10);
        if (!endptr || *endptr != '\0') {
            fprintf(stderr, "Invalid channel number \"%s\"\n", argv[2]);
            return 1;
        }
    }

    if (record_channel != -1)
        fprintf(stderr, "Reading %zd samples test data...", st.st_size/sizeof(float));
    ssize_t nread = 0;
    while (nread < st.st_size) {
        ssize_t rc = read(fd, buf + nread, st.st_size - nread);

        if (rc == -EINTR || rc == -EAGAIN)
            continue;

        if (rc < 0) {
            fprintf(stderr, "\nError reading test data: %s\n", strerror(errno));
            return 2;
        }
        
        if (rc == 0) {
            fprintf(stderr, "\nError reading test data: Unexpected end of file\n");
            return 2;
        }

        nread += rc;
    }
    if (record_channel != -1)
        fprintf(stderr, " done.\n");

    const size_t n_samples = st.st_size / sizeof(float);
    float *buf_f = (float *)buf;

    if (record_channel != -1)
        fprintf(stderr, "Starting simulation.\n");

    struct dsss_demod_state demod;
    dsss_demod_init(&demod);
    for (size_t i=0; i<n_samples; i++) {
        //fprintf(stderr, "Iteration %zd/%zd\n", i, n_samples);
        dsss_demod_step(&demod, buf_f[i], i);
    }

    free(buf);
    return 0;
}
