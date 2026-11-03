#include <iostream>
#include "Vec3.h"
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
}

int main()
{
	testVec3();
	std::cout << "all tests passed" << std::endl;

	return 0;
}