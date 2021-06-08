#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum wn_result
{
    WN_OK,
    WN_ERR,
} wn_result;

// math types
typedef union wn_v2f_t
{
    float v2f[2];
    struct
    {
        float x, y;
    };
    struct
    {
        float u, v;
    };
    struct
    {
        float r, g;
    };
} wn_v2f_t;

typedef union wn_v3f_t
{
    float v3f[3];
    struct
    {
        float x, y, z;
    };
    struct
    {
        float u, v, w;
    };
    struct
    {
        float r, g, b;
    };
} wn_v3f_t;

typedef union wn_v4f_t
{
    float v4f[4];
    struct
    {
        float x, y, z, w;
    };
    struct
    {
        float r, g, b, a;
    };
} wn_v4f_t;

typedef union wn_mat4f_t
{
    float mat4f[4][4];
    wn_v4f_t v4f[4];
    struct
    {
        float xx, xy, xz, xw;
        float yx, yy, yz, yw;
        float zx, zy, zz, zw;
        float wx, wy, wz, ww;
    };
} wn_mat4f_t;
