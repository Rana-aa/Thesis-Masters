
#include <assert.h>

#include "sr_global.h"
#include "dsss_demod.h"
#include "con_usart.h"
#include "rslib.h"
#include "crypto.h"

#include <stdint.h>
//#include <stdlib.h>

static uint8_t data2[TRANSMISSION_SYMBOLS];
/*static uint8_t test_data3[TRANSMISSION_SYMBOLS] = {
        0xcb, 0xb2, 0xb6, 0x48, 0x4b, 0x7d, 0xb6, 0x9e, 0x4d, 0xa1, 0x0e, 0x75, 0xc1, 0x7b, 0xd8, 0xb2, 0xb6, 0x48,
		0x4b, 0x7d, 0xb6, 0x9e, 0x4d, 0xa1, 0x0e, 0x75, 0xc1, 0x7b, 0xee, 0xcd
    };*/

unsigned long strtoul_custom(const char *nptr, char **endptr, int base) {
    unsigned long result = 0;
    int digit;

    // Process the string
    while (*nptr != '\0') {
        // Convert the character to a digit
        if (*nptr >= '0' && *nptr <= '9') {
            digit = *nptr - '0';
        } else if (*nptr >= 'a' && *nptr <= 'f') {
            digit = *nptr - 'a' + 10;
        } else if (*nptr >= 'A' && *nptr <= 'F') {
            digit = *nptr - 'A' + 10;
        } else {
            break;  // Not a valid digit
        }

        // Update the result based on the digit and base
        result = result * base + digit;

        // Move to the next character
        nptr++;
    }

    // Set endptr if it's provided
    if (endptr != NULL) {
        *endptr = (char *)nptr;
    }

    return result;
}

void handle_dsss_received(uint8_t data[static TRANSMISSION_SYMBOLS]) {

	 for (int i=0; i<TRANSMISSION_SYMBOLS; i++) {
	        data2[i] = data[i];
	    }

    /* Console status output */
    con_printf("DSSS data received: ");
    for (int i=0; i<TRANSMISSION_SYMBOLS; i++) {
        int x = (data[i]>>1) * (data[i]&1 ? 1 : -1);
        con_printf("%3d ", x);
    }
    con_printf("\r\n");
    con_printf("done\r\n");

    //uint8_t numbers[30] = {12,11,11,2,11,6,4,8,4,11,7,13,11,6,9,14,4,13,10,1,0,14,7,5,12,1,7,11,13,8};
        char hexString[61]; // Assuming each number is represented by two hex characters and add a null terminator
        int index = 0;
        int nonZeroEncountered = 0;

        for (size_t i = 0; i < 30; i++) {
            char highNibble = data[i] >> 4;
            char lowNibble = data[i] & 0xF;

            if (highNibble > 0 || nonZeroEncountered) {
                hexString[index++] = "0123456789abcdef"[highNibble];
                nonZeroEncountered = 1;
            }

            hexString[index++] = "0123456789abcdef"[lowNibble];
        }

        hexString[index] = '\0';

        //printf("%s\n", hexString);

        uint8_t auth_key[16];


        for (size_t i=0; i/2<sizeof(auth_key); i+= 2) {
            char buf[3] = {hexString[i+0], hexString[i+1], 0};
            char *endptr;
            auth_key[i/2] = strtoul_custom(buf, &endptr, 16);
            if (!endptr || *endptr != '\0') {}
        }



    /* Run reed-solomon error correction */
    //const int sym_bits = DSSS_GOLD_CODE_NBITS + 1; /* +1 for amplitude sign bit */
    /* TODO identify erasures in DSSS demod layer */
    //(void) rslib_decode(sym_bits, TRANSMISSION_SYMBOLS, (char *)data);
    /* TODO error detection & handling */

    /* Re-bit-pack data buffer to be bit-continuous:
     *      [ . . a b  c d e f ] [ . . g h  i j k l ] [ . . m n  o p q r ] ...
     *  ==> [ a b c d  e f g h ] [ i j k l  m n o p ] [ q r ...          ] ...
     */
    /*static_assert((TRANSMISSION_SYMBOLS - NPAR) * (DSSS_GOLD_CODE_NBITS + 1) == OOB_TRIGGER_LEN * 8);
    for (uint8_t i=0, j=0; i < TRANSMISSION_SYMBOLS - NPAR; i++, j += sym_bits) {
        uint32_t sym = data[i]; // [ ... | . . X X  X X X X ] for 5-bit dsss
        data[i] = 0; // clear for output

        sym <<= 8-sym_bits; // left-align: [ ... | X X X X  X X . . ]
        sym <<= 8; // shift to second byte: [ ... | X X X X  X X . . | . . . .  . . . . ]
        sym >>= (j%8); // shift to bit write offset: [ ... | . . . .  X X X X | X X . .  . . . . ] for offset 4
        data[j/8] |= sym >> 8; // write upper byte
        data[j/8 + 1] |= sym & 0xff; // write lower byte
    }*/

    /* hand off to crypto.c */
    oob_message_received(auth_key);
}
