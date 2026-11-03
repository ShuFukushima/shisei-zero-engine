#pragma once

#include <iostream>
#include <cmath>

struct Vec3
{
	float x;
	float y;
	float z;

	Vec3()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
	
	Vec3(float startX, float startY, float startZ)
	{
		x = startX;
		y = startY;
		z = startZ;
	}

	float length() const
	{
		return std::sqrt(x * x + y * y + z * z);
	}
};

inline Vec3 operator+ (const Vec3& a, const Vec3& b)
{
	return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vec3 operator- (const Vec3& a, const Vec3& b)
{
	return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline Vec3 operator* (const Vec3& v, float s)
{
	return Vec3(v.x * s, v.y * s, v.z * s);
}

inline Vec3 operator* (float s, const Vec3& v)
{
	return v * s;
}

inline std::ostream& operator<< (std::ostream& out, const Vec3& v)
{
	out << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return out;
}
