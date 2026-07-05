#pragma once
#include <cmath>

namespace sdk {
    struct vector2 {
        float x = 0.f, y = 0.f;

        vector2 operator+(const vector2& rhs) const { return { x + rhs.x, y + rhs.y }; }
        vector2 operator-(const vector2& rhs) const { return { x - rhs.x, y - rhs.y }; }
        vector2 operator*(float s) const { return { x * s, y * s }; }
        vector2& operator+=(const vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }
        float distance(const vector2& o) const { float dx = x - o.x, dy = y - o.y; return std::sqrt(dx * dx + dy * dy); }
    };

    struct vector3 {
        float x = 0.f, y = 0.f, z = 0.f;

        vector3 operator+(const vector3& rhs) const { return { x + rhs.x, y + rhs.y, z + rhs.z }; }
        vector3 operator-(const vector3& rhs) const { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
        vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
        vector3 operator/(float s) const { return { x / s, y / s, z / s }; }
        vector3& operator+=(const vector3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
        vector3 operator-() const { return { -x, -y, -z }; }

        float magnitude() const { return std::sqrt(x * x + y * y + z * z); }
        float distance(const vector3& o) const { return (*this - o).magnitude(); }

        vector3 normalize() const {
            float mag = magnitude();
            if (mag < 0.0001f) return {};
            return { x / mag, y / mag, z / mag };
        }

        vector3 cross(const vector3& v) const {
            return { y * v.z - z * v.y, -(x * v.z - z * v.x), x * v.y - y * v.x };
        }
    };

    struct matrix4 { float data[16]; };

    struct matrix3 {
        float data[9];
        vector3 operator*(const vector3& v) const {
            return { data[0] * v.x + data[1] * v.y + data[2] * v.z,
                     data[3] * v.x + data[4] * v.y + data[5] * v.z,
                     data[6] * v.x + data[7] * v.y + data[8] * v.z };
        }
    };

    struct vector4 { float x, y, z, w; };
}
