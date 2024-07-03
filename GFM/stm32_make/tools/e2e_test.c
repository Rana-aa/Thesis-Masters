
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

#include "freq_meas.h"
#include "dsss_demod.h"

typedef uint16_t adc_data_t;

void handle_dsss_received(uint8_t data[static TRANSMISSION_SYMBOLS]) {
    printf("data sequence received: [ ");
    for (size_t i=0; i<TRANSMISSION_SYMBOLS; i++) {
        printf("%+3d", ((data[i]&1) ? 1 : -1) * (data[i]>>1));
        if (i+1 < TRANSMISSION_SYMBOLS)
            printf(", ");
    }
    printf(" ]\n");
}

void print_usage(void);
void print_usage() {
    fprintf(stderr, "Usage: e2e_test [emulated_adc_data.bin]\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
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

    if (st.st_size < 0 || st.st_size > 100000000) {
        fprintf(stderr, "Error reading test data: too much test data (size=%zd)\n", st.st_size);
        return 2;
    }

    if (st.st_size % sizeof(adc_data_t) != 0) {
        fprintf(stderr, "Error reading test data: file size is not divisible by %zd (size=%zd)\n", sizeof(adc_data_t), st.st_size);
        return 2;
    }

    char *buf = malloc(st.st_size);
    if (!buf) {
        fprintf(stderr, "Error allocating memory");
        return 2;
    }

    const size_t n_samples = st.st_size / sizeof(adc_data_t);
    fprintf(stderr, "Reading %zd samples test data...", n_samples);
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
    fprintf(stderr, " done. Read %zd bytes.\n", nread);

    adc_data_t *buf_d = (adc_data_t *)buf;

    struct dsss_demod_state demod;
    dsss_demod_init(&demod);

    fprintf(stderr, "Starting simulation.\n");
    size_t iterations = (n_samples-FMEAS_FFT_LEN)/(FMEAS_FFT_LEN/2);
    for (size_t i=0; i<iterations; i++) {

        /*
        fprintf(stderr, "Iteration %zd/%zd\n", i, iterations);
        */
        float res = NAN;
        int rc = adc_buf_measure_freq(buf_d + i*(FMEAS_FFT_LEN/2), &res);
        if (rc)
            printf("ERROR: Simulation error in iteration %zd at position %zd: %d\n", i, i*(FMEAS_FFT_LEN/2), rc);

        dsss_demod_step(&demod, res, i);
        /*
        printf("%09zd %12f\n", i, res);
        */
    }
    
    free(buf);
    return 0;
}
