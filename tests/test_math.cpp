#include <iostream>
#include "Vec3.h"
#include "Mat4.h"
#include <assert.h>

void testVec3()
{
	Vec3 zero;
	assert(zero.x == 0.0f);
	assert(zero.y == 0.0f);
	assert(zero.z == 0.0f);
	Vec3 sum = Vec3(1, 2, 3) + Vec3(4, 5, 6);
	assert(sum.x == 5.0f);
	assert(sum.y == 7.0f);
	assert(sum.z == 9.0f);
	Vec3 diff = Vec3(3, 5, 9) - Vec3(1, 2, 3);
	assert(diff.x == 2.0f);
	assert(diff.y == 3.0f);
	assert(diff.z == 6.0f);
	Vec3 right = Vec3(3, 2, 0) * 2;
	assert(right.x == 6.0f);
	assert(right.y == 4.0f);
	assert(right.z == 0.0f);
	Vec3 left = 2 * Vec3(3, 2, 0);
	assert(left.x == 6.0f);
	assert(left.y == 4.0f);
	assert(left.z == 0.0f);
	float length1 = Vec3(2, 3, 6).length();
	float length2 = Vec3(1, 2, 2).length();
	assert(length1 == 7);
	assert(length2 == 3);
	float dot1 = dot(Vec3(1, 2, 2), Vec3(2, -2, 1));
	float dot2 = dot(Vec3(3, 4, 0), Vec3(3, 4, 0));
	assert(dot1 == 0.0f);
	assert(dot2 == 25.0f);
	Vec3 cross1 = cross(Vec3(1, 0, 0), Vec3(0, 1, 0));
	Vec3 a = Vec3(1, 2, 3);
	Vec3 b = Vec3(4, 5, 6);
	Vec3 cross2 = cross(a, b);
	assert(cross1.x == 0.0f);
	assert(cross1.y == 0.0f);
	assert(cross1.z == 1.0f);
	assert(cross2.x == -3.0f);
	assert(cross2.y == 6.0f);
	assert(cross2.z == -3.0f);
	assert(dot(cross2, a) == 0.0f);
	assert(dot(cross2, b) == 0.0f);
	Vec3 cross3 = cross(Vec3(1, 2, 3), Vec3(2, 4, 6));
	assert(cross3.x == 0.0f);
	assert(cross3.y == 0.0f);
	assert(cross3.z == 0.0f);
	float normalize1Len = normalize(Vec3(1, 1, 1)).length();
	Vec3 normalize2 = normalize(Vec3(-6, 8, 0));
	Vec3 normalize3 = normalize(Vec3(0, 0, 0));
	assert(nearlyEqual(normalize1Len, 1.0f));
	assert(nearlyEqual(normalize2.x, -0.6f));
	assert(nearlyEqual(normalize2.y, 0.8f));
	assert(nearlyEqual(normalize2.z, 0.0f));
	assert(normalize3.x == 0.0f);
	assert(normalize3.y == 0.0f);
	assert(normalize3.z == 0.0f);
	Vec3 normalA = normalize(a);
	Vec3 normalB = normalize(b);
	Vec3 crossAB = cross(normalA, normalB);
	assert(nearlyEqual(dot(crossAB, normalA), 0.0f));
	assert(nearlyEqual(dot(crossAB, normalB), 0.0f));
}

void testMat4()
{
	Mat4 zeroMat4;
	for (int i = 0; i < 16; i++)
	{
		assert(zeroMat4.m[i] == 0.0f);
	}
	Mat4 identityM4 = Mat4::identity();
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			if (col == row)
			{
				assert(identityM4.m[col * 4 + row] == 1.0f);
			}
			else
			{
				assert(identityM4.m[col * 4 + row] == 0.0f);
			}
		}
	}
}

int main()
{
	testVec3();
	testMat4();
	std::cout << "all tests passed" << std::endl;

	return 0;
}