#include <metin2/game/world/vector.h>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace metin2::game::inline world {

    static auto degree_to_radian(float degree) -> float { return degree * (std::numbers::pi_v<float> / 180.0f); }
    static auto radian_to_degree(float radian) -> float { return radian * (180.0f / std::numbers::pi_v<float>); }

    void normalize(vector *v1, vector *v2) {
        float l = sqrtf(v1->x * v1->x + v1->y * v1->y + v1->z * v1->z + 1.0e-12);

        v2->x = v1->x / l;
        v2->y = v1->y / l;
        v2->z = v1->z / l;
    }

    auto dot_product(vector *v1, vector *v2) -> float {
        return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
    }

    auto get_degree_from_position(float x, float y) -> float {
        vector vt_dir;
        vector vt_stan;
        float ret;

        vt_dir.x = x;
        vt_dir.y = y;
        vt_dir.z = 0.0f;

        normalize(&vt_dir, &vt_dir);

        vt_stan.x = 0.0f;
        vt_stan.y = 1.0f;
        vt_stan.z = 0.0f;

        ret = radian_to_degree(acosf(dot_product(&vt_dir, &vt_stan)));

        if (vt_dir.x < 0.0f)
            ret = 360.0f - ret;

        return (ret);
    }

    auto get_degree_from_position_xy(int32_t sx, int32_t sy, int32_t ex, int32_t ey) -> float {
        return get_degree_from_position(ex - sx, ey - sy);
    }

    void get_delta_by_degree(float degree, float distance, float *x, float *y) {
        float radian = degree_to_radian(degree);

        *x = distance * sin(radian);
        *y = distance * cos(radian);
    }

    auto get_degree_delta(float degree, float degree2) -> float {
        if (degree > 180.0f)
            degree = degree - 360.0f;

        if (degree2 > 180.0f)
            degree2 = degree2 - 360.0f;

        return fabs(degree - degree2);
    }
}
