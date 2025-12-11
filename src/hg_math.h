

#ifndef HG_MATH_H
#define HG_MATH_H

#include <string.h> // memset
#include <math.h>   // tanf

typedef struct _UniformBufferObject
{
    float model[16];
    float view[16];
    float proj[16];
} UniformBufferObject;


void 
mat4_identity(float* m) 
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void 
mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float tanHalfFov = tanf(fov / 2.0f);
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f / (aspect * tanHalfFov);
    m[5] = 1.0f / tanHalfFov;
    m[10] = -(far + near) / (far - near);
    m[11] = -1.0f;
    m[14] = -(2.0f * far * near) / (far - near);
}

void 
mat4_rotate_y(float* m, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    mat4_identity(m);
    m[0] = c;
    m[2] = s;
    m[8] = -s;
    m[10] = c;
}

void 
mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

#endif 