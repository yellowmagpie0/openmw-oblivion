#ifndef OPENMW_MWRENDER_TES4FACEGEN_H
#define OPENMW_MWRENDER_TES4FACEGEN_H

#include <string>
#include <string_view>
#include <vector>

#include <osg/Geometry>

namespace ESM4
{
    struct Npc;
    struct Race;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    struct Tes4FaceMorph
    {
        osg::observer_ptr<osg::Geometry> mGeometry;
        osg::ref_ptr<osg::Vec3Array> mBaseVertices;
        std::vector<osg::ref_ptr<osg::Vec3Array>> mOffsets;
        std::vector<std::string> mNames;
    };

    void applyTes4FaceGen(std::string_view model, osg::Node& node, const ESM4::Npc& traits,
        const ESM4::Race& race, bool isFemale, std::string_view texture, bool bodyTexture,
        Resource::ResourceSystem* resourceSystem, std::vector<Tes4FaceMorph>& morphs);
    float getTes4FaceMorphWeight(std::string_view name, std::string_view active, std::string_view next,
        float visemeBlend, float speechWeight, float blink);
    void animateTes4FaceGen(std::vector<Tes4FaceMorph>& morphs, float faceTime, bool speaking,
        bool hasLipCompanion, float loudness, float speechTime);
}

#endif
