#ifndef __DSSS_DEMOD_H__
#define __DSSS_DEMOD_H__

#include <stdint.h>
#include <unistd.h>

#define DSSS_GOLD_CODE_LENGTH   ((1<<DSSS_GOLD_CODE_NBITS) - 1)
#define DSSS_GOLD_CODE_COUNT    ((1<<DSSS_GOLD_CODE_NBITS) + 1)
#define DSSS_CORRELATION_LENGTH (DSSS_GOLD_CODE_LENGTH * DSSS_DECIMATION)

/* FIXME: move to makefile */
#define DSSS_MATCHER_CACHE_SIZE 8

#if DSSS_GOLD_CODE_NBITS < 8
typedef uint8_t symbol_t;
#else
typedef uint16_t symbol_t;
#endif

struct iir_biquad {
    float a[2];
    float b[3];
};

struct iir_biquad_state {
    float reg[2];
};

struct cwt_iir_filter_state {
    struct iir_biquad_state st[3];
};

struct group {
    int len; /* length of group in samples */
    float max; /* signed value of largest peak in group on any channel */
    uint64_t max_ts; /* absolute position of above peak */
    int max_ch; /* channel (gold sequence index) of above peak */
};

struct matcher_state {
    int last_phase; /* 0 .. DSSS_CORRELATION_LENGTH */
    int candidate_phase;

    float last_score;
    float candidate_score;

    int last_skips;
    int candidate_skips;

    symbol_t data[TRANSMISSION_SYMBOLS]; 
    int data_pos;
    symbol_t candidate_data;
};

struct dsss_demod_state {
    float signal[DSSS_CORRELATION_LENGTH];
    size_t signal_wpos;

    float correlation[DSSS_GOLD_CODE_COUNT][DSSS_WAVELET_LUT_SIZE];
    size_t correlation_wpos;

    struct cwt_iir_filter_state cwt_filter;

    struct group group;

    struct matcher_state matcher_cache[DSSS_MATCHER_CACHE_SIZE];
};


extern void handle_dsss_received(symbol_t data[static TRANSMISSION_SYMBOLS]);

void dsss_demod_init(struct dsss_demod_state *st);
void dsss_demod_step(struct dsss_demod_state *st, float new_value, uint64_t ts);

#endif /* __DSSS_DEMOD_H__ */
