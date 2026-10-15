#include <metin2/game/world/vector.h>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace metin2::game::inline world {



static float degree_to_radian(float degree) { return degree * (std::numbers::pi_v<float> / 180.0f); }
static float radian_to_degree(float radian) { return radian * (180.0f / std::numbers::pi_v<float>); }

void normalize(vector * pV1, vector * pV2)
{
	float l = sqrtf(pV1->x * pV1->x + pV1->y * pV1->y + pV1->z * pV1->z + 1.0e-12);

	pV2->x = pV1->x / l;
	pV2->y = pV1->y / l;
	pV2->z = pV1->z / l;
}

float dot_product(vector * pV1, vector * pV2)
{
	return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z;
}

float get_degree_from_position(float x, float y)
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

float get_degree_from_position_xy(int32_t sx, int32_t sy, int32_t ex, int32_t ey)
{
	return get_degree_from_position(ex - sx, ey - sy);
}

void get_delta_by_degree(float fDegree, float fDistance, float *x, float *y)
{
	float fRadian = degree_to_radian(fDegree);

	*x = fDistance * sin(fRadian);
	*y = fDistance * cos(fRadian);
}

float get_degree_delta(float iDegree, float iDegree2)
{
	if (iDegree > 180.0f)
		iDegree = iDegree - 360.0f;

	if (iDegree2 > 180.0f)
		iDegree2 = iDegree2 - 360.0f;

	return fabs(iDegree - iDegree2);
}
}
