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

inline float dot(const Vec3& a, const Vec3& b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
	float x = (a.y * b.z) - (a.z * b.y);
	float y = (a.z * b.x) - (a.x * b.z);
	float z = (a.x * b.y) - (a.y * b.x);

	return Vec3(x, y, z);
}

inline bool nearlyEqual(float a, float b)
{
	if (std::fabs(a - b) < 0.00001f)
	{
		return true;
	}
	else
	{
		return false;
	}
}

inline Vec3 normalize(const Vec3& a)
{
	float length = a.length();
	
	if (nearlyEqual(length, 0.0f))
	{
		return Vec3();
	}
	else
	{
		return Vec3(a.x * (1.0f / length), a.y * (1.0f / length), a.z * (1.0f / length));
	}
}
