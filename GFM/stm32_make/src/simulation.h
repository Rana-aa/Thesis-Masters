#ifndef __SIMULATION_H__
#define __SIMULATION_H__

#ifdef SIMULATION
#include <stdio.h>
#define DEBUG_PRINTN(...) printf(__VA_ARGS__)
#define DEBUG_PRINTNF(fmt, ...) DEBUG_PRINTN("%s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG_PRINT(fmt, ...) DEBUG_PRINTNF(fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#define DEBUG_PRINTN(...) ((void)0)
#endif

#endif /* __SIMULATION_H__ */
