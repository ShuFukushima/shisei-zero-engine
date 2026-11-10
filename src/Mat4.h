#pragma once

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