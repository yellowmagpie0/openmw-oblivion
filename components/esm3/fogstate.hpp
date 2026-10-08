#ifndef OPENMW_ESM_FOGSTATE_H
#define OPENMW_ESM_FOGSTATE_H

#include <cstdint>
#include <vector>
#include <osg/Image>

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    struct FogTexture
    {
        static constexpr int Resolution = 32;
        int32_t mX, mY; // Only used for interior cells
        std::vector<char> mImageData;
        // Ephemeral native restore resource, already oriented for local-map access.
        bool mPrepared = false;
        osg::ref_ptr<osg::Image> mPreparedImage = nullptr;
    };

    // format 0, saved games only
    // Fog of war state
    struct FogState
    {
        // Only used for interior cells
        float mNorthMarkerAngle;
        struct Bounds
        {
            float mMinX;
            float mMinY;
            float mMaxX;
            float mMaxY;
        } mBounds;
        float mCenterX;
        float mCenterY;

        std::vector<FogTexture> mFogTextures;

        void load(ESMReader& esm);
        void save(ESMWriter& esm, bool interiorCell) const;
    };

    FogState prepareFogState(const FogState& state, bool interior);
    bool isUsableFogImage(const osg::Image& image);
}

#endif
