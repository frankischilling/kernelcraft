#include "math.h"
#include <math.h>

v3 v3_add(v3 a, v3 b) { return (v3){a.x+b.x, a.y+b.y, a.z+b.z}; }
v3 v3_sub(v3 a, v3 b) { return (v3){a.x-b.x, a.y-b.y, a.z-b.z}; }
v3 v3_mul(v3 a, float s) { return (v3){a.x*s, a.y*s, a.z*s}; }
float v3_dot(v3 a, v3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
v3 v3_cross(v3 a, v3 b) {
    return (v3){ a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
float v3_len(v3 a) { return sqrtf(v3_dot(a,a)); }
v3 v3_norm(v3 a) {
    float l = v3_len(a);
    if (l <= 0.000001f) return (v3){0,0,0};
    return v3_mul(a, 1.0f/l);
}

m4 m4_identity(void) {
    m4 r = {0};
    r.m[0]=1; r.m[5]=1; r.m[10]=1; r.m[15]=1;
    return r;
}

m4 m4_mul(m4 a, m4 b) {
    m4 r = {0};
    for (int c=0;c<4;c++) {
        for (int rrow=0;rrow<4;rrow++) {
            float s = 0.0f;
            for (int k=0;k<4;k++) {
                s += a.m[k*4 + rrow] * b.m[c*4 + k];
            }
            r.m[c*4 + rrow] = s;
        }
    }
    return r;
}

m4 m4_translate(v3 t) {
    m4 r = m4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    m4 r = {0};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

m4 m4_look(v3 eye, v3 center, v3 up) {
    v3 f = v3_norm(v3_sub(center, eye));
    v3 s = v3_norm(v3_cross(f, up));
    v3 u = v3_cross(s, f);

    m4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;

    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] =  v3_dot(f, eye);
    return r;
}
