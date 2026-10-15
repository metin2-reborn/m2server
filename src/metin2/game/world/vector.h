#pragma once

#include <cstdint>

namespace metin2::game::inline world {

struct vector {
	float x;
	float y;
	float z;
};

extern void		normalize(vector * pV1, vector * pV2);
extern float    get_degree_from_position(float x, float y);
extern float    get_degree_from_position_xy(int32_t sx, int32_t sy, int32_t ex, int32_t ey);
extern void     get_delta_by_degree(float fDegree, float fDistance, float *x, float *y);
extern float	get_degree_delta(float iDegree, float iDegree2);
}
