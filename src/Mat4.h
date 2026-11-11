#pragma once
#include <cmath>
#include "Vec3.h"

const float PI = 3.14159265f;

struct Mat4
{
	float m[16];

	Mat4()
	{
		for (int i = 0; i < 16; i++)
		{
			m[i] = 0.0f;
		}
	}

	static Mat4 identity()
	{
		Mat4 mat4;
		for (int col = 0; col < 4; col++)
		{
			for (int row = 0; row < 4; row++)
			{
				if (col == row)
				{
					mat4.m[col * 4 + row] = 1.0f;
				}
			}
		}

		return mat4;
	}

	static Mat4 translate(Vec3 trans)
	{
		Mat4 mat4 = identity();
		mat4.m[12] = trans.x;
		mat4.m[13] = trans.y;
		mat4.m[14] = trans.z;

		return mat4;
	}

	static Mat4 scale(Vec3 scale)
	{
		Mat4 mat4 = identity();
		mat4.m[0] = scale.x;
		mat4.m[5] = scale.y;
		mat4.m[10] = scale.z;

		return mat4;
	}

	static Mat4 rotateZ(float rad)
	{
		float c = std::cos(rad);
		float s = std::sin(rad);
		Mat4 mat4 = identity();
		mat4.m[0] = c;
		mat4.m[1] = s;
		mat4.m[4] = -s;
		mat4.m[5] = c;

		return mat4;
	}
};

inline Mat4 operator* (const Mat4& a, const Mat4& b)
{
	Mat4 returnMat4;
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; k++)
			{
				sum += a.m[k * 4 + row] * b.m[col * 4 + k];
			}
			
			returnMat4.m[col * 4 + row] = sum;
		}
	}

	return returnMat4;
}

inline Vec3 transformPoint(const Mat4& mat4, const Vec3& vec3)
{
	float vecArray[] = { vec3.x, vec3.y, vec3.z, 1 };
	float retArray[] = { 0, 0, 0 };
	
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			retArray[i] += mat4.m[j * 4 + i] * vecArray[j];
		}
	}

	return Vec3(retArray[0], retArray[1], retArray[2]);
}