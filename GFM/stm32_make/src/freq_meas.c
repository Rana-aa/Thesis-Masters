
#define ARM_MATH_CM4
#ifndef SIMULATION
#include <stm32f407xx.h>
#endif

#include <stdio.h>
#define DEBUG_PRINTN(...) printf(__VA_ARGS__)
#define DEBUG_PRINTNF(fmt, ...) DEBUG_PRINTN("%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG_PRINT(fmt, ...) DEBUG_PRINTNF(fmt "\n", ##__VA_ARGS__)
#include <unistd.h>
#include <math.h>

#include <stdint.h>
#include <arm_math.h>
#include <levmarq.h>

#include "freq_meas.h"
#include "sr_global.h"
#include "simulation.h"
#include "con_usart.h"


/* FTT window lookup table defined in generated/fmeas_fft_window.c */
extern const float * const fmeas_fft_window_table;

/* jury-rig some definitions for these functions since the ARM headers only export an over-generalized variable bin size
 * variant. */
extern arm_status arm_rfft_32_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_64_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_128_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_256_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_512_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_1024_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_2048_fast_init_f32(arm_rfft_fast_instance_f32 * S);
extern arm_status arm_rfft_4096_fast_init_f32(arm_rfft_fast_instance_f32 * S);

#define CONCAT(A, B, C) A ## B ## C
#define arm_rfft_init_name(nbits) CONCAT(arm_rfft_, nbits, _fast_init_f32)

void func_gauss_grad(float *out, float *params, int x, void *userdata);
float func_gauss(float *params, int x, void *userdata);
//float a_max = 0.0f;
//float out_buf[FMEAS_FFT_LEN];
//float res = 0.0f;
//int i_max = 0;
int adc_buf_measure_freq(uint16_t adc_buf[FMEAS_FFT_LEN], float *out) {
    int rc;
    float in_buf[FMEAS_FFT_LEN];
    float out_buf[FMEAS_FFT_LEN];
    
   /* con_printf("    [emulated adc buf] ");
    for (size_t i=0; i<FMEAS_FFT_LEN; i++)
        con_printf("index_%d : %5d, ", i, adc_buf[i]);
    con_printf("\n");*/

    //con_printf("\n\rApplying window function");
    for (size_t i=0; i<FMEAS_FFT_LEN; i++){
    	in_buf[i] = ((float)adc_buf[i] / (float)FMEAS_ADC_MAX - 0.5) * fmeas_fft_window_table[i];
        //in_buf[i] = ((float)adc_buf[i] / (float)FMEAS_ADC_MAX - 0.5) * fmeas_fft_window_table[i];
       // con_printf("inbuf_%d : %5d, ", i,in_buf[i]);
    }
    //con_printf("\n");

    //con_printf("\n\rRunning FFT");
    //con_printf("\nbeforeRC: %d",rc);
    arm_rfft_fast_instance_f32 fft_inst;
    if ((rc = arm_rfft_init_name(FMEAS_FFT_LEN)(&fft_inst)) != ARM_MATH_SUCCESS) { //ARM_MATH of 0 means no error
        *out = NAN;
        return rc; //It could be -1 for arg error
    }
    //con_printf("\nafterRC: %d",rc);


    /*con_printf("    [input] ");
    for (size_t i=0; i<FMEAS_FFT_LEN; i++)
        con_printf("inbuf_%d : %5d ,%010f, ",i, in_buf[i],in_buf[i]);
    con_printf("\n");*/

#ifndef SIMULATION
    GPIOA->BSRR = 1<<12;
#endif
    arm_rfft_fast_f32(&fft_inst, in_buf, out_buf, 0);
#ifndef SIMULATION
    GPIOA->BSRR = 1<<12<<16;
#endif

#define FMEAS_FFT_WINDOW_MIN_F_HZ 30.0f
#define FMEAS_FFT_WINDOW_MAX_F_HZ 70.0f
    const float binsize_hz = (float)FMEAS_ADC_SAMPLING_RATE / FMEAS_FFT_LEN;
    const size_t first_bin = (int)(FMEAS_FFT_WINDOW_MIN_F_HZ / binsize_hz);
    const size_t last_bin = (int)(FMEAS_FFT_WINDOW_MAX_F_HZ / binsize_hz + 0.5f);
    const size_t nbins = last_bin - first_bin + 1;


    /*con_printf("binsize_hz=%f first_bin=%zd last_bin=%zd nbins=%zd", binsize_hz, first_bin, last_bin, nbins);
    con_printf("    [bins real] ");
    for (size_t i=0; i<FMEAS_FFT_LEN/2; i+=2)
        con_printf("%010f, ", out_buf[i]);
    con_printf("\n    [bins imag] ");
    for (size_t i=1; i<FMEAS_FFT_LEN/2; i+=2)
        con_printf("%010f, ", out_buf[i]);
    con_printf("\n");*/

    //con_printf("Repacking FFT results");

    /* Copy real values of target data to front of output buffer */
    for (size_t i=0; i<nbins; i++) {
        float real = out_buf[2 * (first_bin + i)];
        float imag = out_buf[2 * (first_bin + i) + 1];
        out_buf[i] = sqrtf(real*real + imag*imag);
    }

    /*
    DEBUG_PRINT("Running Levenberg-Marquardt");
    */
    LMstat lmstat;
    levmarq_init(&lmstat);

    float a_max = 0.0f;
    int i_max = 0;
    for (size_t i=0; i<nbins; i++) {
        if (out_buf[i] > a_max) {
            a_max = out_buf[i];
            i_max = i;
        }
    }

    float par[3] = {
        a_max, i_max, 1.0f
    };

    //DEBUG_PRINT("    par_pre={%010f, %010f, %010f}\n", par[0], par[1], par[2]);

#ifndef SIMULATION
    GPIOA->BSRR = 1<<12;
#endif
    //con_printf("levmarg_before: %d\n\r",levmarq(3, par, nbins, out_buf, NULL, func_gauss, func_gauss_grad, NULL, &lmstat) );

    if (levmarq(3, par, nbins, out_buf, NULL, func_gauss, func_gauss_grad, NULL, &lmstat) < 0) {
        //con_printf("levmarg_during: %d\n\r",levmarq(3, par, nbins, out_buf, NULL, func_gauss, func_gauss_grad, NULL, &lmstat) );

#ifndef SIMULATION
        GPIOA->BSRR = 1<<12<<16;
#endif
        *out = NAN;
        return -1;
    }
    //con_printf("levmarg_after: %d\n\r",levmarq(3, par, nbins, out_buf, NULL, func_gauss, func_gauss_grad, NULL, &lmstat) );
#ifndef SIMULATION
    GPIOA->BSRR = 1<<12<<16;
#endif

    /*
    DEBUG_PRINT("    par_post={%010f, %010f, %010f}", par[0], par[1], par[2]);

    DEBUG_PRINT("done.");
    */
    float res = (par[1] + first_bin) * binsize_hz;
    //con_printf("res: %d\n\r",res);
    //con_printf("par[1]: %d\n\r",par[1]);
    //con_printf("par[0]: %d\n\r",par[0]);
    if (par[1] < 2 || res < 5 || res > 150 || par[0] < 1) {
   // if (res < 5 || res > 150) {
        *out = NAN;
        return -1;
    }
    
    *out = res;
    return 0;
}

float func_gauss(float *params, int x, void *userdata) {
    UNUSED(userdata);
    float a = params[0], b = params[1], c = params[2];
    float n = x-b;
    return a*expf(-n*n / (2.0f* c*c));
}

void func_gauss_grad(float *out, float *params, int x, void *userdata) {
    UNUSED(userdata);
    float a = params[0], b = params[1], c = params[2];
    float n = x-b;
    float e = expf(-n*n / (2.0f * c*c));
    
    /* d/da */
    out[0] = e;
    /* d/db */
    out[1] = a*n/(c*c) * e;
    /* d/dc */
    out[2] = a*n*n/(c*c*c) * e;
}
