/**
 * @file lv_math.h
 *
 */

#ifndef MUI_MATH_H
#define MUI_MATH_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>

    /*********************
 *      DEFINES
 *********************/

#define LV_BEZIER_VAL_SHIFT 10 /**< log2(LV_BEZIER_VAL_MAX): used to normalize up scaled values*/
#define LV_BEZIER_VAL_MAX   (1L << LV_BEZIER_VAL_SHIFT) /**< Max time in Bezier functions (not [0..1] to use integers)*/
#define LV_BEZIER_VAL_FLOAT(f)                                                                                         \
    ((int32_t) ((f) * LV_BEZIER_VAL_MAX)) /**< Convert const float number cubic-bezier values to fix-point value*/

    /**********************
 *      TYPEDEFS
 **********************/

    /**********************
 * GLOBAL PROTOTYPES
 **********************/

    //! @cond Doxygen_Suppress

    //! @endcond

    /**
 * Calculate the y value of cubic-bezier(x1, y1, x2, y2) function as specified x.
 * @param x time in range of [0..LV_BEZIER_VAL_MAX]
 * @param x1 x of control point 1 in range of [0..LV_BEZIER_VAL_MAX]
 * @param y1 y of control point 1 in range of [0..LV_BEZIER_VAL_MAX]
 * @param x2 x of control point 2 in range of [0..LV_BEZIER_VAL_MAX]
 * @param y2 y of control point 2 in range of [0..LV_BEZIER_VAL_MAX]
 * @return the value calculated
 */
    int32_t lv_cubic_bezier(int32_t x, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

    //! @cond Doxygen_Suppress

    //! @endcond

    /**
 * Get the mapped of a number given an input and output range
 * @param x integer which mapped value should be calculated
 * @param min_in min input range
 * @param max_in max input range
 * @param min_out max output range
 * @param max_out max output range
 * @return the mapped number
 */
    int32_t lv_map(int32_t x, int32_t min_in, int32_t max_in, int32_t min_out, int32_t max_out);

/**********************
 *      MACROS
 **********************/
#define LV_MIN(a, b) ((a) < (b) ? (a) : (b))

#define LV_MAX(a, b) ((a) > (b) ? (a) : (b))

#define LV_ABS(x) ((x) > 0 ? (x) : (-(x)))

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* MUI_MATH_H */
