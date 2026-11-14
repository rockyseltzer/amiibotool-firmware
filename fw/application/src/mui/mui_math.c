/**
 * @file lv_math.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "mui_math.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

#define CUBIC_NEWTON_ITERATIONS 8
#define CUBIC_PRECISION_BITS    10 /* 10 or 14 bits recommended, int64_t calculation is used for >14bit precision */

#if CUBIC_PRECISION_BITS < 10 || CUBIC_PRECISION_BITS > 20
#error "cubic precision bits should be in range of [10, 20] for 32bit/64bit calculations."
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static const uint16_t sin0_90_table[] = {
    0,     572,   1144,  1715,  2286,  2856,  3425,  3993,  4560,  5126,  5690,  6252,  6813,  7371,  7927,  8481,
    9032,  9580,  10126, 10668, 11207, 11743, 12275, 12803, 13328, 13848, 14365, 14876, 15384, 15886, 16384, 16877,
    17364, 17847, 18324, 18795, 19261, 19720, 20174, 20622, 21063, 21498, 21926, 22348, 22763, 23170, 23571, 23965,
    24351, 24730, 25102, 25466, 25822, 26170, 26510, 26842, 27166, 27482, 27789, 28088, 28378, 28660, 28932, 29197,
    29452, 29698, 29935, 30163, 30382, 30592, 30792, 30983, 31164, 31336, 31499, 31651, 31795, 31928, 32052, 32166,
    32270, 32365, 32449, 32524, 32588, 32643, 32688, 32723, 32748, 32763, 32768};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * cubic-bezier Reference:
 *
 * https://github.com/gre/bezier-easing
 * https://opensource.apple.com/source/WebCore/WebCore-955.66/platform/graphics/UnitBezier.h
 *
 * Copyright (c) 2014 Gaëtan Renaudeau
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

static int32_t do_cubic_bezier(int32_t t, int32_t a, int32_t b, int32_t c)
{
    /*a * t^3 + b * t^2 + c * t*/
#if CUBIC_PRECISION_BITS > 14
    int64_t ret;
#else
    int32_t ret;
#endif

    ret = a;
    ret = (ret * t) >> CUBIC_PRECISION_BITS;
    ret = ((ret + b) * t) >> CUBIC_PRECISION_BITS;
    ret = ((ret + c) * t) >> CUBIC_PRECISION_BITS;
    return ret;
}

/**
 * Calculate the y value of cubic-bezier(x1, y1, x2, y2) function as specified x.
 * @param x time in range of [0..LV_BEZIER_VAL_MAX]
 * @param x1 x of control point 1 in range of [0..LV_BEZIER_VAL_MAX]
 * @param y1 y of control point 1 in range of [0..LV_BEZIER_VAL_MAX]
 * @param x2 x of control point 2 in range of [0..LV_BEZIER_VAL_MAX]
 * @param y2 y of control point 2 in range of [0..LV_BEZIER_VAL_MAX]
 * @return the value calculated
 */
int32_t lv_cubic_bezier(int32_t x, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    int32_t ax, bx, cx, ay, by, cy;
    int32_t tl, tr, t; /*t in cubic-bezier function, used for bisection */
    int32_t xs;        /*x sampled on curve */
#if CUBIC_PRECISION_BITS > 14
    int64_t d; /*slope value at specified t*/
#else
    int32_t d;
#endif

    if (x == 0 || x == LV_BEZIER_VAL_MAX)
        return x;

    /* input is always LV_BEZIER_VAL_SHIFT bit precision */

#if CUBIC_PRECISION_BITS != LV_BEZIER_VAL_SHIFT
    x <<= CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT;
    x1 <<= CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT;
    x2 <<= CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT;
    y1 <<= CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT;
    y2 <<= CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT;
#endif

    cx = 3 * x1;
    bx = 3 * (x2 - x1) - cx;
    ax = (1L << CUBIC_PRECISION_BITS) - cx - bx;

    cy = 3 * y1;
    by = 3 * (y2 - y1) - cy;
    ay = (1L << CUBIC_PRECISION_BITS) - cy - by;

    /*Try Newton's method firstly */
    t = x; /*Make a guess*/
    for (int i = 0; i < CUBIC_NEWTON_ITERATIONS; i++)
    {
        /*Check if x on curve at t matches input x*/
        xs = do_cubic_bezier(t, ax, bx, cx) - x;
        if (LV_ABS(xs) <= 1)
            goto found;

        /* get slop at t, d = 3 * ax * t^2 + 2 * bx + t + cx */
        d = ax; /* use 64bit operation if needed. */
        d = (3 * d * t) >> CUBIC_PRECISION_BITS;
        d = ((d + 2 * bx) * t) >> CUBIC_PRECISION_BITS;
        d += cx;

        if (LV_ABS(d) <= 1)
            break;

        d = ((int64_t) xs * (1L << CUBIC_PRECISION_BITS)) / d;
        if (d == 0)
            break; /*Reached precision limits*/
        t -= d;
    }

    /*Fallback to bisection method for reliability*/
    tl = 0, tr = 1L << CUBIC_PRECISION_BITS, t = x;

    if (t < tl)
    {
        t = tl;
        goto found;
    }

    if (t > tr)
    {
        t = tr;
        goto found;
    }

    while (tl < tr)
    {
        xs = do_cubic_bezier(t, ax, bx, cx);
        if (LV_ABS(xs - x) <= 1)
            goto found;
        if (x > xs)
            tl = t;
        else
            tr = t;
        t = (tr - tl) / 2 + tl;
        if (t == tl)
            break;
    }

    /*Failed to find suitable t for given x, return a value anyway.*/
found:
    /*Return y at t*/
#if CUBIC_PRECISION_BITS != LV_BEZIER_VAL_SHIFT
    return do_cubic_bezier(t, ay, by, cy) >> (CUBIC_PRECISION_BITS - LV_BEZIER_VAL_SHIFT);
#else
    return do_cubic_bezier(t, ay, by, cy);
#endif
}

/**
 * Get the mapped of a number given an input and output range
 * @param x integer which mapped value should be calculated
 * @param min_in min input range
 * @param max_in max input range
 * @param min_out max output range
 * @param max_out max output range
 * @return the mapped number
 */
int32_t lv_map(int32_t x, int32_t min_in, int32_t max_in, int32_t min_out, int32_t max_out)
{
    if (max_in >= min_in && x >= max_in)
        return max_out;
    if (max_in >= min_in && x <= min_in)
        return min_out;

    if (max_in <= min_in && x <= max_in)
        return max_out;
    if (max_in <= min_in && x >= min_in)
        return min_out;

    /**
     * The equation should be:
     *   ((x - min_in) * delta_out) / delta in) + min_out
     * To avoid rounding error reorder the operations:
     *   (x - min_in) * (delta_out / delta_min) + min_out
     */

    int32_t delta_in = max_in - min_in;
    int32_t delta_out = max_out - min_out;

    return ((x - min_in) * delta_out) / delta_in + min_out;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
