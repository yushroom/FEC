#include "simd_math.h"

enum RotationOrder {
    RotationOrderXYZ = 0,
    RotationOrderYZX,
    RotationOrderZXY,  // unity
    RotationOrderXZY,
    RotationOrderYXZ,
    RotationOrderZYX
};

float float4x4_determinant(float4x4 this) {
    float *te = this.a;

    float n11 = te[0], n12 = te[4], n13 = te[8], n14 = te[12];
    float n21 = te[1], n22 = te[5], n23 = te[9], n24 = te[13];
    float n31 = te[2], n32 = te[6], n33 = te[10], n34 = te[14];
    float n41 = te[3], n42 = te[7], n43 = te[11], n44 = te[15];

    // TODO: make this more efficient
    //( based on
    // http://www.euclideanspace.com/maths/algebra/matrix/functions/inverse/fourD/index.htm
    //)

    return (n41 * (+n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 +
                   n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34) +
            n42 * (+n11 * n23 * n34 - n11 * n24 * n33 + n14 * n21 * n33 -
                   n13 * n21 * n34 + n13 * n24 * n31 - n14 * n23 * n31) +
            n43 * (+n11 * n24 * n32 - n11 * n22 * n34 - n14 * n21 * n32 +
                   n12 * n21 * n34 + n14 * n22 * n31 - n12 * n24 * n31) +
            n44 * (-n13 * n22 * n31 - n11 * n23 * n32 + n11 * n22 * n33 +
                   n13 * n21 * n32 - n12 * n21 * n33 + n12 * n23 * n31));
}

float4x4 float4x4_compose(float3 position, quat quaternion, float3 scale) {
    float4x4 m;
    float *te = m.a;
    float x = quaternion.x, y = quaternion.y, z = quaternion.z,
          w = quaternion.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    float sx = scale.x, sy = scale.y, sz = scale.z;

    te[0] = (1 - (yy + zz)) * sx;
    te[1] = (xy + wz) * sx;
    te[2] = (xz - wy) * sx;
    te[3] = 0;

    te[4] = (xy - wz) * sy;
    te[5] = (1 - (xx + zz)) * sy;
    te[6] = (yz + wx) * sy;
    te[7] = 0;

    te[8] = (xz + wy) * sz;
    te[9] = (yz - wx) * sz;
    te[10] = (1 - (xx + yy)) * sz;
    te[11] = 0;

    te[12] = position.x;
    te[13] = position.y;
    te[14] = position.z;
    te[15] = 1;
    return m;
}

// void decomposeMatrix(float3 *position);

float4x4 makeRotationFromQuaternion(quat q) {
    float3 zero = {0, 0, 0};
    float3 scale = {1, 1, 1};
    return float4x4_compose(zero, q, scale);
}

float clamp(float a, float min, float max) {
    if (a < min) return min;
    if (a > max) return max;
    return a;
}

// from three.js
float3 makeEulerFromRotation(float4x4 m) {
    // assumes the upper 3x3 of m is a pure rotation matrix (i.e, unscaled)
    float3 this = {0, 0, 0};
    float *te = m.a;
    float m11 = te[0], m12 = te[4], m13 = te[8];
    float m21 = te[1], m22 = te[5], m23 = te[9];
    float m31 = te[2], m32 = te[6], m33 = te[10];

    const enum RotationOrder order = RotationOrderZXY;

    if (order == RotationOrderXYZ) {
        this.y = asin(clamp(m13, -1, 1));

        if (fabsf(m13) < 0.9999999f) {
            this.x = atan2(-m23, m33);
            this.z = atan2(-m12, m11);

        } else {
            this.x = atan2(m32, m22);
            this.z = 0;
        }

    } else if (order == RotationOrderYXZ) {
        this.x = asin(-clamp(m23, -1, 1));

        if (fabsf(m23) < 0.9999999f) {
            this.y = atan2(m13, m33);
            this.z = atan2(m21, m22);

        } else {
            this.y = atan2(-m31, m11);
            this.z = 0;
        }

    } else if (order == RotationOrderZXY) {
        this.x = asin(clamp(m32, -1, 1));

        if (fabsf(m32) < 0.9999999f) {
            this.y = atan2(-m31, m33);
            this.z = atan2(-m12, m22);

        } else {
            this.y = 0;
            this.z = atan2(m21, m11);
        }

    } else if (order == RotationOrderZYX) {
        this.y = asin(-clamp(m31, -1, 1));

        if (fabsf(m31) < 0.9999999f) {
            this.x = atan2(m32, m33);
            this.z = atan2(m21, m11);

        } else {
            this.x = 0;
            this.z = atan2(-m12, m22);
        }

    } else if (order == RotationOrderYZX) {
        this.z = asin(clamp(m21, -1, 1));

        if (fabsf(m21) < 0.9999999f) {
            this.x = atan2(-m23, m22);
            this.y = atan2(-m31, m11);

        } else {
            this.x = 0;
            this.y = atan2(m13, m33);
        }

    } else if (order == RotationOrderXZY) {
        this.z = asin(-clamp(m12, -1, 1));

        if (fabsf(m12) < 0.9999999f) {
            this.x = atan2(m32, m22);
            this.y = atan2(m13, m11);

        } else {
            this.x = atan2(-m23, m33);
            this.y = 0;
        }
    }

    return this;
}

float3 makeEulerFromQuaternion(quat q) {
    return makeEulerFromRotation(makeRotationFromQuaternion(q));
}

quat euler_to_quat(float3 e) {
#define SinCos(d, s, c)               \
    do {                              \
        const float rad = deg2rad(d); \
        s = sinf(rad);                \
        c = cosf(rad);                \
    } while (0);
    float s1, c1, s2, c2, s3, c3;
    SinCos(e.x / 2.f, s1, c1);
    SinCos(e.y / 2.f, s2, c2);
    SinCos(e.z / 2.f, s3, c3);

    quat q;
    const enum RotationOrder order = RotationOrderZXY;
    if (order == RotationOrderXYZ) {
        q.x = s1 * c2 * c3 + c1 * s2 * s3;
        q.y = c1 * s2 * c3 - s1 * c2 * s3;
        q.z = c1 * c2 * s3 + s1 * s2 * c3;
        q.w = c1 * c2 * c3 - s1 * s2 * s3;

    } else if (order == RotationOrderYXZ) {
        q.x = s1 * c2 * c3 + c1 * s2 * s3;
        q.y = c1 * s2 * c3 - s1 * c2 * s3;
        q.z = c1 * c2 * s3 - s1 * s2 * c3;
        q.w = c1 * c2 * c3 + s1 * s2 * s3;

    } else if (order == RotationOrderZXY) {
        q.x = s1 * c2 * c3 - c1 * s2 * s3;
        q.y = c1 * s2 * c3 + s1 * c2 * s3;
        q.z = c1 * c2 * s3 + s1 * s2 * c3;
        q.w = c1 * c2 * c3 - s1 * s2 * s3;

    } else if (order == RotationOrderZYX) {
        q.x = s1 * c2 * c3 - c1 * s2 * s3;
        q.y = c1 * s2 * c3 + s1 * c2 * s3;
        q.z = c1 * c2 * s3 - s1 * s2 * c3;
        q.w = c1 * c2 * c3 + s1 * s2 * s3;

    } else if (order == RotationOrderYZX) {
        q.x = s1 * c2 * c3 + c1 * s2 * s3;
        q.y = c1 * s2 * c3 + s1 * c2 * s3;
        q.z = c1 * c2 * s3 - s1 * s2 * c3;
        q.w = c1 * c2 * c3 - s1 * s2 * s3;

    } else if (order == RotationOrderXZY) {
        q.x = s1 * c2 * c3 - c1 * s2 * s3;
        q.y = c1 * s2 * c3 - s1 * c2 * s3;
        q.z = c1 * c2 * s3 + s1 * s2 * c3;
        q.w = c1 * c2 * c3 + s1 * s2 * s3;
    }
    return q;
}

float4x4 float4x4_ortho_rh(float left, float right, float bottom, float top,
                           float nearZ, float farZ) {
    float4x4 m = float4x4_diagonal(2 / (right - left), 2 / (top - bottom),
                                   -1 / (farZ - nearZ), 1);
    m.a[12] = (left + right) / (left - right);
    m.a[13] = (top + bottom) / (bottom - top);
    m.a[14] = nearZ / (nearZ - farZ);
    return m;
}

float4x4 float4x4_ortho_lh(float left, float right, float bottom, float top,
                           float nearZ, float farZ) {
    float4x4 m = float4x4_diagonal(2 / (right - left), 2 / (top - bottom),
                                   1 / (farZ - nearZ), 1);
    m.a[12] = (left + right) / (left - right);
    m.a[13] = (top + bottom) / (bottom - top);
    m.a[14] = nearZ / (nearZ - farZ);
    return m;
}

float4x4 float4x4_ortho(float left, float right, float bottom, float top,
                        float nearZ, float farZ) {
    // TODO: ???
    return float4x4_ortho_rh(left, right, bottom, top, nearZ, farZ);
}

float4x4 float4x4_perspective_lh(float fovy, float aspect, float zNear,
                                 float zFar) {
    float angle = deg2rad(fovy * 0.5f);
    float ys = 1.f / tanf(angle);
    float xs = ys / aspect;
    float zs = zFar / (zFar - zNear);
    float4x4 m = float4x4_diagonal(xs, ys, zs, 0);
    m.m32 = 1.0f;
    m.m23 = -zNear * zs;
    return m;
}

float4x4 float4x4_perspective_rh(float fovy, float aspect, float zNear,
                                 float zFar) {
    float angle = deg2rad(fovy * 0.5f);
    float ys = 1.f / tanf(angle);
    float xs = ys / aspect;
    float zs = zFar / (zNear - zFar);
    float4x4 m = float4x4_diagonal(xs, ys, zs, 0);
    m.m32 = -1;
    m.m23 = zNear * zs;
    return m;
}

float4x4 float4x4_perspective(float fovy, float aspect, float zNear,
                              float zFar) {
    assert(aspect > 1e-3);
    assert(zNear > 0 && zNear < zFar);
    return float4x4_perspective_rh(fovy, aspect, zNear, zFar);
}

float4x4 float4x4_look_to_lh(float3 eye, float3 forward, float3 up) {
    float3 zAxis = float3_normalize(forward);
    float3 xAxis = float3_normalize(float3_cross(up, zAxis));
    float3 yAxis = float3_cross(zAxis, xAxis);
    float4 P = {xAxis.x, yAxis.x, zAxis.x, 0};
    float4 Q = {xAxis.y, yAxis.y, zAxis.y, 0};
    float4 R = {xAxis.z, yAxis.z, zAxis.z, 0};
    float4 S = {-float3_dot(xAxis, eye), -float3_dot(yAxis, eye),
                -float3_dot(zAxis, eye), 1.0f};
    float4x4 m;
    m.columns[0] = P;
    m.columns[1] = Q;
    m.columns[2] = R;
    m.columns[3] = S;
    return m;
}

float4x4 float4x4_look_to_rh(float3 eye, float3 forward, float3 up) {
    return float4x4_look_to_lh(eye, float3_negate(forward), up);
}

float4x4 float4x4_look_to(float3 eye, float3 forward, float3 up) {
    return float4x4_look_to_rh(eye, forward, up);
}

float4x4 float4x4_look_at_lh(float3 eye, float3 focus, float3 up) {
    float3 forward = float3_subtract(focus, eye);
    return float4x4_look_to_lh(eye, forward, up);
}

float4x4 float4x4_look_at_rh(float3 eye, float3 focus, float3 up) {
    float3 forward = float3_subtract(eye, focus);
    return float4x4_look_to_lh(eye, forward, up);  // note: still use lh version
}

float4x4 float4x4_look_at(float3 eye, float3 focus, float3 up) {
    return float4x4_look_at_rh(eye, focus, up);
}
