
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

void print_usage(void);

void print_usage() {
    fprintf(stderr, "Usage: freq_meas_test [test_data.bin]\n");
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

    if (st.st_size < 0 || st.st_size > 1000000) {
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
    fprintf(stderr, " done.\n");

    const size_t n_samples = st.st_size / sizeof(float);
    float *buf_f = (float *)buf;

    int16_t *sim_adc_buf = calloc(sizeof(int16_t), n_samples);
    if (!sim_adc_buf) {
        fprintf(stderr, "Error allocating memory\n");
        return 2;
    }

    fprintf(stderr, "Converting and truncating test data...");
    for (size_t i=0; i<n_samples; i++)
        /* Note on scaling: We can't simply scale by 0x8000 (1/2 full range) here. Our test data is nominally 1Vp-p but
         * certain tests such as the interharmonics one can have some samples exceeding that range. */
        sim_adc_buf[i] = buf_f[i] * (0x4000-1);
    fprintf(stderr, " done.\n");

    fprintf(stderr, "Starting simulation.\n");

    size_t iterations = (n_samples-FMEAS_FFT_LEN)/(FMEAS_FFT_LEN/2);
    for (size_t i=0; i<iterations; i++) {

        fprintf(stderr, "Iteration %zd/%zd\n", i, iterations);
        float res = NAN;
        int rc = adc_buf_measure_freq(sim_adc_buf + i*(FMEAS_FFT_LEN/2), &res);
        if (rc)
            printf("ERROR: Simulation error in iteration %zd at position %zd: %d\n", i, i*(FMEAS_FFT_LEN/2), rc);

        printf("%09zd %12f\n", i, res);
    }
    
    free(buf);
    free(sim_adc_buf);
    return 0;
}
