#define ARM_MATH_CM4
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include <stm32f407xx.h>
#include <stdint.h>
#include <arm_math.h>

#include "con_usart.h"
#include "freq_meas.h"
#include "sr_global.h"
#include "dsss_demod.h"
#include "simulation.h"

#include "generated/dsss_gold_code.h"
// #include "generated/dsss_butter_filter.h"

/* Generated CWT wavelet LUT */
extern const float * const dsss_cwt_wavelet_table;

//struct iir_biquad cwt_filter_bq[DSSS_FILTER_CLEN] = {DSSS_FILTER_COEFF};

void debug_print_vector(const char *name, size_t len, const float *data, size_t stride, bool index, bool debug);
static float gold_correlate_step(const size_t ncode, const float a[DSSS_CORRELATION_LENGTH], size_t offx, bool debug);
static float cwt_convolve_step(const float v[DSSS_WAVELET_LUT_SIZE], size_t offx);
//static float run_iir(const float x, const int order, const struct iir_biquad q[order], struct iir_biquad_state st[order]);
//static float run_biquad(float x, const struct iir_biquad *const q, struct iir_biquad_state *const restrict st);
static void matcher_init(struct matcher_state states[static DSSS_MATCHER_CACHE_SIZE]);
static void matcher_tick(struct matcher_state states[static DSSS_MATCHER_CACHE_SIZE],
        uint64_t ts, int peak_ch, float peak_ampl);
static void group_received(struct dsss_demod_state *st);
static symbol_t decode_peak(int peak_ch, float peak_ampl);

#ifdef SIMULATION
void debug_print_vector(const char *name, size_t len, const float *data, size_t stride, bool index, bool debug) {
    if (!debug)
        return;

    if (index) {
        DEBUG_PRINTN("    %16s  [", "");
        for (size_t i=0; i<len; i++)
            DEBUG_PRINTN("%8zu  ", i);
        DEBUG_PRINTN("]\n");
    }

    DEBUG_PRINTN("    %16s: [", name);
    for (size_t i=0; i<len; i++)
        DEBUG_PRINTN("%8.5f, ", data[i*stride]);
    DEBUG_PRINTN("]\n");
}
#else 
void debug_print_vector(unused_a const char *name, unused_a size_t len, unused_a const float *data,
        unused_a size_t stride, unused_a bool index, unused_a bool debug) {}
#endif

void dsss_demod_init(struct dsss_demod_state *st) {
    memset(st, 0, sizeof(*st));
    matcher_init(st->matcher_cache);
}

void dsss_demod_step(struct dsss_demod_state *st, float new_value, uint64_t ts) {
    //const float hole_patching_threshold = 0.01 * DSSS_CORRELATION_LENGTH;
    //con_printf("dsss_demod_step\r\n");
    bool log = false;
    bool log_groups = false;

    st->signal[st->signal_wpos] = new_value;
    st->signal_wpos = (st->signal_wpos + 1) % ARRAY_LENGTH(st->signal);

    /* use new, incremented wpos for gold_correlate_step as first element of old data in ring buffer */
    for (size_t i=0; i<DSSS_GOLD_CODE_COUNT; i++)
        st->correlation[i][st->correlation_wpos] = gold_correlate_step(i, st->signal, st->signal_wpos, false);

    st->correlation_wpos = (st->correlation_wpos + 1) % ARRAY_LENGTH(st->correlation[0]);

    float cwt[DSSS_GOLD_CODE_COUNT];
    for (size_t i=0; i<DSSS_GOLD_CODE_COUNT; i++)
        cwt[i] = cwt_convolve_step(st->correlation[i], st->correlation_wpos);

    float avg = 0.0f;
    for (size_t i=0; i<DSSS_GOLD_CODE_COUNT; i++)
        avg += fabsf(cwt[i]);
    avg /= (float)DSSS_GOLD_CODE_COUNT;
    if (log) con_printf("%6zu: %f ", ts, avg);
    /* FIXME fix this filter */
    //avg = run_iir(avg, ARRAY_LENGTH(cwt_filter_bq), cwt_filter_bq, st->cwt_filter.st);

    float max_val = st->group.max;
    int max_ch = st->group.max_ch;
    int max_ts = st->group.max_ts;
    bool found = false;
    for (size_t i=0; i<DSSS_GOLD_CODE_COUNT; i++) {
        float val = cwt[i] / avg;
        if (log) con_printf("%f ", cwt[i]);
        if (log) con_printf("%f ", val);

        if (fabsf(val) > fabsf(max_val)) {
            max_val = val;
            max_ch = i;
            max_ts = ts;
        }

        if (fabsf(val) > DSSS_THRESHOLD_FACTOR)
            found = true;
    }
    if (log) con_printf("%f %d ", max_val, found);
    if (log) con_printf("\n");

    matcher_tick(st->matcher_cache, ts, max_ch, max_val);

    if (found) {
        /* Continue ongoing group */
        st->group.len++;
        st->group.max = max_val;
        st->group.max_ch = max_ch;
        st->group.max_ts = max_ts;
        return;
    }
    
    if (st->group.len == 0)
        /* We're between groups */
        return;

    if (log_groups) DEBUG_PRINTN("GROUP: %zu %d %f\n", st->group.max_ts, st->group.max_ch, st->group.max);
    /* A group ended. Process result. */
    group_received(st);

    /* reset grouping state */
    st->group.len = 0;
    st->group.max_ts = 0;
    st->group.max_ch = 0;
    st->group.max = 0.0f;
}

/* Map a sequence match to a data symbol. This maps the sequence's index number to the 2nd to n+2nd bit of the result,
 * and maps the polarity of detection to the LSb. 5-bit example:
 * 
 * [0, S, S, S, S, S, S, P] ; S ^= symbol index (0 - 2^n+1 so we need just about n+1 bit), P ^= symbol polarity
 *
 * Symbol polarity is preserved from transmitter to receiver. The symbol index is n+1 bit instead of n bit since we have
 * 2^n+1 symbols to express, one too many for an n-bit index.
 */
symbol_t decode_peak(int peak_ch, float peak_ampl) {
    return (peak_ch<<1) | (peak_ampl > 0);
}

void matcher_init(struct matcher_state states[static DSSS_MATCHER_CACHE_SIZE]) {
    for (size_t i=0; i<DSSS_MATCHER_CACHE_SIZE; i++)
        states[i].last_phase = -1; /* mark as inactive */
}

/* TODO make these constants configurable from Makefile */
const int group_phase_tolerance = (int)(DSSS_CORRELATION_LENGTH * 0.10);

void matcher_tick(struct matcher_state states[static DSSS_MATCHER_CACHE_SIZE], uint64_t ts, int peak_ch, float peak_ampl) {
    /* TODO make these constants configurable from Makefile */
    const float skip_sampling_depreciation = 0.2f; /* 0.0 -> no depreciation, 1.0 -> complete disregard */
    const float score_depreciation = 0.1f; /* 0.0 -> no depreciation, 1.0 -> complete disregard */
    const int current_phase = ts % DSSS_CORRELATION_LENGTH;
    const int max_skips = TRANSMISSION_SYMBOLS/4*3;
    bool debug = false;

    bool header_printed = false;
    for (size_t i=0; i<DSSS_MATCHER_CACHE_SIZE; i++) {
        if (states[i].last_phase == -1)
            continue; /* Inactive entry */

        if (current_phase == states[i].last_phase) {
            /* Skip sampling */
            float score = fabsf(peak_ampl) * (1.0f - skip_sampling_depreciation);
            if (score > states[i].candidate_score) {
                if (debug && !header_printed) {
                    header_printed = true;
                    DEBUG_PRINTN("windows %zu\n", ts);
                }
                if (debug) DEBUG_PRINTN("    skip %zd old=%f new=%f\n", i, states[i].candidate_score, score);
                /* We win, update candidate */
                assert(i < DSSS_MATCHER_CACHE_SIZE);
                states[i].candidate_score = score;
                states[i].candidate_phase = current_phase;
                states[i].candidate_data = decode_peak(peak_ch, peak_ampl);
                states[i].candidate_skips = 1;
            }
        }

        /* Note of caution on group_phase_tolerance: Group detection has some latency since a group is only considered
         * "detected" after signal levels have fallen back below the detection threshold. This means we only get to
         * process a group a couple ticks after its peak. We have to make sure the window is still open at this point.
         * This means we have to match against group_phase_tolerance should a little bit loosely.
         */
        int phase_delta = current_phase - states[i].last_phase;
        if (phase_delta < 0)
            phase_delta += DSSS_CORRELATION_LENGTH;
        if (phase_delta == group_phase_tolerance + DSSS_DECIMATION) {
            if (debug && !header_printed) {
                header_printed = true;
                DEBUG_PRINTN("windows %zu\n", ts);
            }
            if (debug) DEBUG_PRINTN("    %zd ", i);
            /* Process window results */
            assert(i < DSSS_MATCHER_CACHE_SIZE);
            assert(0 <= states[i].data_pos && states[i].data_pos < TRANSMISSION_SYMBOLS);
            states[i].data[ states[i].data_pos ] = states[i].candidate_data;
            states[i].data_pos = states[i].data_pos + 1;
            states[i].last_score = score_depreciation * states[i].last_score +
                (1.0f - score_depreciation) * states[i].candidate_score;
            if (debug) DEBUG_PRINTN("commit pos=%d val=%d score=%f ", states[i].data_pos, states[i].candidate_data, states[i].last_score);
            states[i].candidate_score = 0.0f;
            states[i].last_skips += states[i].candidate_skips;

            if (states[i].last_skips > max_skips) {
                if (debug) DEBUG_PRINTN("expire ");
                states[i].last_phase = -1; /* invalidate entry */

            } else if (states[i].data_pos == TRANSMISSION_SYMBOLS) {
                if (debug) DEBUG_PRINTN("match ");
                /* Frame received completely */
                handle_dsss_received(states[i].data);
                states[i].last_phase = -1; /* invalidate entry */
            }
            if (debug) DEBUG_PRINTN("\n");
        }
    }
}

static float gaussian(float a, float b, float c, float x) {
    float n = x-b;
    return a*expf(-n*n / (2.0f* c*c));
}


static float score_group(const struct group *g, int phase_delta) {
    /* TODO make these constants configurable from Makefile */
    const float distance_func_phase_tolerance = 10.0f;
    return fabsf(g->max) * gaussian(1.0f, 0.0f, distance_func_phase_tolerance, phase_delta);
}

void group_received(struct dsss_demod_state *st) {
    bool debug = false;
    const int group_phase = st->group.max_ts % DSSS_CORRELATION_LENGTH;
    /* This is the score of a decoding starting at this group (with no context) */
    float base_score = score_group(&st->group, 0);

    float min_score = INFINITY;
    ssize_t min_idx = -1;
    ssize_t empty_idx = -1;
    for (size_t i=0; i<DSSS_MATCHER_CACHE_SIZE; i++) {

        /* Search for empty entries */
        if (st->matcher_cache[i].last_phase == -1) {
            empty_idx = i;
            continue;
        }

        /* Search for entries with matching phase */
        /* This is the score of this group given the cached decoding at [i] */
        int phase_delta = st->matcher_cache[i].last_phase - group_phase;
        if (abs(phase_delta) <= group_phase_tolerance) {

            float group_score = score_group(&st->group, phase_delta);
            if (st->matcher_cache[i].candidate_score < group_score) {
                assert(i < DSSS_MATCHER_CACHE_SIZE);
                if (debug) DEBUG_PRINTN("    appending %zu %d score=%f pd=%d\n", i, decode_peak(st->group.max_ch, st->group.max), group_score, phase_delta);
                /* Append to entry */
                st->matcher_cache[i].candidate_score = group_score;
                st->matcher_cache[i].candidate_phase = group_phase;
                st->matcher_cache[i].candidate_data = decode_peak(st->group.max_ch, st->group.max);
                st->matcher_cache[i].candidate_skips = 0;
            }
        }

        /* Search for weakest entry */
        /* TODO figure out this fitness function */
        float score = st->matcher_cache[i].last_score * (1.5f + 0.1 * st->matcher_cache[i].data_pos);
        if (debug) DEBUG_PRINTN("    score %zd %f %f %d", i, score, st->matcher_cache[i].last_score, st->matcher_cache[i].data_pos);
        if (score < min_score) {
            min_idx = i;
            min_score = score;
        }
    }

    /* If we found empty entries, replace one by a new decoding starting at this group */
    if (empty_idx >= 0) {
        if (debug) DEBUG_PRINTN("    empty %zd %d\n", empty_idx, decode_peak(st->group.max_ch, st->group.max));
        assert(0 <= empty_idx && empty_idx < DSSS_MATCHER_CACHE_SIZE);
        st->matcher_cache[empty_idx].last_phase = group_phase;
        st->matcher_cache[empty_idx].candidate_score = base_score;
        st->matcher_cache[empty_idx].last_score = base_score;
        st->matcher_cache[empty_idx].candidate_phase = group_phase;
        st->matcher_cache[empty_idx].candidate_data = decode_peak(st->group.max_ch, st->group.max);
        st->matcher_cache[empty_idx].data_pos = 0;
        st->matcher_cache[empty_idx].candidate_skips = 0;
        st->matcher_cache[empty_idx].last_skips = 0;

    /* If the weakest decoding in cache is weaker than a new decoding starting here, replace it */
    } else if (min_score < base_score && min_idx >= 0) {
        if (debug) DEBUG_PRINTN("    min %zd %d\n", min_idx, decode_peak(st->group.max_ch, st->group.max));
        assert(0 <= min_idx && min_idx < DSSS_MATCHER_CACHE_SIZE);
        st->matcher_cache[min_idx].last_phase = group_phase;
        st->matcher_cache[min_idx].candidate_score = base_score;
        st->matcher_cache[min_idx].last_score = base_score;
        st->matcher_cache[min_idx].candidate_phase = group_phase;
        st->matcher_cache[min_idx].candidate_data = decode_peak(st->group.max_ch, st->group.max);
        st->matcher_cache[min_idx].data_pos = 0;
        st->matcher_cache[min_idx].candidate_skips = 0;
        st->matcher_cache[min_idx].last_skips = 0;
    }
}

#if 0
float run_iir(const float x, const int order, const struct iir_biquad q[order], struct iir_biquad_state st[order]) {
    float intermediate = x;
    for (int i=0; i<(order+1)/2; i++)
        intermediate = run_biquad(intermediate, &q[i], &st[i]);
    return intermediate;
}

float run_biquad(float x, const struct iir_biquad *const q, struct iir_biquad_state *const restrict st) {
    /* direct form 2, see https://en.wikipedia.org/wiki/Digital_biquad_filter */
    float intermediate = x + st->reg[0] * -q->a[0] + st->reg[1] * -q->a[1];
    float out = intermediate * q->b[0] + st->reg[0] * q->b[1] + st->reg[1] * q->b[2];
    st->reg[1] = st->reg[0];
    st->reg[0] = intermediate;
    return out;
}
#endif

float cwt_convolve_step(const float v[DSSS_WAVELET_LUT_SIZE], size_t offx) {
    float sum = 0.0f;
    for (ssize_t j=0; j<DSSS_WAVELET_LUT_SIZE; j++) {
        /* Our wavelet is symmetric so convolution and correlation are identical. Use correlation here for ease of
         * implementation */
        sum += v[(offx + j) % DSSS_WAVELET_LUT_SIZE] * dsss_cwt_wavelet_table[j];
        //DEBUG_PRINT("        j=%d v=%f w=%f", j, v[(offx + j) % DSSS_WAVELET_LUT_SIZE], dsss_cwt_wavelet_table[j]);
    }
    return sum;
}

/* Compute last element of correlation for input [a] and hard-coded gold sequences.
 *
 * This is intened to be used once for each new incoming sample in [a]. It expects [a] to be of length
 * [dsss_correlation_length] and produces the one sample where both the reference sequence and the input fully overlap.
 * This is equivalent to "valid" mode in numpy's terminology[0].
 *
 * [0] https://docs.scipy.org/doc/numpy/reference/generated/numpy.correlate.html
 */
float gold_correlate_step(const size_t ncode, const float a[DSSS_CORRELATION_LENGTH], size_t offx, bool debug) {

    float acc_outer = 0.0f;
    uint8_t table_byte = 0;
    if (debug) DEBUG_PRINTN("Correlate n=%zd: ", ncode);
    for (size_t i=0; i<DSSS_GOLD_CODE_LENGTH; i++) {

        if ((i&7) == 0) {
            table_byte = dsss_gold_code_table[ncode][i>>3]; /* Fetch sequence table item */
            if (debug) DEBUG_PRINTN("|");
        }
        int bv = table_byte & (0x80>>(i&7)); /* Extract bit */
        bv = !!bv*2 - 1; /* Map 0, 1 -> -1, 1 */
        if (debug) DEBUG_PRINTN("%s%d\033[0m", bv == 1 ? "\033[92m" : "\033[91m", (bv+1)/2);

        float acc_inner = 0.0f;
        for (size_t j=0; j<DSSS_DECIMATION; j++)
            acc_inner += a[(offx + i*DSSS_DECIMATION + j) % DSSS_CORRELATION_LENGTH]; /* Multiply item */
        //if (debug) DEBUG_PRINTN("%.2f ", acc_inner);
        acc_outer += acc_inner * bv;
    }
    if (debug) DEBUG_PRINTN("\n");
    return acc_outer / DSSS_CORRELATION_LENGTH;
}
