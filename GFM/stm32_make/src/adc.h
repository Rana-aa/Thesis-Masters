#ifndef __ADC_H__
#define __ADC_H__

void adc_init(void);

extern uint16_t adc_fft_buf[2][FMEAS_FFT_LEN];
/* set index of ready buffer in adc.c, reset to -1 in main.c after processing */
extern volatile int adc_fft_buf_ready_idx;

#endif /* __ADC_H__ */
