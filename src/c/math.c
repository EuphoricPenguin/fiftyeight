#include <math.h>
#include <stdint.h>

// Define math constants if not available
#ifndef M_PI
#define M_PI 3.141592653589793f
#endif

#ifndef M_PI_2
#define M_PI_2 1.5707963267948966f
#endif

// Fast absolute value
float my_fabs(float x) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    u.i &= 0x7FFFFFFF;  // Clear sign bit
    return u.f;
}

// Fast floor function
float my_floor(float x) {
    if (x >= 0.0f) {
        return (float)(int)x;
    } else {
        float i = (float)(int)x;
        return (x == i) ? i : i - 1.0f;
    }
}

// Round to nearest integer
float my_rint(float x) {
    return (x >= 0.0f) ? (float)(int)(x + 0.5f) : (float)(int)(x - 0.5f);
}

// Fast square root using Newton-Raphson method
float my_sqrt(const float x) {
    if (x <= 0.0f) return 0.0f;
    
    float y, z;
    y = x * 0.5f;
    z = x;
    
    // Reinterpret cast for initial guess
    union {
        float f;
        int i;
    } u;
    u.f = z;
    u.i = 0x5f3759df - (u.i >> 1);  // Magic number for initial approximation
    z = u.f;
    
    // Two Newton-Raphson iterations for better precision
    z = z * (1.5f - y * z * z);
    z = z * (1.5f - y * z * z);
    
    return x * z;
}

// Fast arctan using rational approximation
float my_atan(float x) {
    if (my_fabs(x) < 0.4375f) {
        // Small angle approximation for |x| < 0.4375
        float x2 = x * x;
        return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f + x2 * (-0.11643287f + x2 * 0.05265332f))));
    }
    
    if (my_fabs(x) < 1.0f) {
        // Medium range approximation
        float x2 = x * x;
        return x / (1.0f + 0.28f * x2);
    }
    
    // Large values use atan(x) = pi/2 - atan(1/x)
    return (x > 0.0f ? M_PI_2 : -M_PI_2) - my_atan(1.0f / x);
}

// Range reduction helper for trig functions
static float range_reduce(float x, float* sign) {
    *sign = 1.0f;
    
    // Normalize to [-PI, PI]
    x = x - (float)((int)(x / (2.0f * M_PI))) * 2.0f * M_PI;
    
    // Handle negative angles
    if (x < 0.0f) {
        x = -x;
        *sign = -1.0f;
    }
    
    // Reduce to [0, PI/2]
    if (x > M_PI) {
        x = 2.0f * M_PI - x;
        *sign = -(*sign);
    }
    if (x > M_PI_2) {
        x = M_PI - x;
    }
    
    return x;
}

// Fast sine using polynomial approximation
float my_sin(float x) {
    float sign;
    x = range_reduce(x, &sign);
    
    // Bhaskara I's sine approximation
    if (x <= M_PI_2) {
        float x2 = x * x;
        return sign * (16.0f * x * (M_PI - x)) / (5.0f * M_PI * M_PI - 4.0f * x * (M_PI - x));
    }
    
    return 0.0f;
}

// Fast cosine
float my_cos(float x) {
    return my_sin(x + M_PI_2);
}

// Fast tangent
float my_tan(float x) {
    float s = my_sin(x);
    float c = my_cos(x);
    
    // Avoid division by zero
    if (my_fabs(c) < 1e-6f) {
        return (s > 0.0f) ? 1e6f : -1e6f;
    }
    
    return s / c;
}

// Fast arcsine using approximation
float my_asin(float x) {
    if (my_fabs(x) > 1.0f) return 0.0f;  // Domain error
    
    // Rational approximation
    float x2 = x * x;
    return x * (1.0f + x2 * (0.0833333333f + x2 * (0.0375f + x2 * 0.0208333333f)));
}

// Fast arccosine
float my_acos(float x) {
    return M_PI_2 - my_asin(x);
}
