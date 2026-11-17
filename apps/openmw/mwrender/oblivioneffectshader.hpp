#ifndef OPENMW_MWRENDER_OBLIVIONEFFECTSHADER_H
#define OPENMW_MWRENDER_OBLIVIONEFFECTSHADER_H

#include <string_view>
#include <cstddef>

namespace osg { class Group; }
namespace ESM4 { struct EffectShader; }
namespace Resource { class ResourceSystem; }

namespace MWRender
{
    // The currently supported TES4 visual is a particle-only EFSH. Reject
    // membrane shaders explicitly rather than substituting an actor glow.
    void validateOblivionParticleShader(const ESM4::EffectShader& shader);

    // One instance per model and stable shader identity. Simulation time comes
    // from the world, never the render clock (menus must pause the effect).
    // Recreated models reconstruct the emitter from its saved elapsed time.
    void advanceOblivionParticleShader(osg::Group& model, std::string_view identity,
        const ESM4::EffectShader& shader, Resource::ResourceSystem& resources,
        double elapsed, double seconds);
    void removeOblivionParticleShader(osg::Group& model, std::string_view identity);
    void clearOblivionParticleShaders(osg::Group& model);

    struct OblivionParticleShaderStats
    {
        unsigned int mInstances = 0;
        int mParticles = 0;
        std::size_t mSurfaceTriangles = 0;
    };
    OblivionParticleShaderStats inspectOblivionParticleShader(const osg::Group& model, std::string_view identity);
}

#endif
