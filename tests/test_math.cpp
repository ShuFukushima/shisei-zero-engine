#include <iostream>
#include "Vec3.h"

int main()
{
	Vec3 zero;
	Vec3 sum = Vec3(1, 2, 3) + Vec3(4, 5, 6);
	Vec3 diff = Vec3(3, 5, 9) - Vec3(1, 2, 3);
	Vec3 right = Vec3(3, 2, 0) * 2;
	Vec3 left = 2 * Vec3(3, 2, 0);
	float length = Vec3(2, 3, 6).length();

	std::cout << "zero = " << zero << std::endl;
	std::cout << "sum = " << sum << std::endl;
	std::cout << "diff = " << diff << std::endl;
	std::cout << "right = " << right << std::endl;
	std::cout << "left = " << left << std::endl;
	std::cout << "length = " << length << std::endl;

	return 0;
}