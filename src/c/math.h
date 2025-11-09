#pragma once

#include <stdint.h>

#define M_PI 3.141592653589793f
#define M_PI_2 1.5707963267948966f
#define M_PI_4 0.785398163396f

// Fast absolute value
float my_fabs(float x);

// Fast floor function
float my_floor(float x);

// Round to nearest integer
float my_rint(float x);

// Fast square root using Newton-Raphson method
float my_sqrt(float x);

// Fast arctan using rational approximation
float my_atan(float x);

// Fast sine using polynomial approximation
float my_sin(float x);

// Fast cosine
float my_cos(float x);

// Fast tangent
float my_tan(float x);

// Fast arcsine using approximation
float my_asin(float x);

// Fast arccosine
float my_acos(float x);