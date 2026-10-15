#pragma once

#include <cstdint>

namespace building {
    inline constexpr int OBJECT_MATERIAL_MAX_NUM = 5;

    using TLand = struct SLand {
        uint32_t dwID;
        int32_t lMapIndex;
        int32_t x, y;
        int32_t width, height;
        uint32_t dwGuildID;
        uint8_t bGuildLevelLimit;
        uint32_t dwPrice;
    };

    using TObjectMaterial = struct SObjectMaterial {
        uint32_t dwItemVnum;
        uint32_t dwCount;
    };

    using TObjectProto = struct SObjectProto {
        uint32_t dwVnum;
        uint32_t dwPrice;

        TObjectMaterial kMaterials[OBJECT_MATERIAL_MAX_NUM];

        uint32_t dwUpgradeVnum;
        uint32_t dwUpgradeLimitTime;
        int32_t lLife;
        int32_t lRegion[4];

        uint32_t dwNPCVnum;
        int32_t lNPCX;
        int32_t lNPCY;

        uint32_t dwGroupVnum;         // Only one per group can be built
        uint32_t dwDependOnGroupVnum; // Group that must already be built
    };

    using TObject = struct SObject {
        uint32_t dwID;
        uint32_t dwLandID;
        uint32_t dwVnum;
        int32_t lMapIndex;
        int32_t x, y;

        float xRot;
        float yRot;
        float zRot;
        int32_t lLife;
    };
};
