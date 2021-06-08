/*
===========================================================================

whynot::math.inl: linear algebra + general math library

===========================================================================
*/

#include "core_types.h"

#include <math.h>



/*
 * NOTE: source coordinates are right handed y-up, +z out, +x right
 *       dest coordinates are right handed y-down with z(depth) clip from 0.0(near) - 1.0(far)
 */

// wn_v2f_t defs

// wn_v3f_t defs
float wn_v3f_sqr_magnitude(const wn_v3f_t* v);
float wn_v3f_magnitude(const wn_v3f_t* v);
void wn_v3f_normalize(wn_v3f_t* v);
wn_v3f_t wn_v3f_normalized(const wn_v3f_t* v);
float wn_v3f_dot(const wn_v3f_t* a, const wn_v3f_t* b);
wn_v3f_t wn_v3f_cross(const wn_v3f_t* a, const wn_v3f_t* b);
wn_v3f_t wn_v3f_minus(const wn_v3f_t* a, const wn_v3f_t* b);

// wn_v4f_t defs

// wn_mat4f_t defs
wn_mat4f_t wn_mat4f_transpose(const wn_mat4f_t* m);
wn_mat4f_t wn_mat4f_look_at(const wn_v3f_t* eye, const wn_v3f_t* at, const wn_v3f_t* up);
wn_mat4f_t wn_mat4f_perspective(float vertical_fov, float aspect_ratio, float z_near, float z_far);
wn_mat4f_t wn_mat4f_from_rotation_z(float angle);
wn_mat4f_t wn_mat4f_indentity();

// uint32_t defs
static inline uint32_t wn_u32_max(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}
static inline uint32_t wn_u32_min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

// wn_v2f_t impl

// wn_v3f_t impl
float wn_v3f_sqr_magnitude(const wn_v3f_t* v)
{
    return ((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

float wn_v3f_magnitude(const wn_v3f_t* v)
{
    float sqr_mag = wn_v3f_sqr_magnitude(v);
    return sqrtf(sqr_mag);
}

void wn_v3f_normalize(wn_v3f_t* v)
{
    float inv_mag = 1.0f / wn_v3f_magnitude(v);
    v->x *= inv_mag;
    v->y *= inv_mag;
    v->z *= inv_mag;
}

wn_v3f_t wn_v3f_normalized(const wn_v3f_t* v)
{
    wn_v3f_t r = {
        .x = v->x,
        .y = v->y,
        .z = v->z,
    };
    wn_v3f_normalize(&r);
    return r;
}

float wn_v3f_dot(const wn_v3f_t* a, const wn_v3f_t* b)
{
    return ((a->x * b->x) + (a->y * b->y) + (a->z * b->z));
}

wn_v3f_t wn_v3f_cross(const wn_v3f_t* a, const wn_v3f_t* b)
{
    // clang-format off
    return (wn_v3f_t) {
        .x = (a->y * b->z) - (a->z * b->y),
        .y = (a->z * b->x) - (a->x * b->z),
        .z = (a->x * b->y) - (a->y * b->x),
    };
    // clang-format on
}

wn_v3f_t wn_v3f_minus(const wn_v3f_t* a, const wn_v3f_t* b)
{
    return (wn_v3f_t) { .x = (a->x - b->x), .y = (a->y - b->y), .z = (a->z - b->z) };
}

// wn_mat4f_t impl

// FIXME: This file shouldn't have anything to do with stdio
#if 0
void wn_mat4f_print(const wn_mat4f_t* m)
{
    // clang-format off
    printf(
        "%f, %f, %f, %f,\n"
        "%f, %f, %f, %f,\n"
        "%f, %f, %f, %f,\n"
        "%f, %f, %f, %f,\n\n",
        m->xx, m->xy, m->xz, m->xw,
        m->yx, m->yy, m->yz, m->yw,
        m->zx, m->zy, m->zz, m->zw,
        m->wx, m->wy, m->wz, m->ww);
    // clang-format on
}
#endif

wn_mat4f_t wn_mat4f_transpose(const wn_mat4f_t* m)
{
    return (wn_mat4f_t) { .mat4f = {
                              { m->xx, m->yx, m->zx, m->wx },
                              { m->xy, m->yy, m->zy, m->wy },
                              { m->xz, m->yz, m->zz, m->wz },
                              { m->xw, m->yw, m->zw, m->ww },
                          } };
}

wn_mat4f_t wn_mat4f_look_at(const wn_v3f_t* eye, const wn_v3f_t* at, const wn_v3f_t* up)
{
    wn_v3f_t f = wn_v3f_minus(at, eye);
    wn_v3f_normalize(&f);
    wn_v3f_t r = wn_v3f_cross(&f, up);
    wn_v3f_normalize(&r);
    wn_v3f_t u = wn_v3f_cross(&r, &f);

    wn_v3f_t w = { -wn_v3f_dot(&r, eye), -wn_v3f_dot(&u, eye), wn_v3f_dot(&f, eye) };

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {r.x, u.x, -f.x, 0.0f},
            (wn_v4f_t) {r.y, u.y, -f.y, 0.0f},
            (wn_v4f_t) {r.z, u.z, -f.z, 0.0f},
            (wn_v4f_t) {w.x, w.y, w.z, 1.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_perspective(float vertical_fov, float aspect_ratio, float z_near, float z_far)
{
    float t = tanf(vertical_fov / 2.0f);
    float sy = 1.0f / t;
    float sx = sy / aspect_ratio;
    float nmf = z_near - z_far;

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {sx, 0.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, -sy, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, z_far / nmf, -1.0},
            (wn_v4f_t) {0.0f, 0.0f, z_near * z_far / nmf, 0.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_from_rotation_z(float angle)
{
    float s = sinf(angle);
    float c = cosf(angle);

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {c, s, 0.0f, 0.0f},
            (wn_v4f_t) {-s, c, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 1.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 0.0f, 1.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_indentity()
{
    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {1.0f, 0.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 1.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 1.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 0.0f, 1.0f},
        }
    };
    // clang-format on
}
