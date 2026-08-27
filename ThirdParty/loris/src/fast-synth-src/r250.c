/*
 * r250.c  the r250 uniform random number algorithm
 *
 *         Kirkpatrick, S., and E. Stoll, 1981; "A Very Fast
 *         Shift-Register Sequence Random Number Generator",
 *         Journal of Computational Physics, V.40
 *
 *         also:
 *
 *         see W.L. Maier, DDJ May 1991
 */

// static char rcsid[] = "@(#)r250.c    1.2 15:50:31 11/21/94   EFC";

#include "r250.h"

#include "randlcg.h"

#include <limits.h>

#define BITS 32
#define MSB 0x80000000L
#define ALL_BITS 0xffffffffL
#define HALF_RANGE 0x40000000L
#define STEP 7

static unsigned int r250_buffer[250];
static int r250_index;

void
r250_init(int sd)
{
    int j, k;
    unsigned int mask, msb;

    set_seed(sd);

    r250_index = 0;
    for (j = 0; j < 250; j++)
    { /* fill r250 buffer with BITS-1 bit values */
        r250_buffer[j] = randlcg();
    }

    for (j = 0; j < 250; j++)
    { /* set some MSBs to 1 */
        if (randlcg() > HALF_RANGE)
        {
            r250_buffer[j] |= MSB;
        }
    }

    msb = MSB;       /* turn on diagonal bit */
    mask = ALL_BITS; /* turn off the leftmost bits */

    for (j = 0; j < BITS; j++)
    {
        k = STEP * j + 3;       /* select a word to operate on */
        r250_buffer[k] &= mask; /* turn off bits left of the diagonal */
        r250_buffer[k] |= msb;  /* turn on the diagonal bit */
        mask >>= 1;
        msb >>= 1;
    }
}

unsigned int
r250(void) /* returns a random unsigned integer */
{
    register int j;
    register unsigned int new_rand;

    if (r250_index >= 147)
        j = r250_index - 147; /* wrap pointer around */
    else
        j = r250_index + 103;

    new_rand = r250_buffer[r250_index] ^ r250_buffer[j];
    r250_buffer[r250_index] = new_rand;

    if (r250_index >= 249) /* increment pointer for next time */
        r250_index = 0;
    else
        r250_index++;

    return new_rand;
}

double
dr250(void) /* returns a random double in range 0..1 */
{
    register int j;
    register unsigned int new_rand;

    if (r250_index >= 147)
        j = r250_index - 147; /* wrap pointer around */
    else
        j = r250_index + 103;

    new_rand = r250_buffer[r250_index] ^ r250_buffer[j];
    r250_buffer[r250_index] = new_rand;

    if (r250_index >= 249) /* increment pointer for next time */
        r250_index = 0;
    else
        r250_index++;

    return (double)new_rand / ALL_BITS;
}

#ifdef TEST_R250

/*
 * test driver
 *
 * prints out either NMR_RAND values or a histogram.
 *
 * Compile using:  gcc -g -DTEST_R250 -o r250 r250.c randlcg.c
 */

#include <stdio.h>

#define TOTAL_BUCKETS 100
#define TOTAL_SAMPLES 10000000

int
main(int argc, char **argv)
{
    int j, k, bucket_count, seed;
    int buckets[TOTAL_BUCKETS];
    double value, bucket_inc;
    double bucket_limit[TOTAL_BUCKETS];

    if (argc != 3)
    {
        printf("Usage -- %s bucket_count seed\n", argv[0]);
        return -1;
    }

    bucket_count = atoi(argv[1]);
    if (bucket_count > TOTAL_BUCKETS)
    {
        printf("ERROR -- maximum number of bins is %d\n", TOTAL_BUCKETS);
        return -1;
    }

    seed = atoi(argv[2]);

    r250_init(seed);

    if (bucket_count < 1)
    { /* just print out the numbers */
        for (j = 0; j < TOTAL_SAMPLES; j++)
            printf("%f\n", dr250());
        return -1;
    }

    bucket_inc = 1.0 / bucket_count;
    for (j = 0; j < bucket_count; j++)
    { /* initialize buckets to zero */
        buckets[j] = 0;
        bucket_limit[j] = (j + 1) * bucket_inc;
    }

    bucket_limit[bucket_count - 1] =
        1.0e7; /* make sure all others are in last bucket */

    for (j = 0; j < TOTAL_SAMPLES; j++)
    {
        value = r250() / (double)ALL_BITS;
        for (k = 0; k < bucket_count; k++)
        {
            if (value < bucket_limit[k])
            {
                buckets[k]++;
                break;
            }
        }
    }

    for (j = 0; j < bucket_count; j++)
    {
        printf("%d\n", buckets[j]);
    }

    return 0;
}

#endif /* TEST_R250 */
