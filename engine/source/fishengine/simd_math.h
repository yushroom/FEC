#ifndef SIMD_MATH_H
#define SIMD_MATH_H

#include <assert.h>
#include <math.h>
#include <string.h>
#include <xmmintrin.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y;
} float2;

typedef struct {
    float x, y, z;
} float3;

#define float3_zero \
    (float3) { 0, 0, 0 }
#define float3_forward \
    (float3) { 0, 0, -1 }
#define float3_up \
    (float3) { 0, 1, 0 }
#define float3_right \
    (float3) { 1, 0, 0 }

static inline bool float3_is_zero(float3 v) {
    return v.x == 0 && v.y == 0 && v.z == 0;
}

#if __clang__
typedef __attribute__((ext_vector_type(4))) float float4;
#else
typedef __declspec(align(16)) struct { float x, y, z, w; } float4;
#endif

typedef float4 quat;

#define quat_identity \
    (quat) {0, 0, 0, 1}

#define float4_zero \
    (float4) { 0, 0, 0, 0 }

static inline float3 float4_to_float3(float4 v) {
    float3 r = {v.x, v.y, v.z};
    return r;
}

static inline float4 float4_make(float x, float y, float z, float w) {
    float4 r = {x, y, z, w};
    return r;
}

#if __clang__
struct __attribute((aligned(16))) float4x4 {
#else
struct __declspec(align(16)) float4x4 {
#endif
    union {
        float4 columns[4];
        struct {
            float m00, m10, m20, m30, m01, m11, m21, m31, m02, m12, m22, m32,
                m03, m13, m23, m33;
        };
        float m[4][4];
        float a[16];
    };
};

typedef struct float4x4 float4x4;

static inline float4x4 float4x4_diagonal(float x, float y, float z, float w) {
    float4x4 m;
    memset(&m, 0, sizeof(m));
    m.m00 = x;
    m.m11 = y;
    m.m22 = z;
    m.m33 = w;
    return m;
}

static inline float4x4 float4x4_identity() {
    return float4x4_diagonal(1, 1, 1, 1);
}

static inline float3 float3_negate(float3 a) {
    float3 r = {-a.x, -a.y, -a.z};
    return r;
}

static inline float3 float3_add(float3 a, float3 b) {
    float3 r = {a.x + b.x, a.y + b.y, a.z + b.z};
    return r;
}

static inline float3 float3_subtract(float3 a, float3 b) {
    float3 r = {a.x - b.x, a.y - b.y, a.z - b.z};
    return r;
}

static inline float float3_dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float3 float3_cross(float3 a, float3 b) {
    float3 r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    return r;
}

static inline float3 float3_mul1(float3 a, float b) {
    float3 r = {a.x * b, a.y * b, a.z * b};
    return r;
}

static inline float float3_length(float3 v) {
    return sqrt(float3_dot(v, v));
}

static inline float3 float3_normalize(float3 v) {
    float len = float3_length(v);
    if (len > 1e-5f) {
        float inv_len = 1.f / len;
        return float3_mul1(v, inv_len);
    }
    return float3_zero;
}

static float _lerp(float a, float b, float t) { return a + (b - a) * t; }

static inline float3 float3_lerp(float3 a, float3 b, float t) {
    float3 c;
    c.x = _lerp(a.x, b.x, t);
    c.y = _lerp(a.y, b.y, t);
    c.z = _lerp(a.z, b.z, t);
    return c;
}

static inline float float4_dot(float4 a, float4 b) {
    float4 y = a * b;
    return y.x + y.y + y.z + y.w;
}

// static inline float float4_dot1(float4 x) {
//	return float4_dot(x, x);
//}

static inline float float4_length_squared(float4 x) { return float4_dot(x, x); }

static inline float float4_length(float4 x) {
    return sqrt(float4_length_squared(x));
}

static inline float4 float4_add(float4 a, float4 b) {
#if __clang__
    return a + b;
#else
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
#endif
}

static inline float4 float4_mul(float4 a, float4 b) {
#if __clang__
    return a * b;
#else
    return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
#endif
}

static inline float4 float4_normalize(float4 a) {
    float len = float4_length(a);
    if (len > 1e-6f) {
        return a * (1.0f / len);
    }
    return a;
}

// static inline float4 float4_transpose(float4x4 __x)
//{
//#if defined __SSE__
//	float4x4 __t0 = _mm_unpacklo_ps(__x.columns[0],__x.columns[2]);
//	float4x4 __t1 = _mm_unpackhi_ps(__x.columns[0],__x.columns[2]);
//	float4x4 __t2 = _mm_unpacklo_ps(__x.columns[1],__x.columns[3]);
//	float4x4 __t3 = _mm_unpackhi_ps(__x.columns[1],__x.columns[3]);
//	float4x4 __r0 = _mm_unpacklo_ps(__t0,__t2);
//	float4x4 __r1 = _mm_unpackhi_ps(__t0,__t2);
//	float4x4 __r2 = _mm_unpacklo_ps(__t1,__t3);
//	float4x4 __r3 = _mm_unpackhi_ps(__t1,__t3);
//	return simd_matrix(__r0,__r1,__r2,__r3);
//#else
//	abort();
//#endif
//}

static inline float4x4 float4x4_transpose(float4x4 __x) {
    float4 __t0 = _mm_unpacklo_ps(__x.columns[0], __x.columns[2]);
    float4 __t1 = _mm_unpackhi_ps(__x.columns[0], __x.columns[2]);
    float4 __t2 = _mm_unpacklo_ps(__x.columns[1], __x.columns[3]);
    float4 __t3 = _mm_unpackhi_ps(__x.columns[1], __x.columns[3]);
    float4 __r0 = _mm_unpacklo_ps(__t0, __t2);
    float4 __r1 = _mm_unpackhi_ps(__t0, __t2);
    float4 __r2 = _mm_unpacklo_ps(__t1, __t3);
    float4 __r3 = _mm_unpackhi_ps(__t1, __t3);
    float4x4 ret = {.columns[0] = __r0,
                    .columns[1] = __r1,
                    .columns[2] = __r2,
                    .columns[3] = __r3};
    return ret;
}

static inline float4x4 float4x4_inverse(float4x4 m) {
    float p10_21 = m.m10 * m.m21 - m.m20 * m.m11;
    float p10_22 = m.m10 * m.m22 - m.m20 * m.m12;
    float p10_23 = m.m10 * m.m23 - m.m20 * m.m13;
    float p11_22 = m.m11 * m.m22 - m.m21 * m.m12;
    float p11_23 = m.m11 * m.m23 - m.m21 * m.m13;
    float p12_23 = m.m12 * m.m23 - m.m22 * m.m13;
    float p10_31 = m.m10 * m.m31 - m.m30 * m.m11;
    float p10_32 = m.m10 * m.m32 - m.m30 * m.m12;
    float p10_33 = m.m10 * m.m33 - m.m30 * m.m13;
    float p11_32 = m.m11 * m.m32 - m.m31 * m.m12;
    float p11_33 = m.m11 * m.m33 - m.m31 * m.m13;
    float p12_33 = m.m12 * m.m33 - m.m32 * m.m13;
    float p20_31 = m.m20 * m.m31 - m.m30 * m.m21;
    float p20_32 = m.m20 * m.m32 - m.m30 * m.m22;
    float p20_33 = m.m20 * m.m33 - m.m30 * m.m23;
    float p21_32 = m.m21 * m.m32 - m.m31 * m.m22;
    float p21_33 = m.m21 * m.m33 - m.m31 * m.m23;
    float p22_33 = m.m22 * m.m33 - m.m32 * m.m23;
    float4x4 A;
    A.m00 = +(m.m11 * p22_33 - m.m12 * p21_33 + m.m13 * p21_32);
    A.m10 = -(m.m10 * p22_33 - m.m12 * p20_33 + m.m13 * p20_32);
    A.m20 = +(m.m10 * p21_33 - m.m11 * p20_33 + m.m13 * p20_31);
    A.m30 = -(m.m10 * p21_32 - m.m11 * p20_32 + m.m12 * p20_31);
    A.m01 = -(m.m01 * p22_33 - m.m02 * p21_33 + m.m03 * p21_32);
    A.m11 = +(m.m00 * p22_33 - m.m02 * p20_33 + m.m03 * p20_32);
    A.m21 = -(m.m00 * p21_33 - m.m01 * p20_33 + m.m03 * p20_31);
    A.m31 = +(m.m00 * p21_32 - m.m01 * p20_32 + m.m02 * p20_31);
    A.m02 = +(m.m01 * p12_33 - m.m02 * p11_33 + m.m03 * p11_32);
    A.m12 = -(m.m00 * p12_33 - m.m02 * p10_33 + m.m03 * p10_32);
    A.m22 = +(m.m00 * p11_33 - m.m01 * p10_33 + m.m03 * p10_31);
    A.m32 = -(m.m00 * p11_32 - m.m01 * p10_32 + m.m02 * p10_31);
    A.m03 = -(m.m01 * p12_23 - m.m02 * p11_23 + m.m03 * p11_22);
    A.m13 = +(m.m00 * p12_23 - m.m02 * p10_23 + m.m03 * p10_22);
    A.m23 = -(m.m00 * p11_23 - m.m01 * p10_23 + m.m03 * p10_21);
    A.m33 = +(m.m00 * p11_22 - m.m01 * p10_22 + m.m02 * p10_21);

    float det = 0;
    det += m.m00 * A.m00;
    det += m.m01 * A.m10;
    det += m.m02 * A.m20;
    det += m.m03 * A.m30;
    float inv_det = 1.f / det;
    for (int i = 0; i < 16; ++i) {
        A.a[i] *= inv_det;
    }
    //	for (int i = 0; i < 4; i++) {
    //		A.columns[i] = A.columns[i] * inv_det;
    //	}
    return A;
}

static inline float4 float4x4_mul_float4(float4x4 a, float4 b) {
#pragma STDC FP_CONTRACT ON
    float4 o;
    o = a.columns[0] * b[0];
    o = a.columns[1] * b[1] + o;
    o = a.columns[2] * b[2] + o;
    o = a.columns[3] * b[3] + o;
    return o;
}

static inline float4x4 float4x4_mul(float4x4 a, float4x4 b) {
    float4x4 o;
    for (int i = 0; i < 4; ++i) {
        o.columns[i] = float4x4_mul_float4(a, b.columns[i]);
    }
    return o;

    //	float4x4 o;
    //	#pragma unroll
    //	for (int row = 0; row < 4; ++row)
    //	{
    //		for (int col = 0; col < 4; ++col)
    //		{
    ////			float sum = a->m[col][row] * b->m[0][j] +
    ////				a->m[j][i] * b->m[1][j] +
    ////				a->m[j][i] * b->m[2][j] +
    ////				a->m[j][i] * b->m[3][j];
    //			float sum = 0;
    //			for (int k = 0; k < 4; ++k) {
    //				sum += a.m[k][row] * b.m[col][k];
    //			}
    //			o.m[col][row] = sum;
    //		}
    //	}
    //	return o;
}

static inline float3 float4x4_mul_vector(const float4x4 m, float3 v) {
    float4 _v = {v.x, v.y, v.z, 0};
    _v = float4x4_mul_float4(m, _v);
    float3 r = {_v.x, _v.y, _v.z};
    return r;
}

static inline float3 float4x4_mul_point(const float4x4 m, float3 v) {
    float4 _v = {v.x, v.y, v.z, 1.f};
    _v = float4x4_mul_float4(m, _v);
    if (fabsf(_v.w) < 1e-5f) return v;
    _v = _v * (1.0f / _v.w);
    float3 o = {_v.x, _v.y, _v.z};
    return o;
    //	float3 o;
    //	o.x = m->m00 * v->x + m->m01 * v->y + m->m02 * v->z + m->m03;
    //	o.y = m->m10 * v->x + m->m11 * v->y + m->m12 * v->z + m->m13;
    //	o.z = m->m20 * v->x + m->m21 * v->y + m->m22 * v->z + m->m23;
    //	float num = m->m30 * v->x + m->m31 * v->y + m->m32 * v->z + m->m33;
    //	if (num > 1e-4 || num < -1e-4)
    //	{
    //		num = 1.f / num;
    //		o.x *= num;
    //		o.y *= num;
    //		o.z *= num;
    //	}
    //	return o;
}

float4x4 makeRotationFromQuaternion(quat q);

static inline float4x4 quat_to_float4x4(const quat q) {
    const float m = float4_length_squared(q);
    assert(fabsf(m - 1) <= 1e-3f);
#if 0
    float4x4 result;
    memset(&result, 0, sizeof(result));

    float x = 2.0f * q.x;
    float y = 2.0f * q.y;
    float z = 2.0f * q.z;
    float qxx = q.x * x;
    float qyy = q.y * y;
    float qzz = q.z * z;
    float qxy = q.x * y;
    float qxz = q.x * z;
    float qyz = q.y * z;
    float qwx = q.w * x;
    float qwy = q.w * y;
    float qwz = q.w * z;

    result.m00 = 1.f - (qyy + qzz);
    result.m10 = qxy + qwz;
    result.m20 = qxz - qwy;

    result.m01 = qxy - qwz;
    result.m11 = 1.f - (qxx + qzz);
    result.m21 = qyz + qwx;

    result.m02 = qxz + qwy;
    result.m12 = qyz - qwx;
    result.m22 = 1.f - (qxx + qyy);
    return result;
#else
    return makeRotationFromQuaternion(q);
#endif
}

static inline quat float4x4_to_quat(const float4x4 *m) {
#if 0
	// see XMQuaternionRotationMatrix in DirectXMath/DirectXMathMisc.inl
	float4 q;
	float r22 = m->m[2][2];
	if (r22 <= 0.f) // x^2 + y^2 >= z^2 + w^2
	{
		float dif10 = m->m[1][1] - m->m[0][0];
		float omr22 = 1.f - r22;
		if (dif10 <= 0.f) // x^2 >= y^2
		{
			float fourXSqr = omr22 - dif10;
			float inv4x = 0.5f / sqrtf(fourXSqr);
			q.x = fourXSqr*inv4x;
			q.y = (m->m[0][1] + m->m[1][0])*inv4x;
			q.z = (m->m[0][2] + m->m[2][0])*inv4x;
			q.w = (m->m[1][2] - m->m[2][1])*inv4x;
		}
		else // y^2 >= x^2
		{
			float fourYSqr = omr22 + dif10;
			float inv4y = 0.5f / sqrtf(fourYSqr);
			q.x = (m->m[0][1] + m->m[1][0])*inv4y;
			q.y = fourYSqr*inv4y;
			q.z = (m->m[1][2] + m->m[2][1])*inv4y;
			q.w = (m->m[2][0] - m->m[0][2])*inv4y;
		}
	}
	else  // z^2 + w^2 >= x^2 + y^2
	{
		float sum10 = m->m[1][1] + m->m[0][0];
		float opr22 = 1.f + r22;
		if (sum10 <= 0.f)  // z^2 >= w^2
		{
			float fourZSqr = opr22 - sum10;
			float inv4z = 0.5f / sqrtf(fourZSqr);
			q.x = (m->m[0][2] + m->m[2][0])*inv4z;
			q.y = (m->m[1][2] + m->m[2][1])*inv4z;
			q.z = fourZSqr*inv4z;
			q.w = (m->m[0][1] - m->m[1][0])*inv4z;
		}
		else  // w^2 >= z^2
		{
			float fourWSqr = opr22 + sum10;
			float inv4w = 0.5f / sqrtf(fourWSqr);
			q.x = (m->m[1][2] - m->m[2][1])*inv4w;
			q.y = (m->m[2][0] - m->m[0][2])*inv4w;
			q.z = (m->m[0][1] - m->m[1][0])*inv4w;
			q.w = fourWSqr*inv4w;
		}
	}
	return float4_normalize(q);
#else
    float fourXSquaredMinus1 = m->m00 - m->m11 - m->m22;
    float fourYSquaredMinus1 = m->m11 - m->m00 - m->m22;
    float fourZSquaredMinus1 = m->m22 - m->m00 - m->m11;
    float fourWSquaredMinus1 = m->m00 + m->m11 + m->m22;

    int biggestIndex = 0;
    float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }

    float biggestVal = sqrtf(fourBiggestSquaredMinus1 + 1.f) * 0.5f;
    float mult = 0.25f / biggestVal;

    quat result;
    switch (biggestIndex) {
        case 0:
            result.w = biggestVal;
            result.x = (m->m21 - m->m12) * mult;
            result.y = (m->m02 - m->m20) * mult;
            result.z = (m->m10 - m->m01) * mult;
            break;
        case 1:
            result.w = (m->m21 - m->m12) * mult;
            result.x = biggestVal;
            result.y = (m->m10 + m->m01) * mult;
            result.z = (m->m02 + m->m20) * mult;
            break;
        case 2:
            result.w = (m->m02 - m->m20) * mult;
            result.x = (m->m10 + m->m01) * mult;
            result.y = biggestVal;
            result.z = (m->m21 + m->m12) * mult;
            break;
        default:  // 3
            result.w = (m->m10 - m->m01) * mult;
            result.x = (m->m02 + m->m20) * mult;
            result.y = (m->m21 + m->m12) * mult;
            result.z = biggestVal;
            break;
    }
    return float4_normalize(result);
#endif
}

float float4x4_determinant(float4x4 m);

static inline void float4x4_decompose(const float4x4 *m, float3 *t, quat *r,
                                      float3 *scale) {
    if (t) {
        //*t = m->columns[3];
        t->x = m->columns[3].x;
        t->y = m->columns[3].y;
        t->z = m->columns[3].z;
    }

    float3 s;
    s.x = sqrtf(m->m00 * m->m00 + m->m10 * m->m10 + m->m20 * m->m20);
    s.y = sqrtf(m->m01 * m->m01 + m->m11 * m->m11 + m->m21 * m->m21);
    s.z = sqrtf(m->m02 * m->m02 + m->m12 * m->m12 + m->m22 * m->m22);

    float det = float4x4_determinant(*m);
    if (det < 0) s.x = -s.x;

    if (scale) {
        *scale = s;
    }

    if (r) {
        float4x4 rot_mat;
        rot_mat.m00 = m->m00 / s.x;
        rot_mat.m10 = m->m10 / s.x;
        rot_mat.m20 = m->m20 / s.x;
        rot_mat.m30 = 0;
        rot_mat.m01 = m->m01 / s.y;
        rot_mat.m11 = m->m11 / s.y;
        rot_mat.m21 = m->m21 / s.y;
        rot_mat.m31 = 0;
        rot_mat.m02 = m->m02 / s.z;
        rot_mat.m12 = m->m12 / s.z;
        rot_mat.m22 = m->m22 / s.z;
        rot_mat.m32 = 0;
        rot_mat.m03 = 0;
        rot_mat.m13 = 0;
        rot_mat.m23 = 0;
        rot_mat.m33 = 1;
        *r = float4x4_to_quat(&rot_mat);
    }
}

// static inline quat quat_conjugate(quat q) {
//    const quat c = {-1, -1, -1, 1};
//    return q * c;
//}

//// TODO: fast recip
// static inline quat quat_inverse(quat q) {
//    return quat_conjugate(q) * (1.0f / float4_length_squared(q));
//}

static inline float _rsqrt(float x) { return 1.0f / sqrt(x); }

static inline quat quat_normalize(quat q) {
    float length_squared = float4_length_squared(q);
    if (fabsf(length_squared) < 1e-5f) {
        quat ret = {0, 0, 0, 1};
        return ret;
    }
    return q * _rsqrt(length_squared);
}

#define DEG2RAD ((float)0.01745329251994329576923690768489)
#define deg2rad(d) ((d)*DEG2RAD)

#define RAD2DEG ((float)57.295779513082320876798154814105)
#define rad2deg(d) ((d)*RAD2DEG)

quat euler_to_quat(float3 e);
// static inline quat euler_to_quat(float3 e) {
//#define SinCos(d, s, c)               \
//    do {                              \
//        const float rad = deg2rad(d); \
//        s = sinf(rad);                \
//        c = cosf(rad);                \
//    } while (0);
//    float sx, cx, sy, cy, sz, cz;
//    SinCos(e.x / 2.f, sx, cx);
//    SinCos(e.y / 2.f, sy, cy);
//    SinCos(e.z / 2.f, sz, cz);
//#undef SinCos
//    quat q = {cy * sx * cz + sy * cx * sz, -cy * sx * sz + sy * cx * cz,
//              cy * cx * sz - sy * sx * cz, cy * cx * cz + sy * sx * sz};
//    q = quat_normalize(q);
//    return q;
//}

float3 makeEulerFromQuaternion(quat q);

static inline float3 quat_to_euler(quat q) {
    return float3_mul1(makeEulerFromQuaternion(q), RAD2DEG);
}

// The angle between p and q interpreted as 4-dimensional vectors.
static inline float quat_angle(quat p, quat q) {
    return 2 * atan2(float4_length(p - q), float4_length(p + q));
}

quat quat_angle_axis(float angle, float3 axis);
float3 quat_mul_point(quat q, float3 point);
quat quat_mul_quat(quat a, quat b);

static inline float _recip(float x) { return 1.0f / x; }

// sin(x)/x
static inline float _sinc(float x) {
    if (x == 0) return 1;
    return sin(x) / x;
}

static inline quat quat_negate(quat q) { return -q; }

/*! @abstract Spherical lerp between q0 and q1.
 *
 *  @discussion This function may interpolate along either the longer or
 *  shorter path between q0 and q1; it is used as an implementation detail
 *  in `simd_slerp` and `simd_slerp_longest`; you should use those functions
 *  instead of calling this directly.                                         */
static inline quat _quat_slerp_internal(quat q0, quat q1, float t) {
    float s = 1 - t;
    float a = quat_angle(q0, q1);
    float r = _recip(_sinc(a));
    return quat_normalize(_sinc(s * a) * r * s * q0 +
                          _sinc(t * a) * r * t * q1);
}

static inline quat quat_slerp(quat a, quat b, float t) {
    //    if (float4_dot(q0, q1) >= 0) return _quat_slerp_internal(q0, q1, t);
    //    return _quat_slerp_internal(q0, quat_negate(q1), t);
    // from Assimp

    float cosTheta = float4_dot(a, b);

    // adjust signs (if necessary)
    quat end = b;
    if (cosTheta < 0.0f) {
        cosTheta = -cosTheta;
        end.x = -end.x;
        end.y = -end.y;
        end.z = -end.z;
        end.w = -end.w;
    }

    float sclp, sclq;
    if ((1.0f - cosTheta) > 0.0001f) {
        // Standard case (slerp)
        float theta, sinTheta;
        theta = acos(cosTheta);
        sinTheta = sin(theta);
        sclp = sin((1.0f - t) * theta) / sinTheta;
        sclq = sin(t * theta) / sinTheta;
    } else {
        // Very close, do LERP (because it's faster)
        sclp = 1.0f - t;
        sclq = t;
    }

    quat ret = {sclp * a.x + sclq * end.x, sclp * a.y + sclq * end.y,
                sclp * a.z + sclq * end.z, sclp * a.w + sclq * end.w};
    ret = quat_normalize(ret);
    return ret;
}

float4x4 float4x4_ortho(float left, float right, float bottom, float top,
                        float nearZ, float farZ);
float4x4 float4x4_look_to(float3 eye, float3 forward, float3 up);
float4x4 float4x4_look_at(float3 eye, float3 focus, float3 up);
float4x4 float4x4_perspective(float fovy, float aspect, float zNear,
                              float zFar);

#ifdef __cplusplus
}
#endif

#endif /* SIMD_MATH_H */
