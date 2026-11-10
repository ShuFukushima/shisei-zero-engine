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