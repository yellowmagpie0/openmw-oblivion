#ifndef OPENMW_TESTS_OBLIVIONWORLDFIXTURE_HPP
#define OPENMW_TESTS_OBLIVIONWORLDFIXTURE_HPP

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwworld/worldimp.hpp"

#include <components/esm/gameprofile.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace MWLua { class LuaManager; }
namespace MWSound { class SoundManager; }

namespace TestingOpenMW
{
    // A real native content-load path with no renderer, input, physics or audio
    // device. Registration tests exercise actual Actors admission without an
    // animation/controller; this remains a headless integration fixture.
    struct NativeWorldFixture
    {
        const std::filesystem::path mDirectory = TestingOpenMW::currentTestDirPath();
        MWBase::Environment mEnvironment;
        VFS::Manager mVfs;
        Resource::ResourceSystem mResources{&mVfs, 0., nullptr};
        MWWorld::World mWorld{&mResources, -1, "", mDirectory, ESM::GameProfile::Oblivion};
        std::unique_ptr<MWLua::LuaManager> mLuaManager;
        std::unique_ptr<MWSound::SoundManager> mSoundManager;

        explicit NativeWorldFixture(bool scriptsFirst = false, bool extraNative = false, bool activationScript = false,
            const std::string& scriptSource = {}, const std::filesystem::path& luaLibs = {},
            const std::string& extraRecords = {}, std::uint32_t extraRecordCount = 0);
        ~NativeWorldFixture();
    };
}

#endif
