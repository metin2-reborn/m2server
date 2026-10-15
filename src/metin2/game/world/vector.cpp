#include <metin2/game/world/vector.h>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace metin2::game::inline world {



static auto degree_to_radian(float degree) -> float { return degree * (std::numbers::pi_v<float> / 180.0f); }
static auto radian_to_degree(float radian) -> float { return radian * (180.0f / std::numbers::pi_v<float>); }

void normalize(vector * v1, vector * v2)
{
	float l = sqrtf(v1->x * v1->x + v1->y * v1->y + v1->z * v1->z + 1.0e-12);

	v2->x = v1->x / l;
	v2->y = v1->y / l;
	v2->z = v1->z / l;
}

auto dot_product(vector * v1, vector * v2) -> float
{
	return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

auto get_degree_from_position(float x, float y) -> float
{   
	vector	vtDir;
	vector	vtStan;
	float	ret;

	vtDir.x = x;
	vtDir.y = y; 
	vtDir.z = 0.0f;

	normalize(&vtDir, &vtDir); 

	vtStan.x = 0.0f;
	vtStan.y = 1.0f;
	vtStan.z = 0.0f;

	ret = radian_to_degree(acosf(dot_product(&vtDir, &vtStan)));

	if (vtDir.x < 0.0f)
		ret = 360.0f - ret;

	return (ret);
}   

auto get_degree_from_position_xy(int32_t sx, int32_t sy, int32_t ex, int32_t ey) -> float
{
	return get_degree_from_position(ex - sx, ey - sy);
}

void get_delta_by_degree(float degree, float distance, float *x, float *y)
{
	float fRadian = degree_to_radian(degree);

	*x = distance * sin(fRadian);
	*y = distance * cos(fRadian);
}

auto get_degree_delta(float degree, float degree2) -> float
{
	if (degree > 180.0f)
		degree = degree - 360.0f;

	if (degree2 > 180.0f)
		degree2 = degree2 - 360.0f;

	return fabs(degree - degree2);
}
}
