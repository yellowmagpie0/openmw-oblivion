#include "worldimp.hpp"
#include "savedreference.hpp"
#include "../mwphysics/oblivionragdoll.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>

#include <osg/ComputeBoundsVisitor>
#include <osg/Group>
#include <osg/Timer>

#include <MyGUI_TextIterator.h>

#include <LinearMath/btAabbUtil2.h>

#include <components/debug/debuglog.hpp>

#include <components/esm3/cellref.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadregn.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadstat.hpp>
#include <components/esm4/loadsoun.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorclock.hpp>
#include <components/esm4/runtimereferences.hpp>

#include <components/misc/constants.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/mathutil.hpp>
#include <components/misc/pathhelpers.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/rng.hpp>

#include <components/files/collections.hpp>
#include <components/files/hash.hpp>
#include <components/files/openfile.hpp>

#include <components/resource/bulletshape.hpp>
#include <components/resource/resourcesystem.hpp>

#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/workqueue.hpp>

#include <components/detournavigator/agentbounds.hpp>
#include <components/detournavigator/debug.hpp>
#include <components/detournavigator/navigator.hpp>
#include <components/detournavigator/settings.hpp>
#include <components/detournavigator/stats.hpp>
#include <components/detournavigator/updateguard.hpp>

#include <components/files/conversion.hpp>
#include <components/loadinglistener/loadinglistener.hpp>

#include <components/esm/attr.hpp>
#include <components/esm/util.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/aiavoiddoor.hpp" //Used to tell actors to avoid doors
#include "../mwmechanics/combat.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "oblivionactorstats.hpp"
#include "../mwmechanics/levelledlist.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include <components/esm4/observation.hpp>
#include "../mwmechanics/spellcasting.hpp"
#include "../mwmechanics/spellutil.hpp"
#include "../mwmechanics/summoning.hpp"

#include "../mwrender/animation.hpp"
#include "../mwrender/camera.hpp"
#include "../mwrender/npcanimation.hpp"
#include "../mwrender/postprocessor.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwscript/globalscripts.hpp"

#include "../mwclass/door.hpp"

#include "../mwphysics/actor.hpp"
#include "../mwphysics/collisiontype.hpp"
#include "../mwphysics/object.hpp"
#include "../mwphysics/physicssystem.hpp"

#include "../mwsound/constants.hpp"

#include "actionteleport.hpp"
#include "cellstore.hpp"
#include "containerstore.hpp"
#include "datetimemanager.hpp"
#include "inventorystore.hpp"
#include "manualref.hpp"
#include "oblivionprofileservices.hpp"
#include "oblivioninventoryidentity.hpp"
#include "oblivionscriptmanager.hpp"
#include "player.hpp"
#include "projectilemanager.hpp"
#include "weather.hpp"

#include "contentloader.hpp"
#include "esmloader.hpp"

namespace MWWorld
{
    struct World::PreparedOblivionServices
    {
        std::optional<MWMechanics::OblivionCombatService> mCombat;
        std::optional<OblivionScriptManager::PreparedRestore> mScripts;
        std::optional<MWMechanics::OblivionAiService::PreparedRestore> mAi;
        std::optional<ESMStore::PreparedDynamicRecords> mDefinitions;
        std::unique_ptr<InventoryStore> mPlayerInventory;
        std::map<ESM::FormKey, std::unique_ptr<InventoryStore>> mActorInventories;
    };

    class World::PreparedOblivionSaveStateImpl final : public MWBase::World::PreparedOblivionSaveState
    {
        World* mWorld;
        std::weak_ptr<const char> mIdentity;
        std::uint64_t mClearGeneration;
        std::unique_ptr<ESM4::RuntimeState> mState;
        std::unique_ptr<PreparedOblivionServices> mServices;

    public:
        PreparedOblivionSaveStateImpl(World& world, const ESM4::RuntimeState& state,
            std::unique_ptr<ESMStore> definitions)
            : mWorld(&world)
            , mIdentity(world.mOblivionRestoreIdentity)
            , mClearGeneration(world.mOblivionClearGeneration + 1)
            , mState(std::make_unique<ESM4::RuntimeState>(state))
            , mServices(std::make_unique<PreparedOblivionServices>())
        {
            if (definitions)
                mServices->mDefinitions.emplace(world.mStore.prepareDynamicRecords(std::move(definitions)));
            // Even a caller without shared records must not borrow outgoing overrides.
            const ESMStore emptyDefinitions;
            const auto& incoming = mServices->mDefinitions
                ? mServices->mDefinitions->definitions() : emptyDefinitions;
            world.validateOblivionSaveStateImpl(*mState, mServices.get(), &incoming);
        }

        bool install() noexcept override
        {
            if (!mState || mIdentity.expired())
                return false;
            auto& world = *mWorld;
            if (world.mOblivionClearGeneration != mClearGeneration
                || world.mPendingOblivionRuntimeState || world.mOblivionRuntimeState
                || world.mGameProfile != ESM::GameProfile::Oblivion)
                return false;
            if (mServices->mDefinitions)
            {
                const auto* player = mServices->mDefinitions->commit();
                if (!player)
                    return false;
                if (world.mPlayer)
                    world.mPlayer->set(player);
                world.mSharedDefinitionsPrepared = true;
                mServices->mDefinitions.reset();
            }
            world.mPendingOblivionRuntimeState = std::move(mState);
            world.mPendingOblivionServices = std::move(mServices);
            return true;
        }
    };

    namespace
    {
        using LegacyDeathMarker = decltype(ESM4::RuntimeReferenceState::mCustomState)::iterator;

        std::optional<LegacyDeathMarker> findLegacyDeathMarker(ESM4::RuntimeReferenceState* reference)
        {
            if (reference)
            {
                const auto found = reference->mCustomState.find("obscript.dead");
                if (found != reference->mCustomState.end())
                    return found;
            }
            return std::nullopt;
        }

        GlobalVariableName canonicalOblivionGlobal(GlobalVariableName name)
        {
            if (Misc::StringUtils::ciEqual(name.getValue(), "GameDaysPassed"))
                return Globals::sDaysPassed;
            if (Misc::StringUtils::ciEqual(name.getValue(), "GameHour"))
                return Globals::sGameHour;
            if (Misc::StringUtils::ciEqual(name.getValue(), "GameDay"))
                return Globals::sDay;
            if (Misc::StringUtils::ciEqual(name.getValue(), "GameMonth"))
                return Globals::sMonth;
            if (Misc::StringUtils::ciEqual(name.getValue(), "GameYear"))
                return Globals::sYear;
            if (Misc::StringUtils::ciEqual(name.getValue(), "TimeScale"))
                return Globals::sTimeScale;
            return name;
        }

        void synchronizeOblivionCalendarGlobals(Globals& globals)
        {
            static const std::array<std::pair<GlobalVariableName, GlobalVariableName>, 5> aliases{{
                { Globals::sDaysPassed, GlobalVariableName(std::string_view("gamedayspassed")) },
                { Globals::sGameHour, GlobalVariableName(std::string_view("gamehour")) },
                { Globals::sDay, GlobalVariableName(std::string_view("gameday")) },
                { Globals::sMonth, GlobalVariableName(std::string_view("gamemonth")) },
                { Globals::sYear, GlobalVariableName(std::string_view("gameyear")) },
            }};
            for (const auto& [canonical, native] : aliases)
            {
                if (globals.getType(native) != ' ')
                    globals[native] = globals[canonical];
            }
        }

        std::vector<std::pair<GlobalVariableName, ESM::Variant>> generateDefaultGlobals()
        {
            return {
                // vanilla Morrowind does not define dayspassed.
                { Globals::sDaysPassed, ESM::Variant(1) }, // but the addons start counting at 1 :(
                { Globals::sWerewolfClawMult, ESM::Variant(25.f) },
                { Globals::sPCKnownWerewolf, ESM::Variant(0) },
                // following should exist in all versions of MW, but not necessarily in TCs
                { Globals::sGameHour, ESM::Variant(0) },
                { Globals::sTimeScale, ESM::Variant(30.f) },
                { Globals::sDay, ESM::Variant(1) },
                { Globals::sYear, ESM::Variant(1) },
                { Globals::sPCRace, ESM::Variant(0) },
                { Globals::sPCHasCrimeGold, ESM::Variant(0) },
                { Globals::sCrimeGoldDiscount, ESM::Variant(0) },
                { Globals::sCrimeGoldTurnIn, ESM::Variant(0) },
                { Globals::sPCHasTurnIn, ESM::Variant(0) },
            };
        }

        void migrateLegacyActorEquipment(std::vector<ESM4::RuntimeInventoryItem>& inventory,
            const ESMStore& store, const ESM::FormKeyResolver& resolver)
        {
            // Versions 1-3 stored quantities only. Apply the same deliberate
            // default-equipment migration in lazy loading and prepared restore.
            const auto original = inventory;
            for (const auto& item : original)
                if (const auto id = resolver.toFormId(item.mBase))
                    if (auto definition = OblivionProfileServices::itemDefinition(store, ESM::RefId(*id)))
                    {
                        definition->mBase = item.mBase;
                        ESM4::equipInventoryItem(inventory, *definition);
                    }
        }

        std::uint32_t getNativeEquippedSlots(const InventoryStore& inventory, const ConstPtr& item,
            const ESM4::InventoryItemDefinition& definition)
        {
            if (!definition.mChooseOneSlot)
                return definition.mSlots;
            const auto isItemInSlot = [&](int slot) {
                const ConstContainerStoreIterator equipped = inventory.getSlot(slot);
                return equipped != inventory.cend() && *equipped == item;
            };
            return isItemInSlot(InventoryStore::Slot_LeftRing)
                ? static_cast<std::uint32_t>(ESM4::Armor::TES4_LeftRing)
                : static_cast<std::uint32_t>(ESM4::Armor::TES4_RightRing);
        }
    }

    struct GameContentLoader : public ContentLoader
    {
        void addLoader(std::string&& extension, ContentLoader& loader)
        {
            mLoaders.emplace(std::move(extension), &loader);
        }

        void load(const std::filesystem::path& filepath, int& index, Loading::Listener* listener) override
        {
            const auto it
                = mLoaders.find(Misc::StringUtils::lowerCase(Files::pathToUnicodeString(filepath.extension())));
            if (it != mLoaders.end())
            {
                const auto filename = filepath.filename();
                Log(Debug::Info) << "Loading content file " << filename;
                if (listener != nullptr)
                    listener->setLabel(MyGUI::TextIterator::toTagsString(Files::pathToUnicodeString(filename)));
                it->second->load(filepath, index, listener);
            }
            else
            {
                std::string msg("Cannot load file: ");
                msg += Files::pathToUnicodeString(filepath);
                throw std::runtime_error(msg.c_str());
            }
        }

    private:
        std::map<std::string, ContentLoader*> mLoaders;
    };

    struct OMWScriptsLoader : public ContentLoader
    {
        ESMStore& mStore;
        OMWScriptsLoader(ESMStore& store)
            : mStore(store)
        {
        }
        void load(const std::filesystem::path& filepath, int& /*index*/, Loading::Listener* /*listener*/) override
        {
            mStore.addOMWScripts(filepath);
        }
    };

    void World::adjustSky()
    {
        if (mSky && (isCellExterior() || isCellQuasiExterior()))
        {
            // Select the cell's native climate before the sky lazily creates its sun, moons, and
            // night model. Waiting for the next weather tick would briefly instantiate fallback assets.
            if (mGameProfile == ESM::GameProfile::Oblivion && mWeatherManager && getPlayerPtr().isInCell())
                mWeatherManager->playerTeleported(getPlayerPtr().getCell()->getCell()->getRegion(), true);
            mRendering->setSkyEnabled(true);
        }
        else
            mRendering->setSkyEnabled(false);
    }

    World::World(Resource::ResourceSystem* resourceSystem, int activationDistanceOverride, const std::string& startCell,
        const std::filesystem::path& userDataPath, ESM::GameProfile requestedGameProfile)
        : mResourceSystem(resourceSystem)
        , mLocalScripts(mStore)
        , mWorldModel(mStore, mReaders)
        , mRequestedGameProfile(requestedGameProfile)
        , mTimeManager(std::make_unique<DateTimeManager>())
        , mSky(true)
        , mGodMode(false)
        , mScriptsEnabled(true)
        , mDiscardMovements(true)
        , mUserDataPath(userDataPath)
        , mActivationDistanceOverride(activationDistanceOverride)
        , mStartCell(startCell)
        , mSwimHeightScale(0.f)
        , mDistanceToFocusObject(-1.f)
        , mTeleportEnabled(true)
        , mLevitationEnabled(true)
        , mGoToJail(false)
        , mDaysInPrison(0)
        , mPlayerTraveling(false)
        , mPlayerInJail(false)
        , mSpellPreloadTimer(0.f)
    {
    }

    void World::loadData(const Files::Collections& fileCollections, const std::vector<std::string>& contentFiles,
        const std::vector<std::string>& groundcoverFiles, ToUTF8::Utf8Encoder* encoder, Loading::Listener* listener)
    {
        mContentFiles = contentFiles;
        mESMVersions.resize(mContentFiles.size(), -1);

        loadContentFiles(fileCollections, contentFiles, encoder, listener);
        mOblivionContentIdentities.clear();
        if (mGameProfile == ESM::GameProfile::Oblivion)
        {
            for (const std::string& contentFile : contentFiles)
            {
                if (Misc::StringUtils::ciEqual(contentFile, "builtin.omwscripts"))
                    continue;
                const std::filesystem::path path = fileCollections.getPath(contentFile);
                const std::string pathName = Files::pathToUnicodeString(path);
                const auto stream = Files::openBinaryInputFileStream(path);
                mOblivionContentIdentities.emplace_back(
                    ESM::normalizePluginName(contentFile), "sha256:" + Files::getSha256(pathName, *stream));
            }
            std::sort(mOblivionContentIdentities.begin(), mOblivionContentIdentities.end());
        }
        loadGroundcoverFiles(fileCollections, groundcoverFiles, encoder, listener);
        MWBase::Environment::get().getLuaManager()->contentFilesLoaded();

        if (mGameProfile == ESM::GameProfile::Oblivion)
            installOblivionProfileServices();

        fillGlobalVariables();

        mStore.setUp();
        mStore.validateRecords(mReaders);
        mStore.movePlayerRecord();

        if (mGameProfile == ESM::GameProfile::Oblivion)
        {
            // The native configuration producers run even when VERSION blocks
            // file reads. Start from owned compiled settings; the OS/path
            // adapter supplies any separately resolved overrides later.
            mOblivionPhysicalBlendConfiguration
                = std::make_unique<ESM4::PhysicalBlendProfilesConfiguration>(
                    ESM4::loadPhysicalBlendProfiles({}, 0, {}));
            mOblivionAi = std::make_unique<MWMechanics::OblivionAiService>(*this);
            mOblivionCombat = std::make_unique<MWMechanics::OblivionCombatService>();
            mOblivionObservation = ESM4::ObservationStream::fromEnvironment();
            mOblivionScriptManager = std::make_unique<OblivionScriptManager>(*this, mStore, mContentFiles);
        }

        mSwimHeightScale = mStore.get<ESM::GameSetting>().find("fSwimHeightScale")->mValue.getFloat();
    }

    const ESM4::PhysicalBlendProfilesConfiguration& World::getOblivionPhysicalBlendConfiguration() const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionPhysicalBlendConfiguration)
            throw std::logic_error("native physical configuration requires a loaded Oblivion world");
        return *mOblivionPhysicalBlendConfiguration;
    }

    void World::loadOblivionPhysicalBlendConfiguration(
        std::uint32_t version, const ESM4::PhysicalBlendProfilesValues& processedValues)
    {
        auto candidate = std::make_unique<ESM4::PhysicalBlendProfilesConfiguration>(
            ESM4::loadPhysicalBlendProfiles(getOblivionPhysicalBlendConfiguration(), version, processedValues));
        mOblivionPhysicalBlendConfiguration.swap(candidate);
    }

    void World::beginOblivionActorPhysicalPose(MWRender::Animation& animation, const Ptr& actor,
        const NifBullet::ActorRagdollDefinition& authored, const Nif::NiTransform& placement,
        std::span<const std::uint32_t> resolvedPackedFilters, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter)
    {
        const auto& configuration = getOblivionPhysicalBlendConfiguration();
        if (!mPhysics)
            throw std::logic_error("native physical admission requires initialized World physics");
        beginNativeActorPhysicalPose(*mPhysics, animation, actor, authored, placement, resolvedPackedFilters,
            configuration.mPostLink, collisionGroup, collisionMask, internalFilter);
    }

    std::vector<NifBullet::RagdollNativeKnockdownBlendDisposition> World::prepareOblivionActorKnockdownControllers(
        const Ptr& actor, std::span<const OblivionPhysicalDownRequest> requests)
    {
        const auto& configuration = getOblivionPhysicalBlendConfiguration();
        if (!mPhysics)
            throw std::logic_error("native Down setup requires initialized World physics");
        std::vector<NifBullet::RagdollNativeKnockdownControllerSetupRequest> prepared;
        prepared.reserve(requests.size());
        for (const auto& request : requests)
            prepared.push_back({request.mNodeRecord, request.mWorldVector,
                ESM4::physicalBlendDurationForFilter(configuration.mDurations, request.mResolvedPackedFilter, false)});
        const auto& settings = configuration.mProfiles.mDefault;
        return mPhysics->prepareActorRagdollKnockdownControllerSetup(
            actor, prepared, {settings.mPassOutForce, settings.mPassOutTime});
    }

    std::vector<NifBullet::RagdollNativeHitBlendDisposition> World::prepareOblivionActorHitBlendControllers(
        const Ptr& actor, std::span<const OblivionPhysicalHitBlendRequest> requests, bool useQuadHit)
    {
        const auto& configuration = getOblivionPhysicalBlendConfiguration();
        if (!mPhysics)
            throw std::logic_error("native HIT blend setup requires initialized World physics");
        const auto& gains = useQuadHit ? configuration.mQuadHit : configuration.mHit.mGains;
        std::vector<NifBullet::RagdollNativeHitBlendSetupRequest> prepared;
        prepared.reserve(requests.size());
        for (const auto& request : requests)
            prepared.push_back({request.mNodeRecord, gains[(request.mResolvedPackedFilter >> 8) & 31u]});
        return mPhysics->prepareActorRagdollHitBlends(actor, prepared);
    }

    std::vector<NifBullet::RagdollNativeHitBlendDisposition> World::prepareOblivionActorHitControllers(
        const Ptr& actor, std::span<const OblivionPhysicalHitBlendRequest> blends, bool useQuadHit,
        std::span<const OblivionPhysicalHitVelocityRequest> velocities)
    {
        const auto& configuration = getOblivionPhysicalBlendConfiguration();
        if (!mPhysics)
            throw std::logic_error("native HIT setup requires initialized World physics");
        const auto& gains = useQuadHit ? configuration.mQuadHit : configuration.mHit.mGains;
        std::vector<NifBullet::RagdollNativeHitBlendSetupRequest> preparedBlends;
        preparedBlends.reserve(blends.size());
        for (const auto& request : blends)
            preparedBlends.push_back({request.mNodeRecord, gains[(request.mResolvedPackedFilter >> 8) & 31u]});
        std::vector<NifBullet::RagdollNativeHitVelocitySetupRequest> preparedVelocities;
        preparedVelocities.reserve(velocities.size());
        for (const auto& request : velocities)
            preparedVelocities.push_back({request.mNodeRecord, request.mNativeSourceVector, request.mResolvedMassMultiplier});
        return mPhysics->prepareActorRagdollHitControllerSetup(actor, preparedBlends, preparedVelocities);
    }

    MWPhysics::PhysicsSystem& World::initializePhysics(osg::ref_ptr<osg::Group> rootNode)
    {
        if (mPhysics)
            throw std::logic_error("World physics is already initialized");
        auto physics = std::make_unique<MWPhysics::PhysicsSystem>(mResourceSystem, rootNode);
        if (mGameProfile == ESM::GameProfile::Oblivion && mOblivionRuntimeState
            && mOblivionRuntimeState->mNativePhysicalBlendTimeCache)
            physics->restoreNativeBlendTimeCache(*mOblivionRuntimeState->mNativePhysicalBlendTimeCache);
        mPhysics = std::move(physics);
        return *mPhysics;
    }

    void World::init(Debug::Level maxRecastLogLevel, osgViewer::Viewer* viewer, osg::ref_ptr<osg::Group> rootNode,
        SceneUtil::WorkQueue* workQueue, SceneUtil::UnrefQueue& unrefQueue)
    {
        initializePhysics(rootNode);

        if (Settings::navigator().mEnable)
        {
            auto navigatorSettings = DetourNavigator::makeSettingsFromSettingsManager(maxRecastLogLevel);
            navigatorSettings.mRecast.mSwimHeightScale = mSwimHeightScale;
            mNavigator = DetourNavigator::makeNavigator(navigatorSettings, mUserDataPath);
        }
        else
        {
            mNavigator = DetourNavigator::makeNavigatorStub();
        }

        mRendering = std::make_unique<MWRender::RenderingManager>(
            viewer, rootNode, mResourceSystem, workQueue, *mNavigator, mGroundcoverStore, unrefQueue);
        mProjectileManager = std::make_unique<ProjectileManager>(
            mRendering->getLightRoot()->asGroup(), mResourceSystem, mRendering.get(), mPhysics.get());
        mRendering->preloadCommonAssets();

        mWeatherManager = std::make_unique<MWWorld::WeatherManager>(*mRendering, mStore);

        mWorldScene = std::make_unique<Scene>(*this, *mRendering.get(), mPhysics.get(), *mNavigator);
    }

    void World::fillGlobalVariables()
    {
        mGlobalVariables.fill(mStore);
        mTimeManager->setup(mGlobalVariables);
        if (mGameProfile == ESM::GameProfile::Oblivion)
            synchronizeOblivionCalendarGlobals(mGlobalVariables);
    }

    void World::startNewGame(bool bypass)
    {
        if (mOblivionCombat)
            mOblivionCombat->clear();
        if (mOblivionScriptManager)
        {
            if (bypass)
                mOblivionScriptManager->clear();
            else
                mOblivionScriptManager->startNewGame();
        }
        if (mOblivionAi)
            mOblivionAi->clear();
        mGoToJail = false;
        mLevitationEnabled = true;
        mTeleportEnabled = true;

        mGodMode = false;
        mScriptsEnabled = true;
        mSky = true;

        // Rebuild player
        setupPlayer();

        renderPlayer();
        mRendering->getCamera()->reset();

        // we don't want old weather to persist on a new game
        // Note that if reset later, the initial ChangeWeather that the chargen script calls will be lost.
        mWeatherManager.reset();
        mWeatherManager = std::make_unique<MWWorld::WeatherManager>(*mRendering.get(), mStore);

        if (!bypass)
        {
            // set new game mark
            mGlobalVariables[Globals::sCharGenState].setInteger(1);
        }
        else
            mGlobalVariables[Globals::sCharGenState].setInteger(-1);

        MWBase::Environment::get().getLuaManager()->newGameStarted();

        if (bypass && !mStartCell.empty())
        {
            std::string_view startCell = mStartCell;
            std::optional<std::uint32_t> startReference;
            osg::Vec2f startReferenceOffset(-40.f, 0.f);
            float startReferenceYaw = osg::PIf * 0.5f;
            if (mGameProfile == ESM::GameProfile::Oblivion)
            {
                constexpr std::string_view separator = "::ref=";
                if (const std::size_t offset = startCell.find(separator); offset != std::string_view::npos)
                {
                    std::string_view referenceText = startCell.substr(offset + separator.size());
                    startCell = startCell.substr(0, offset);
                    const auto selectSide = [&](std::string_view suffix, const osg::Vec2f& sideOffset, float yaw) {
                        if (!referenceText.ends_with(suffix))
                            return false;
                        referenceText.remove_suffix(suffix.size());
                        startReferenceOffset = sideOffset;
                        startReferenceYaw = yaw;
                        return true;
                    };
                    if (!selectSide("::side=east", osg::Vec2f(40.f, 0.f), -osg::PIf * 0.5f)
                        && !selectSide("::side=north", osg::Vec2f(0.f, 40.f), osg::PIf))
                    {
                        selectSide("::side=south", osg::Vec2f(0.f, -40.f), 0.f);
                    }
                    std::uint32_t value = 0;
                    const char* begin = referenceText.data();
                    if (referenceText.starts_with("0x"))
                        begin += 2;
                    const std::from_chars_result parsed
                        = std::from_chars(begin, referenceText.data() + referenceText.size(), value, 16);
                    if (parsed.ec != std::errc{} || parsed.ptr != referenceText.data() + referenceText.size())
                        throw std::runtime_error("Invalid TES4 start reference: " + std::string(referenceText));
                    startReference = value;
                }
            }
            ESM::Position pos;
            ESM::RefId cellId = findExteriorPosition(startCell, pos);
            if (!cellId.empty())
            {
                if (startReference)
                {
                    const CellStore* cell = mWorldModel.findCell(cellId);
                    std::optional<ESM::Position> target;
                    if (cell)
                        cell->forEachConst(
                            [&](const ConstPtr& ptr) {
                                // TES4 references in a loaded content file carry the runtime content-file
                                // index in RefNum::mIndex (for example 0x200000 + the on-disk FormID),
                                // while the deterministic start syntax intentionally uses the raw FormID.
                                // Compare the load-order-independent key as well so exterior ACHR/ACRE
                                // references remain addressable without changing the public start syntax.
                                if (ptr.getCellRef().getRefNum().mIndex == *startReference
                                    || (ptr.getCellRef().getFormKey().isContent()
                                        && ptr.getCellRef().getFormKey().localId() == *startReference))
                                {
                                    target = ptr.getCellRef().getPosition();
                                    return false;
                                }
                                return true;
                            },
                            true);
                    if (!target)
                    {
                        // Persistent TES4 ACHR/ACRE references are stored under the world's
                        // persistent-cell group rather than the exterior grid cell selected above.
                        // The normal cell-local scan therefore cannot see a stable horse or rider
                        // even though the reference is valid and its position belongs to that grid.
                        // Resolve the native record first, then ask its post-processed owning cell
                        // to materialize the live reference. WorldModel::getPtrByRefId is not a
                        // suitable fallback here: its preloaded-cell fast path indexes base IDs,
                        // while a start reference is a placed-reference ID.
                        const auto findPersistentReference = [&](const auto& store, const ESM::FormKey& referenceKey) {
                            const auto* nativeReference = store.search(referenceKey);
                            if (nativeReference == nullptr)
                                return;
                            const CellStore* owningCell = mWorldModel.findCell(nativeReference->mParent);
                            if (owningCell == nullptr)
                                return;
                            owningCell->forEachConst(
                                [&](const ConstPtr& ptr) {
                                    if (ptr.getCellRef().getFormKey() == referenceKey)
                                    {
                                        target = ptr.getCellRef().getPosition();
                                        return false;
                                    }
                                    return true;
                                },
                                true);
                        };
                        // FormID 0 belongs to the master file in TES4. The runtime
                        // content list also contains builtin.omwscripts, so using
                        // FormKeyResolver::toFormKey on a raw master FormID would
                        // incorrectly choose that synthetic file. Try every loaded
                        // content identity and retain only an actually indexed ref.
                        for (const std::string& contentFile : mContentFiles)
                        {
                            const ESM::FormKey referenceKey = ESM::FormKey::content(contentFile, *startReference);
                            findPersistentReference(mStore.get<ESM4::ActorCharacter>(), referenceKey);
                            if (!target)
                                findPersistentReference(mStore.get<ESM4::ActorCreature>(), referenceKey);
                            if (!target)
                                findPersistentReference(mStore.get<ESM4::Reference>(), referenceKey);
                            if (target)
                                break;
                        }
                    }
                    if (!target)
                        throw std::runtime_error("TES4 start reference is not present in exterior cell: 0x"
                            + std::to_string(*startReference));
                    const osg::Vec3f startLookTarget = target->asVec3();
                    pos = *target;
                    pos.pos[0] += startReferenceOffset.x();
                    pos.pos[1] += startReferenceOffset.y();
                    pos.pos[2] += 16.f;
                    pos.rot[0] = pos.rot[1] = 0.f;
                    pos.rot[2] = startReferenceYaw;
                    Log(Debug::Info) << "M12 exterior start target: cell=" << startCell << " ref=0x" << std::hex
                                     << *startReference << std::dec << " position=" << pos.pos[0] << ','
                                     << pos.pos[1] << ',' << pos.pos[2];
                    // Exact reference-relative starts are used by deterministic movement courses, including starts
                    // below a water surface. Change the cell without grounding first, then apply the exact global
                    // position through the normal movement path so rendering, physics, and the player cell grid all
                    // see the same coordinates.
                    changeToCell(cellId, pos, true);
                    moveObject(getPlayerPtr(), pos.asVec3(), true, false);
                    // The camera is attached during changeToCell, before the exact position restore. Reapply the
                    // reference-facing view explicitly so a persistent exterior start points at its target even
                    // when no input frame has yet supplied a look delta.
                    mRendering->getCamera()->setPitch(-pos.rot[0]);
                    mRendering->getCamera()->setYaw(-pos.rot[2]);
                    mRendering->getCamera()->update(0.001f, true);
                    const osg::Vec3d cameraPosition = mRendering->getCamera()->getPosition();
                    const osg::Vec3f lookDelta = startLookTarget - osg::Vec3f(
                        static_cast<float>(cameraPosition.x()), static_cast<float>(cameraPosition.y()),
                        static_cast<float>(cameraPosition.z()));
                    const float horizontalDistance = std::hypot(lookDelta.x(), lookDelta.y());
                    if (horizontalDistance > 0.001f)
                    {
                        mRendering->getCamera()->setYaw(std::atan2(-lookDelta.x(), lookDelta.y()));
                        mRendering->getCamera()->setPitch(
                            std::atan2(lookDelta.z(), horizontalDistance));
                    }
                }
                else
                {
                    changeToCell(cellId, pos, true);
                    adjustPosition(getPlayerPtr(), false);
                }
            }
            else
            {
                findInteriorPosition(startCell, pos);
                if (startReference)
                {
                    const CellStore* cell = mWorldModel.findInterior(startCell);
                    std::optional<ESM::Position> target;
                    if (cell)
                        cell->forEachConst(
                            [&](const ConstPtr& ptr) {
                                if (ptr.getCellRef().getRefNum().mIndex == *startReference
                                    || (ptr.getCellRef().getFormKey().isContent()
                                        && ptr.getCellRef().getFormKey().localId() == *startReference))
                                {
                                    target = ptr.getCellRef().getPosition();
                                    return false;
                                }
                                return true;
                            },
                            true);
                    if (!target)
                        throw std::runtime_error("TES4 start reference is not present in cell: 0x"
                            + std::to_string(*startReference));
                    pos = *target;
                    // Keep deterministic interaction probes close enough that an unrelated
                    // piece of cell architecture cannot occlude the selected reference.
                    // Forty units is outside the player capsule for the small prison props
                    // used by M5 while still exercising the normal activation ray.
                    pos.pos[0] += startReferenceOffset.x();
                    pos.pos[1] += startReferenceOffset.y();
                    pos.pos[2] += 16.f;
                    pos.rot[0] = pos.rot[1] = 0.f;
                    pos.rot[2] = startReferenceYaw;
                    Log(Debug::Info) << "M5 start target: cell=" << startCell << " ref=0x" << std::hex
                                     << *startReference << std::dec << " position=" << pos.pos[0] << ','
                                     << pos.pos[1] << ',' << pos.pos[2];
                }
                changeToInteriorCell(startCell, pos, true);
                if (startReference)
                {
                    const Ptr player = getPlayerPtr();
                    const ESM::Position& actual = player.getRefData().getPosition();
                    Log(Debug::Info) << "M5 start target applied: player=" << actual.pos[0] << ',' << actual.pos[1]
                                     << ',' << actual.pos[2];
                }
            }
        }
        else
        {
            for (int i = 0; i < 5; ++i)
                MWBase::Environment::get().getScriptManager()->getGlobalScripts().run();
            if (!getPlayerPtr().isInCell())
            {
                ESM::Position pos;
                if (mGameProfile == ESM::GameProfile::Oblivion)
                {
                    constexpr std::string_view nativeStartCell = "ImperialDungeon01";
                    findInteriorPosition(nativeStartCell, pos);
                    changeToInteriorCell(nativeStartCell, pos, true);
                    Log(Debug::Info) << "M10 native new-game start: " << nativeStartCell;
                }
                else
                {
                    const int cellSize = Constants::CellSizeInUnits;
                    pos.pos[0] = cellSize / 2;
                    pos.pos[1] = cellSize / 2;
                    pos.pos[2] = 0;
                    pos.rot[0] = 0;
                    pos.rot[1] = 0;
                    pos.rot[2] = 0;

                    ESM::ExteriorCellLocation exteriorCellPos
                        = ESM::positionToExteriorCellLocation(pos.pos[0], pos.pos[1]);
                    ESM::RefId cellId = ESM::RefId::esm3ExteriorCell(exteriorCellPos.mX, exteriorCellPos.mY);
                    mWorldScene->changeToExteriorCell(cellId, pos, true);
                }
            }
        }

        if (!bypass)
        {
            const std::string_view video = mGameProfile == ESM::GameProfile::Oblivion
                ? std::string_view("OblivionIntro.bik")
                : Fallback::Map::getString("Movies_New_Game");
            if (!video.empty())
            {
                // Make sure that we do not continue to play a Title music after a new game video.
                MWBase::Environment::get().getSoundManager()->stopMusic();
                MWBase::Environment::get().getWindowManager()->playVideo(video, true);
            }
        }

        // enable collision
        if (!mPhysics->toggleCollisionMode())
            mPhysics->toggleCollisionMode();

        MWBase::Environment::get().getWindowManager()->updatePlayer();
        mTimeManager->setup(mGlobalVariables);

        // Initial seed.
        mPrng.seed(mRandomSeed);
    }

    void World::clear()
    {
        // Static graph registrations outlive a game. Prepare their content
        // defaults before teardown so a new game or a save without T4ST cannot
        // inherit disabled nodes from the outgoing native world.
        std::optional<ESM4::PathgridService::PreparedOverlayRestore> preparedPathgrids;
        if (mGameProfile == ESM::GameProfile::Oblivion)
            preparedPathgrids.emplace(mStore.getOblivionPathgridService().prepareOverlayRestore({}));

        mOblivionDynamicReferenceIdentity.reset();
        if (mWeatherManager)
            mWeatherManager->clear();
        if (mRendering)
            mRendering->clear();
        if (mProjectileManager)
            mProjectileManager->clear();
        mLocalScripts.clear();

        if (mWorldScene)
            mWorldScene->clear();
        mWorldModel.clear();

        mStore.clearDynamic();

        if (mPlayer)
        {
            mPlayer->clear();
            mPlayer->set(mStore.get<ESM::NPC>().find(ESM::RefId::stringRefId("Player")));
        }

        mDoorStates.clear();

        mGoToJail = false;
        mTeleportEnabled = true;
        mLevitationEnabled = true;
        mPlayerTraveling = false;
        mPlayerInJail = false;
        mIdsRebuilt = false;
        mOblivionRuntimeState.reset();
        mPendingOblivionRuntimeState.reset();
        mPendingOblivionServices.reset();
        mSharedDefinitionsPrepared = false;
        if (preparedPathgrids)
            preparedPathgrids->commit();
        if (mOblivionCombat)
            mOblivionCombat->clear();
        if (mOblivionAi)
            mOblivionAi->clear();
        mNextOblivionDynamicSerial = 1;
        mLastOblivionScriptSeconds = 0;
        if (mOblivionScriptManager)
            mOblivionScriptManager->clear();

        fillGlobalVariables();
        ++mOblivionClearGeneration;
    }

    size_t World::countSavedGameRecords() const
    {
        return mWorldModel.countSavedGameRecords() + mStore.countSavedGameRecords()
            + mGlobalVariables.countSavedGameRecords() + mProjectileManager->countSavedGameRecords()
            + 1 // player record
            + 1 // weather record
            + 1 // levitation/teleport enabled state
            + 1 // camera
            + 1 // random state.
            + (mGameProfile == ESM::GameProfile::Oblivion ? 1 : 0); // native TES4 runtime state
    }

    size_t World::countSavedGameCells() const
    {
        return mWorldModel.countSavedGameRecords();
    }

    void World::observeOblivionState(std::string_view event, const std::filesystem::path& save) const
    {
        if (mOblivionObservation)
        {
            try
            {
                mOblivionObservation->stateBoundary(event, save, captureOblivionRuntimeState());
            }
            catch (const std::exception& error)
            {
                // Evidence failure fails the course through both the missing
                // acknowledgment and this diagnostic. It must not roll back or
                // interrupt the normal save/load operation being observed.
                Log(Debug::Error) << "M15 state observation failed: " << error.what();
            }
        }
    }

    namespace
    {
        ESM::FormKey nativeActorBase(const Ptr& actor)
        {
            if (!actor.isEmpty() && actor.getType() == ESM::REC_NPC_4)
            {
                const auto* base = actor.get<ESM4::Npc>()->mBase;
                if (base && base->mIsTES4)
                    return base->mFormKey;
            }
            else if (!actor.isEmpty() && actor.getType() == ESM::REC_CREA4)
            {
                const auto* base = actor.get<ESM4::Creature>()->mBase;
                if (base && base->mAttackReach)
                    return base->mFormKey;
            }
            return {};
        }
    }

    std::optional<ESM4::ActorDrawState> World::captureOblivionActorDrawState(const Ptr& actor) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat)
            return std::nullopt;
        const auto base = nativeActorBase(actor);
        if (base.isNull())
            return std::nullopt;
        const auto key = actor.getCellRef().getFormKey();
        const auto* values = mOblivionCombat->findActorValues(key);
        const auto* life = mOblivionCombat->findActorLife(key);
        if (!values || !life)
            return std::nullopt; // Do not create resource authority during a pose read.
        if (values->mOwner != ESM4::ActorValueOwner::NonPlayer || values->mBase != base || life->mBase != base)
            throw std::invalid_argument("native actor draw capture has conflicting ownership");
        const auto* npc = mStore.search<ESM4::Npc>(base);
        const auto* creature = mStore.search<ESM4::Creature>(base);
        if ((npc != nullptr) == (creature != nullptr)
            || (npc && (!npc->mIsTES4 || actor.getType() != ESM::REC_NPC_4))
            || (creature && (!creature->mAttackReach || actor.getType() != ESM::REC_CREA4)))
            throw std::invalid_argument("native actor draw capture has an invalid winning class/base");
        switch (actor.getClass().getCreatureStats(actor).getDrawState())
        {
            case MWMechanics::DrawState::Nothing: return ESM4::ActorDrawState::Nothing;
            case MWMechanics::DrawState::Weapon: return ESM4::ActorDrawState::Weapon;
            case MWMechanics::DrawState::Spell: return ESM4::ActorDrawState::Spell;
        }
        throw std::invalid_argument("native actor draw capture has an invalid view state");
    }

    namespace
    {
        std::optional<ESM4::ActorDrawState> savedActorDrawState(
            const ESMStore& store, const Ptr& actor, const ESM4::RuntimeState& state)
        {
            if (actor.isEmpty())
                return std::nullopt;
            const auto key = actor.getCellRef().getFormKey();
            const auto saved = std::find_if(state.mReferences.begin(),
                state.mReferences.end(), [&](const auto& reference) { return reference.mKey == key; });
            if (saved == state.mReferences.end() || !saved->mActorDrawState)
                return std::nullopt;
            const auto base = nativeActorBase(actor);
            const auto* npc = store.search<ESM4::Npc>(saved->mBase);
            const auto* creature = store.search<ESM4::Creature>(saved->mBase);
            if (base.isNull() || base != saved->mBase || (npc != nullptr) == (creature != nullptr)
                || (npc && (!npc->mIsTES4 || actor.getType() != ESM::REC_NPC_4))
                || (creature && (!creature->mAttackReach || actor.getType() != ESM::REC_CREA4)))
                throw std::invalid_argument("native actor draw restore has an invalid class/base binding");
            return saved->mActorDrawState;
        }
    }

    std::optional<ESM4::ActorDrawState> World::oblivionSavedActorDrawState(const Ptr& actor) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState)
            return std::nullopt;
        return savedActorDrawState(mStore, actor, *mOblivionRuntimeState);
    }

    bool World::restoreOblivionActorDrawState(const Ptr& actor) const
    {
        const auto saved = oblivionSavedActorDrawState(actor);
        if (!saved)
            return false;
        MWMechanics::DrawState draw;
        switch (*saved)
        {
            case ESM4::ActorDrawState::Nothing: draw = MWMechanics::DrawState::Nothing; break;
            case ESM4::ActorDrawState::Weapon: draw = MWMechanics::DrawState::Weapon; break;
            case ESM4::ActorDrawState::Spell: draw = MWMechanics::DrawState::Spell; break;
            default: throw std::invalid_argument("native actor draw restore has an invalid state");
        }
        // Native class constructors call this only after publishing their
        // custom-data cache, so this access cannot recurse into construction.
        actor.getClass().getCreatureStats(actor).setDrawState(draw);
        return true;
    }

    std::unique_ptr<InventoryStore> World::prepareOblivionSavedActorInventory(const Ptr& actor) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState || actor.isEmpty())
            return nullptr;
        const auto base = nativeActorBase(actor);
        if (base.isNull())
            return nullptr;
        const auto key = actor.getCellRef().getFormKey();
        const auto saved = std::find_if(mOblivionRuntimeState->mReferences.begin(),
            mOblivionRuntimeState->mReferences.end(), [&](const auto& reference) { return reference.mKey == key; });
        if (saved == mOblivionRuntimeState->mReferences.end())
            return nullptr;
        const auto* npc = mStore.search<ESM4::Npc>(saved->mBase);
        const auto* creature = mStore.search<ESM4::Creature>(saved->mBase);
        if (base != saved->mBase || (npc != nullptr) == (creature != nullptr)
            || (npc && (!npc->mIsTES4 || actor.getType() != ESM::REC_NPC_4))
            || (creature && (!creature->mAttackReach || actor.getType() != ESM::REC_CREA4)))
            throw std::invalid_argument("native actor inventory restore has an invalid class/base binding");
        // A non-null empty store is an authoritative saved empty inventory.
        // Validate and stage before the caller publishes its new class cache.
        // This reads existing save data without creating resource/life authority
        // or invoking live add/equip observers during lazy reconstruction.
        const ESM::FormKeyResolver resolver(mContentFiles);
        auto inventory = saved->mInventory;
        if (mOblivionRuntimeState->mVersion < 4)
            migrateLegacyActorEquipment(inventory, mStore, resolver);
        return OblivionProfileServices::stageActorInventory(
            OblivionProfileServices::prepareActorInventory(mStore, resolver, inventory));
    }

    std::vector<ESM4::RuntimeInventoryItem> World::captureOblivionActorInventory(const Ptr& owner) const
    {
        std::vector<ESM4::RuntimeInventoryItem> result;
        const ESM::FormKey ownerKey = owner.isEmpty() ? ESM::FormKey{} : owner.getCellRef().getFormKey();
        try
        {
            if (owner.isEmpty())
                return result;
            if (mGameProfile != ESM::GameProfile::Oblivion)
                return result;
            const bool player = mPlayer && owner == mPlayer->getPlayer();
            const unsigned ownerType = owner.getClass().getType();
            if (!player && ownerType != ESM::REC_NPC_4 && ownerType != ESM::REC_CREA4)
                return result;
            // Hotkeys have no shared-store field. Join the existing Player
            // assignment once per base, just as full world capture does.
            std::map<ESM::FormKey, std::int8_t> previousHotkeys;
            if (player && mOblivionRuntimeState)
                for (const auto& item : mOblivionRuntimeState->mPlayer.mInventory)
                    if (item.mHotkey >= 0)
                        previousHotkeys.emplace(item.mBase, item.mHotkey);
            const ESM::FormKeyResolver inventoryResolver(mContentFiles);
            InventoryStore& inventory = owner.getClass().getInventoryStore(owner);
            for (ContainerStoreIterator iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
            {
                const Ptr itemPtr = *iterator;
                if (itemPtr.isEmpty())
                    continue;
                const ESM::RefId nativeId
                    = OblivionProfileServices::nativeItemId(mStore, itemPtr.getCellRef().getRefId());
                const ESM::FormId* formId = nativeId.getIf<ESM::FormId>();
                const auto generatedKey = OblivionInventory::sharedKey(nativeId);
                if ((!formId && !generatedKey) || itemPtr.getCellRef().getCount() <= 0)
                    continue;
                ESM4::RuntimeInventoryItem item;
                item.mBase = generatedKey ? *generatedKey : inventoryResolver.toFormKey(*formId);
                item.mCount = itemPtr.getCellRef().getCount();
                if (const auto definition = OblivionProfileServices::itemDefinition(mStore, nativeId))
                {
                    item.mCondition = definition->mMaxCondition < 0 ? -1
                        : itemPtr.getCellRef().getItemCondition(static_cast<float>(definition->mMaxCondition));
                    item.mCharge = definition->mMaxCharge < 0.f ? -1.f
                        : itemPtr.getCellRef().getEnchantmentCharge() < 0.f ? definition->mMaxCharge
                                                                           : itemPtr.getCellRef().getEnchantmentCharge();
                    try
                    {
                        item.mRemainingUsageTime = definition->mMaxUsageTime < 0.f ? -1.f
                            : itemPtr.getClass().getRemainingUsageTime(itemPtr);
                    }
                    catch (const std::exception& error)
                    {
                        throw std::runtime_error("remaining usage time for item " + item.mBase.serialize()
                            + ": " + std::string(error.what()));
                    }
                    if (inventory.isEquipped(itemPtr))
                        item.mEquippedSlots = getNativeEquippedSlots(inventory, itemPtr, *definition);
                }
                if (generatedKey)
                {
                    const auto& itemClass = itemPtr.getClass();
                    ContainerStore::getType(itemPtr);
                    item.mCondition = itemClass.hasItemHealth(itemPtr)
                        ? itemPtr.getCellRef().getItemCondition(static_cast<float>(itemClass.getItemMaxHealth(itemPtr)))
                        : -1.f;
                    item.mCharge = itemPtr.getCellRef().getEnchantmentCharge();
                    item.mRemainingUsageTime = itemClass.getRemainingUsageTime(itemPtr);
                    if (inventory.isEquipped(itemPtr))
                        for (int slot = 0; slot != InventoryStore::Slots; ++slot)
                        {
                            const auto equipped = inventory.getSlot(slot);
                            if (equipped != inventory.end() && *equipped == itemPtr)
                            {
                                item.mEquippedSlots = OblivionInventory::slotMask(
                                    slot, itemPtr.getType() == ESM::REC_LIGH);
                                break;
                            }
                        }
                }
                const ESM::RefId itemOwner = itemPtr.getCellRef().getOwner();
                if (const ESM::FormId* ownerId = itemOwner.getIf<ESM::FormId>())
                    item.mOwner = inventoryResolver.toFormKey(*ownerId);
                item.mOwnershipRank = itemPtr.getCellRef().getNativeOwnershipRank();
                item.mOwnershipGlobal = OblivionProfileServices::captureOwnershipGlobal(
                    mStore, inventoryResolver, itemPtr.getCellRef());
                if (const auto previous = previousHotkeys.find(item.mBase); previous != previousHotkeys.end())
                {
                    item.mHotkey = previous->second;
                    previousHotkeys.erase(previous);
                }
                ESM4::addInventoryItem(result, std::move(item));
            }
            return result;
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("TES4 runtime-state actor inventory " + ownerKey.serialize()
                + " capture failed: " + std::string(error.what()));
        }
    }

    namespace
    {
        struct NativePhysicalBindings
        {
            std::vector<std::string> mModels;
            std::vector<MWPhysics::NativeRagdollSnapshotBinding> mBindings;
            NativePhysicalBindings() = default;
            NativePhysicalBindings(const NativePhysicalBindings&) = delete;
            NativePhysicalBindings& operator=(const NativePhysicalBindings&) = delete;
            NativePhysicalBindings(NativePhysicalBindings&&) noexcept = default;
            NativePhysicalBindings& operator=(NativePhysicalBindings&&) noexcept = default;
        };

        NativePhysicalBindings resolveNativePhysicalBindings(MWPhysics::PhysicsSystem& physics,
            const MWMechanics::OblivionCombatService& service, const ESM4::RuntimeState& state,
            const Ptr& player, const VFS::Manager* vfs)
        {
            const auto owners = physics.actorRagdollOwners();
            NativePhysicalBindings result;
            auto& models = result.mModels;
            auto& bindings = result.mBindings;
            models.reserve(owners.size());
            bindings.reserve(owners.size());
            for (const auto& actor : owners)
            {
                const auto key = actor == player ? ESM::FormKey::dynamic("player", 1)
                                                 : actor.getCellRef().getFormKey();
                const auto cached = state.mNativeActorRagdolls.find(key);
                const auto* values = service.findActorValues(key);
                const auto* life = service.findActorLife(key);
                const auto base = actor == player && values ? values->mBase : nativeActorBase(actor);
                if (cached == state.mNativeActorRagdolls.end() || !values || !life || base.isNull()
                    || values->mBase != base || life->mBase != base || cached->second.mBase != base)
                    throw std::runtime_error("native physical state has an unbound actor/base/lifecycle owner");
                // Resolve the current class model, rather than laundering a stale
                // cached model label through a physical adapter.
                auto model = actor.getClass().getCorrectedModel(actor);
                if (actor == player && actor.getType() == ESM::REC_NPC_
                    && !actor.get<ESM::NPC>()->mBase->mModel.empty())
                    model = Misc::ResourceHelpers::correctActorModelPath(
                        Misc::ResourceHelpers::correctMeshPath(actor.get<ESM::NPC>()->mBase->mModel.getNormalized()),
                        vfs);
                if (model.empty() || model.value() != cached->second.mModel)
                    throw std::runtime_error("native physical state model does not match the current actor");
                models.emplace_back(model.value());
                bindings.push_back({actor, key, base, models.back()});
            }
            return result;
        }
    }

    void World::captureOblivionPhysicalState(ESM4::RuntimeState& state, const Ptr& player) const
    {
        if (!mPhysics)
        {
            // A retained save may be inspected without starting a scene/physics
            // subsystem. Its global clock still belongs to the saved authority.
            if (mOblivionRuntimeState)
                state.mNativePhysicalBlendTimeCache = mOblivionRuntimeState->mNativePhysicalBlendTimeCache;
            return;
        }
        if (!mOblivionCombat)
            throw std::logic_error("native physical save requires combat authority");
        const auto bound = resolveNativePhysicalBindings(
            *mPhysics, *mOblivionCombat, state, player, mResourceSystem->getVFS());
        const auto physical = mPhysics->captureActorRagdollSnapshots(bound.mBindings);
        // Validate every captured asset/body binding before replacing any local
        // cached projection. Unloaded/nonphysical actors remain in the map.
        for (const auto& [key, pose] : physical.mActors)
        {
            const auto& cached = state.mNativeActorRagdolls.at(key);
            if (pose.mAssetHash != cached.mAssetHash || pose.mBodies.size() != cached.mBodies.size())
                throw std::runtime_error("native physical save asset does not match the bound authority");
            for (std::size_t i = 0; i < pose.mBodies.size(); ++i)
                if (pose.mBodies[i].mRecord != cached.mBodies[i].mRecord
                    || pose.mBodies[i].mNodeRecord != cached.mBodies[i].mNodeRecord)
                    throw std::runtime_error("native physical save body does not match the bound authority");
        }
        for (const auto& [key, pose] : physical.mActors)
            state.mNativeActorRagdolls.at(key) = pose;
        state.mNativePhysicalBlendTimeCache = physical.mTimeCache;
    }

    void World::retainOblivionPhysicalState()
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mPhysics)
            return;
        const auto owners = mPhysics->actorRagdollOwners();
        if (owners.empty())
            return;
        if (!mOblivionCombat)
            throw std::logic_error("native physical retention requires combat authority");
        ESM4::RuntimeState state;
        mOblivionCombat->capture(state);
        const auto previous = state.mNativeActorRagdolls;
        const auto player = getPlayerPtr();
        // Capture validates every current asset/body binding before replacing
        // local projections. No authority or scene ownership changes on failure.
        captureOblivionPhysicalState(state, player);
        MWMechanics::OblivionCombatService::PhysicalPoseUpdates updates;
        for (const auto& owner : owners)
        {
            const auto key = owner == player ? ESM::FormKey::dynamic("player", 1)
                                             : owner.getCellRef().getFormKey();
            updates.emplace(key, std::make_pair(
                std::optional(previous.at(key)), std::optional(state.mNativeActorRagdolls.at(key))));
        }
        // Publish every loaded projection, including numerically equal signed
        // zero lanes, before releasing any bodies. Other retained actors survive.
        if (!mOblivionCombat->syncActorRagdolls(updates))
            throw std::runtime_error("native physical authority changed during scene retention");
    }

    ESM4::RuntimeState World::captureOblivionRuntimeState() const
    {
        ESM4::RuntimeState state;
        state.mNextDynamicSerial = mNextOblivionDynamicSerial;
        for (const auto& [plugin, fingerprint] : mOblivionContentIdentities)
            state.mContent.push_back({ plugin, fingerprint });

        const ESM::EpochTimeStamp epoch = mTimeManager->getEpochTimeStamp();
        state.mClock = { epoch.mYear, epoch.mMonth, epoch.mDay, epoch.mGameHour,
            mTimeManager->getGameTimeScale() };

        const ESM::FormKeyResolver resolver(mContentFiles);
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        // Class stat access still uses the historical mutable Ptr API even for read-only save serialization.
        const MWWorld::Ptr player = const_cast<World*>(this)->getPlayerPtr();
        if (player.isEmpty())
            throw std::runtime_error("TES4 runtime-state capture has no player reference");
        state.mPlayer.mPosition = player.getRefData().getPosition();
        if (player.isInCell())
        {
            const MWWorld::Cell* cell = player.getCell()->getCell();
            if (cell->isExterior() && cell->isEsm4())
            {
                const ESM::ExteriorCellLocation location = ESM::positionToExteriorCellLocation(
                    player.getRefData().getPosition().pos[0], player.getRefData().getPosition().pos[1],
                    cell->getWorldSpace());
                if (const ESM4::Cell* nativeCell = mStore.get<ESM4::Cell>().searchExterior(location))
                    state.mPlayer.mCell = nativeCell->mFormKey;
            }
            if (state.mPlayer.mCell.isNull())
            {
                const ESM::RefId cellId = cell->getId();
                if (const ESM::FormId* formId = cellId.getIf<ESM::FormId>())
                    state.mPlayer.mCell = resolver.toFormKey(*formId);
                else if (cell->isEsm4())
                    // ESM4 exterior cells can be represented by a synthesized
                    // world-model RefId while their native identity remains on
                    // the variant. Runtime state must retain that stable key.
                    state.mPlayer.mCell = cell->getEsm4().mFormKey;
            }
        }
        const MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
        const auto addDynamicStat = [&state](std::string_view name, const MWMechanics::DynamicStat<float>& value) {
            const std::string prefix(name);
            state.mPlayer.mActorValues.emplace(prefix + ".base", value.getBase());
            state.mPlayer.mActorValues.emplace(prefix + ".modifier", value.getModifier());
            state.mPlayer.mActorValues.emplace(prefix + ".current", value.getCurrent());
        };
        addDynamicStat("health", stats.getHealth());
        addDynamicStat("magicka", stats.getMagicka());
        addDynamicStat("fatigue", stats.getFatigue());
        state.mPlayer.mActorValues.emplace("level", stats.getLevel());
        state.mPlayer.mActorValues.emplace("encumbrance", player.getClass().getEncumbrance(player));
        state.mPlayer.mActorValues.emplace("carryweight", player.getClass().getCapacity(player));
        static constexpr std::array attributeNames{ "strength", "intelligence", "willpower", "agility", "speed",
            "endurance", "personality", "luck" };
        for (std::size_t i = 0; i < attributeNames.size(); ++i)
        {
            const MWMechanics::AttributeValue& value = stats.getAttribute(ESM::Attribute::indexToRefId(i));
            state.mPlayer.mActorValues.emplace(std::string(attributeNames[i]) + ".base", value.getBase());
            state.mPlayer.mActorValues.emplace(std::string(attributeNames[i]) + ".modifier", value.getModifier());
        }
        static const std::array skillIds{ ESM::Skill::Armorer, ESM::Skill::Athletics, ESM::Skill::LongBlade,
            ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand, ESM::Skill::HeavyArmor,
            ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration, ESM::Skill::Destruction,
            ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration, ESM::Skill::Acrobatics,
            ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile, ESM::Skill::Security,
            ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        static constexpr std::array skillNames{ "armorer", "athletics", "blade", "block", "blunt", "handtohand",
            "heavyarmor", "alchemy", "alteration", "conjuration", "destruction", "illusion", "mysticism",
            "restoration", "acrobatics", "lightarmor", "marksman", "mercantile", "security", "sneak",
            "speechcraft" };
        const MWMechanics::NpcStats& npcStats = player.getClass().getNpcStats(player);
        state.mPlayer.mActorValues.emplace("breath_time.current", npcStats.getTimeToStartDrowning());
        for (std::size_t i = 0; i < skillIds.size(); ++i)
        {
            const MWMechanics::SkillValue& value = npcStats.getSkill(ESM::RefId(skillIds[i]));
            state.mPlayer.mActorValues.emplace(std::string(skillNames[i]) + ".base", value.getBase());
            state.mPlayer.mActorValues.emplace(std::string(skillNames[i]) + ".modifier", value.getModifier());
        }
        const ESM::NPC* playerBase = player.get<ESM::NPC>()->mBase;
        state.mPlayer.mName = playerBase->mName;
        if (const ESM::FormId* race = playerBase->mRace.getIf<ESM::FormId>())
            state.mPlayer.mRace = resolver.toFormKey(*race);
        if (const ESM::FormId* characterClass = playerBase->mClass.getIf<ESM::FormId>())
            state.mPlayer.mClass = resolver.toFormKey(*characterClass);
        else
            state.mPlayer.mClass = ESM::FormKey::dynamic("player-class", 1);
        const ESM::RefId& birthSign = mPlayer->getBirthSign();
        if (const ESM::FormId* sign = birthSign.getIf<ESM::FormId>())
            state.mPlayer.mBirthSign = resolver.toFormKey(*sign);
        state.mPlayer.mFemale = !playerBase->isMale();
        state.mPlayer.mCharacterGenerationFlags = mPlayer->getOblivionCharacterGenerationFlags();

        // Player and native actor transactions serialize the same live
        // metadata; preserve Player-only hotkey assignments in that adapter.
        state.mPlayer.mInventory = captureOblivionActorInventory(player);

        const auto runtimeGlobalName = [](std::string_view nativeName) -> std::string_view {
            if (Misc::StringUtils::ciEqual(nativeName, "GameDaysPassed"))
                return Globals::sDaysPassed.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameDay"))
                return Globals::sDay.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameMonth"))
                return Globals::sMonth.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameYear"))
                return Globals::sYear.getValue();
            return nativeName;
        };
        for (const ESM4::GlobalVariable& global : mStore.get<ESM4::GlobalVariable>())
        {
            if (global.mEditorId.empty())
                continue;
            const ESM::Variant& value = mGlobalVariables[GlobalVariableName(runtimeGlobalName(global.mEditorId))];
            ESM4::RuntimeValue runtimeValue;
            if (value.getType() == ESM::VT_Float)
                runtimeValue = static_cast<double>(value.getFloat());
            else if (value.getType() == ESM::VT_String)
                runtimeValue = value.getString();
            else
                runtimeValue = static_cast<std::int64_t>(value.getInteger());
            state.mGlobals.emplace(resolver.toFormKey(global.mId), std::move(runtimeValue));
        }

        std::map<ESM::FormKey, const ESM4::RuntimeReferenceState*> previousReferences;
        if (mOblivionRuntimeState)
            for (const ESM4::RuntimeReferenceState& reference : mOblivionRuntimeState->mReferences)
                previousReferences.emplace(reference.mKey, &reference);



        auto& worldModel = const_cast<WorldModel&>(mWorldModel);
        try
        {
        worldModel.forEachLoadedCellStore([&](CellStore& cell) {
            cell.forEachConst(
                [&](const ConstPtr& ptr) {
                    // CellStore visitors may expose a placeholder while a
                    // reference is being removed. It has no stable identity
                    // or MWClass and cannot contribute to a TES4 save.
                    if (ptr.isEmpty())
                        return true;
                    const ESM::FormKey key = ptr.getCellRef().getFormKey();
                    if (key.isNull())
                        return true;
                    try
                    {
                    const Ptr mutablePtr(const_cast<LiveCellRefBase*>(ptr.mRef),
                        const_cast<CellStore*>(ptr.mCell));
                    const ESM::RefId baseRefId = ptr.getCellRef().getRefId();
                    const ESM::FormId* baseId = baseRefId.getIf<ESM::FormId>();
                    const ESM::FormId* cellId = ptr.getCell()->getCell()->getId().getIf<ESM::FormId>();
                    if (baseId == nullptr || cellId == nullptr)
                        throw std::runtime_error("Native TES4 reference has no FormId base or cell identity");

                    ESM4::RuntimeReferenceState reference;
                    reference.mKey = key;
                    reference.mBase = resolver.toFormKey(*baseId);
                    reference.mCell = resolver.toFormKey(*cellId);
                    reference.mEnabled = ptr.getRefData().isEnabled();
                    reference.mDeleted = ptr.mRef->isDeleted();
                    reference.mPosition = ptr.getRefData().getPosition();
                    const ESM::RefId ownerId = ptr.getCellRef().getOwner();
                    if (const ESM::FormId* owner = ownerId.getIf<ESM::FormId>())
                        reference.mOwner = resolver.toFormKey(*owner);
                    reference.mOwnershipRank = ptr.getCellRef().getNativeOwnershipRank();
                    reference.mOwnershipGlobal = OblivionProfileServices::captureOwnershipGlobal(
                        mStore, resolver, ptr.getCellRef());
                    const unsigned type = ptr.getClass().getType();
                    const bool actorReference = type == ESM::REC_NPC_4 || type == ESM::REC_CREA4;
                    if (!actorReference)
                    {
                        reference.mItemCondition = ptr.getCellRef().getNativeItemCondition();
                        const float charge = ptr.getCellRef().getEnchantmentCharge();
                        if (charge >= 0.f) reference.mItemCharge = charge;
                        try
                        {
                            reference.mLockLevel = ptr.getCellRef().getLockLevel();
                        }
                        catch (const std::logic_error&)
                        {
                            reference.mLockLevel = 0;
                        }
                    }

                    if (const auto previous = previousReferences.find(key); previous != previousReferences.end())
                    {
                        reference.mInventory = previous->second->mInventory;
                        reference.mCustomState = previous->second->mCustomState;
                        reference.mActorDrawState = previous->second->mActorDrawState;
                        if (mOblivionRuntimeState->mVersion < 4
                            && (ptr.getClass().getType() == ESM::REC_NPC_4
                                || ptr.getClass().getType() == ESM::REC_CREA4))
                            migrateLegacyActorEquipment(reference.mInventory, mStore, resolver);

                        // Scripted non-looping animations retain their final
                        // visual pose after playback, but the renderer's live
                        // controller is rebuilt when a TES4 save is loaded.
                        // Mirror its persisted queue position into the native
                        // FormKey-keyed state so that embedded-NIF animations
                        // restore through the ordinary object path as well.
                        const auto group = reference.mCustomState.find("obscript.animation_group");
                        const auto scripted = reference.mCustomState.find("obscript.animation_scripted");
                        bool isScripted = true;
                        if (scripted != reference.mCustomState.end())
                        {
                            const auto* value = std::get_if<bool>(&scripted->second);
                            if (value == nullptr)
                                throw std::runtime_error("TES4 runtime-state animation scripted flag has the wrong type: "
                                    + reference.mKey.serialize());
                            isScripted = *value;
                        }
                        if (group != reference.mCustomState.end() && isScripted)
                        {
                            if (const auto* name = std::get_if<std::string>(&group->second))
                            {
                                const ESM::AnimationState& animationState
                                    = ptr.getRefData().getAnimationState();
                                const auto animation = std::find_if(animationState.mScriptedAnims.begin(),
                                    animationState.mScriptedAnims.end(), [&](const auto& value) {
                                        return Misc::StringUtils::ciEqual(value.mGroup, *name);
                                    });
                                if (animation != animationState.mScriptedAnims.end())
                                {
                                    reference.mCustomState["obscript.animation_progress"]
                                        = static_cast<double>(animation->mTime);
                                    reference.mCustomState["obscript.animation_loop_count"]
                                        = static_cast<std::int64_t>(std::min<std::uint64_t>(animation->mLoopCount,
                                            std::numeric_limits<std::int64_t>::max()));
                                    reference.mCustomState["obscript.animation_absolute"] = animation->mAbsolute;
                                }
                                else if (const auto playing
                                    = reference.mCustomState.find("obscript.animation_playing");
                                    playing != reference.mCustomState.end()
                                    && std::get_if<bool>(&playing->second) != nullptr
                                    && *std::get_if<bool>(&playing->second))
                                {
                                    // A completed scripted non-looping group
                                    // stays posed at its stop key even though
                                    // there is no longer a live playback clock.
                                    reference.mCustomState["obscript.animation_progress"] = 1.0;
                                    reference.mCustomState["obscript.animation_playing"] = false;
                                }
                            }
                        }
                    }
                    else
                    {
                        const auto addInventory
                            = [&](const std::vector<ESM4::InventoryItem>& inventory, bool equip) {
                            for (const ESM4::InventoryItem& item : inventory)
                            {
                                const ESM::FormKey itemKey
                                    = resolver.toFormKey(ESM::FormId::fromUint32(item.item));
                                const std::int32_t count = ESM4::inventoryItemCount(item);
                                if (!itemKey.isNull() && count != 0)
                                {
                                    ESM4::RuntimeInventoryItem runtimeItem;
                                    runtimeItem.mBase = itemKey;
                                    runtimeItem.mCount = count;
                                    if (const auto definition = OblivionProfileServices::itemDefinition(
                                            mStore, ESM::RefId(ESM::FormId::fromUint32(item.item))))
                                    {
                                        runtimeItem.mCondition = definition->mMaxCondition;
                                        runtimeItem.mCharge = definition->mMaxCharge;
                                    }
                                    reference.mInventory.push_back(std::move(runtimeItem));
                                    if (equip)
                                    {
                                        auto definition = OblivionProfileServices::itemDefinition(
                                            mStore, ESM::RefId(ESM::FormId::fromUint32(item.item)));
                                        if (definition && definition->mSlots != 0)
                                        {
                                            definition->mBase = itemKey;
                                            ESM4::equipInventoryItem(reference.mInventory, *definition);
                                        }
                                    }
                                }
                            }
                        };
                        switch (ptr.getClass().getType())
                        {
                            case ESM::REC_CONT4:
                                addInventory(ptr.get<ESM4::Container>()->mBase->mInventory, false);
                                break;
                            case ESM::REC_CREA4:
                                addInventory(ptr.get<ESM4::Creature>()->mBase->mInventory, true);
                                break;
                            case ESM::REC_NPC_4:
                                addInventory(ptr.get<ESM4::Npc>()->mBase->mInventory, true);
                                break;
                            default:
                                break;
                        }
                    }
                    if (actorReference)
                    {
                        reference.mInventory = captureOblivionActorInventory(mutablePtr);
                        reference.mActorDrawState = captureOblivionActorDrawState(mutablePtr);
                    }
                    reference.mCustomState["count"]
                        = static_cast<std::int64_t>(ptr.getCellRef().getCount(false));
                    reference.mCustomState["scale"] = static_cast<double>(ptr.getCellRef().getScale());
                    reference.mCustomState["record_type"]
                        = static_cast<std::int64_t>(ptr.getClass().getType());
                    if (!actorReference)
                    {
                        try
                        {
                            reference.mCustomState["locked"] = ptr.getCellRef().isLocked();
                        }
                        catch (const std::logic_error&)
                        {
                            // Some projected references do not have lock state.
                        }
                    }
                    state.mReferences.push_back(std::move(reference));
                    return true;
                    }
                    catch (const std::exception& error)
                    {
                        throw std::runtime_error("TES4 runtime-state reference " + key.serialize()
                            + " capture failed: " + std::string(error.what()));
                    }
                },
                true);
        });
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("TES4 runtime-state reference capture failed: " + std::string(error.what()));
        }
        // Runtime-created forms may not have a projected MWClass yet. Keep their native state alive so later profile
        // slices can instantiate them without losing identity or allocation order in intervening saves.
        if (mOblivionRuntimeState)
        {
            std::set<ESM::FormKey> captured;
            for (const ESM4::RuntimeReferenceState& reference : state.mReferences)
                captured.insert(reference.mKey);
            for (const ESM4::RuntimeReferenceState& reference : mOblivionRuntimeState->mReferences)
                if (reference.mKey.isDynamic() && !captured.contains(reference.mKey))
                    state.mReferences.push_back(reference);
        }
        std::sort(state.mReferences.begin(), state.mReferences.end(), [](const auto& left, const auto& right) {
            return left.mKey < right.mKey;
        });
        if (mOblivionScriptManager)
            mOblivionScriptManager->capture(state);
        if (mOblivionAi)
            mOblivionAi->capture(state);
        if (mOblivionCombat)
            mOblivionCombat->capture(state);
        captureOblivionPhysicalState(state, player);
        const auto normalizeNativeInventory = [](std::vector<ESM4::RuntimeInventoryItem>& inventory) {
            for (ESM4::RuntimeInventoryItem& item : inventory)
                if (item.mCount < 0)
                    item.mCount = item.mCount == std::numeric_limits<std::int32_t>::min()
                        ? std::numeric_limits<std::int32_t>::max()
                        : -item.mCount;
            ESM4::normalizeInventory(inventory);
        };
        normalizeNativeInventory(state.mPlayer.mInventory);
        for (ESM4::RuntimeReferenceState& reference : state.mReferences)
            normalizeNativeInventory(reference.mInventory);
        state.validate();
        return state;
    }

    std::optional<double> World::getOblivionScriptActorValue(const ESM::FormKey& actor,
        std::uint8_t value, bool base)
    {
        const auto key = ESM4::runtimeReferenceKey(actor);
        if (!mOblivionCombat || !mOblivionCombat->findActorValues(key))
            return std::nullopt;
        Ptr resident;
        if (key == ESM::FormKey::dynamic("player", 1))
            resident = getPlayerPtr();
        else if (const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key))
            resident = mWorldModel.getPtr(*id);
        bool disabled = false;
        if (!resident.isEmpty())
            disabled = !resident.getRefData().isEnabled();
        else
        {
            const ESM4::RuntimeReferenceState* saved = nullptr;
            if (mOblivionRuntimeState)
            {
                const auto reference = std::find_if(mOblivionRuntimeState->mReferences.begin(),
                    mOblivionRuntimeState->mReferences.end(), [&](const auto& ref) { return ref.mKey == key; });
                if (reference != mOblivionRuntimeState->mReferences.end())
                    saved = &*reference;
            }
            if (saved)
                disabled = !saved->mEnabled;
            else if (const auto* reference = mStore.search<ESM4::ActorCharacter>(key))
                disabled = (reference->mFlags & ESM4::Rec_Disabled) != 0;
            else if (const auto* creatureRef = mStore.search<ESM4::ActorCreature>(key))
                disabled = (creatureRef->mFlags & ESM4::Rec_Disabled) != 0;
        }
        return mOblivionCombat->getScriptActorValue(key, value, base, disabled, mStore);
    }

    bool World::initializeOblivionNonPlayerActor(const Ptr& actor, ESM4::ActorValueProcess process, bool activate)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        bool scaled;
        if (actor.getType() == ESM::REC_NPC_4)
        {
            const auto* base = actor.get<ESM4::Npc>()->mBase;
            if (!base || !base->mIsTES4)
                return false;
            scaled = (base->mBaseConfig.tes4.flags & ESM4::Npc::TES4_PCLevelOffset) != 0;
        }
        else if (actor.getType() == ESM::REC_CREA4)
        {
            const auto* base = actor.get<ESM4::Creature>()->mBase;
            if (!base || !base->mAttackReach)
                return false;
            scaled = (base->mBaseConfig.tes4.flags & ESM4::Creature::TES4_PCLevelOffset) != 0;
        }
        else
            return false;
        const auto key = actor.getCellRef().getFormKey();
        std::optional<std::uint16_t> playerLevel;
        if (scaled && !mOblivionCombat->findActorValues(key))
        {
            const auto player = getPlayerPtr();
            if (player.isEmpty())
                throw std::invalid_argument("native scaled actor construction requires the player");
            const int level = player.getClass().getCreatureStats(player).getLevel();
            if (level < 1 || level > std::numeric_limits<std::uint16_t>::max())
                throw std::invalid_argument("invalid native construction player level");
            playerLevel = static_cast<std::uint16_t>(level);
        }
        ESM4::RuntimeReferenceState* reference = nullptr;
        if (mOblivionRuntimeState)
            for (auto& saved : mOblivionRuntimeState->mReferences)
                if (saved.mKey == key)
                {
                    reference = &saved;
                    break;
                }
        const auto marker = findLegacyDeathMarker(reference);
        std::optional<bool> legacyDead;
        if (marker)
        {
            const auto* dead = std::get_if<bool>(&(*marker)->second);
            if (!dead)
                throw std::invalid_argument("invalid legacy native death marker");
            legacyDead = *dead;
        }
        mOblivionCombat->initializeNonPlayerActor(actor, mStore, playerLevel, process, legacyDead, activate);
        if (marker)
            reference->mCustomState.erase(*marker);
        return true;
    }

    bool World::activateOblivionActor(const Ptr& actor)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        if (actor == getPlayerPtr())
        {
            const auto key = ESM::FormKey::dynamic("player", 1);
            if (mOblivionCombat->findActorValues(key) || mOblivionRuntimeState)
                return initializeOblivionPlayerActor();
            const auto* record = actor.get<ESM::NPC>()->mBase;
            if (!record)
                throw std::invalid_argument("native Player admission has no character record");
            return replaceOblivionPlayerCharacter(*record, mPlayer->getBirthSign());
        }
        return initializeOblivionNonPlayerActor(actor, ESM4::ActorValueProcess::Active, true);
    }

    bool World::replaceOblivionPlayerCharacter(const ESM::NPC& candidate, const ESM::RefId& birthSign,
        const ESM::Class* customClass, std::uint8_t characterGenerationFlags)
    {
        if (!mOblivionCombat || !mPlayer)
            return false;
        if (characterGenerationFlags & ~0x0f)
            throw std::invalid_argument("invalid native Player character-generation flags");
        static_assert(std::is_nothrow_copy_assignable_v<ESM::RefId>);
        auto metadata = mStore.preparePlayerRecord(candidate, customClass);
        const auto& proposed = metadata.player();
        const auto stats = resolveOblivionPlayerCharacterBaseStats(mStore, proposed.mRace, proposed.mClass,
            !proposed.isMale(), proposed.mNpdt.mLevel, metadata.customClass());
        const auto spells = resolveOblivionPlayerSpellInputs(mStore, proposed.mRace, birthSign);
        std::vector<MWMechanics::OblivionPassiveEffectIdentity> removalOrder;
        const auto actor = ESM::FormKey::dynamic("player", 1);
        if (const auto* previous = mOblivionCombat->findActorValues(actor))
        {
            if (!previous->mPassiveAbilities)
                throw std::invalid_argument("unknown native Player passive ownership during character choice");
            removalOrder = resolveOblivionPlayerPassiveRemovalOrder(mStore, *previous->mPassiveAbilities);
        }
        const auto settings = resolveOblivionPlayerDynamicBaseSettings(mStore);
        const bool essential = (proposed.mFlags & ESM::NPC::Essential) != 0;
        const auto recovery = essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        ESM4::RuntimeReferenceState* reference = nullptr;
        if (mOblivionRuntimeState)
            for (auto& saved : mOblivionRuntimeState->mReferences)
                if (saved.mKey == actor)
                {
                    reference = &saved;
                    break;
                }
        const auto marker = findLegacyDeathMarker(reference);
        std::optional<bool> legacyDead;
        if (marker)
        {
            const auto* dead = std::get_if<bool>(&(*marker)->second);
            if (!dead)
                throw std::invalid_argument("invalid legacy native Player death marker");
            legacyDead = *dead;
        }
        mOblivionCombat->initializePlayerCharacter(*mPlayer, mStore, stats, spells.mPassiveAbilities,
            removalOrder, settings, essential, recovery, getGodModeState(), legacyDead);
        // All allocating/validating work is finished. Publish metadata before
        // returning to any rendering, scripts, UI or actor-update callbacks.
        mPlayer->set(metadata.commit());
        mPlayer->setBirthSign(birthSign);
        mPlayer->markOblivionCharacterGeneration(characterGenerationFlags);
        if (marker)
            reference->mCustomState.erase(*marker);
        return true;
    }

    bool World::initializeOblivionPlayerActor()
    {
        if (!mOblivionCombat || !mPlayer)
            return false;
        ESM4::RuntimeReferenceState* reference = nullptr;
        if (mOblivionRuntimeState)
            for (auto& saved : mOblivionRuntimeState->mReferences)
                if (saved.mKey == ESM::FormKey::dynamic("player", 1))
                {
                    reference = &saved;
                    break;
                }
        const auto marker = findLegacyDeathMarker(reference);
        std::optional<bool> legacyDead;
        if (marker)
        {
            const auto* dead = std::get_if<bool>(&(*marker)->second);
            if (!dead)
                throw std::invalid_argument("invalid legacy native Player death marker");
            legacyDead = *dead;
        }
        const auto key = ESM::FormKey::dynamic("player", 1);
        if (mOblivionRuntimeState && mOblivionRuntimeState->mVersion < 9
            && !mOblivionCombat->findActorValues(key))
        {
            const auto values = resolveOblivionLegacyPlayerValues(*mPlayer, mStore);
            const float breath = getPlayerPtr().getClass().getNpcStats(getPlayerPtr()).getTimeToStartDrowning();
            mOblivionCombat->initializePlayerActorFromLegacyView(*mPlayer, mStore, values, breath, legacyDead);
        }
        else
            mOblivionCombat->initializePlayerActor(*mPlayer, mStore, legacyDead);
        if (marker)
            reference->mCustomState.erase(*marker);
        return true;
    }

    ESM4::RuntimeReferenceState* World::adoptOblivionActorLife(const Ptr& actor)
    {
        const bool player = actor == getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto* values = mOblivionCombat->findActorValues(key);
        if (!values)
            throw std::invalid_argument("native lifecycle requires registered actor values");
        // Adopt a legacy marker only when this actor first enters a native
        // writer. Older saves without lifecycle state remain loadable.
        ESM4::RuntimeReferenceState* reference = nullptr;
        if (mOblivionRuntimeState)
            for (auto& saved : mOblivionRuntimeState->mReferences)
                if (saved.mKey == key)
                {
                    reference = &saved;
                    break;
                }
        if (!mOblivionCombat->findActorLife(key))
        {
            ESM4::RuntimeActorLife life;
            life.mActor = key;
            life.mBase = values->mBase;
            life.mPhase = actor.getClass().getCreatureStats(actor).isDead()
                ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::Alive;
            if (reference)
                if (const auto marker = reference->mCustomState.find("obscript.dead");
                    marker != reference->mCustomState.end())
                {
                    const auto* dead = std::get_if<bool>(&marker->second);
                    if (!dead)
                        throw std::invalid_argument("invalid legacy native death marker");
                    life.mPhase = *dead ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::Alive;
                }
            if (player)
                mOblivionCombat->publishPlayerLife(*mPlayer, std::move(life));
            else
                mOblivionCombat->publishNonPlayerLife(actor, std::move(life));
        }
        return reference;
    }

    std::uint64_t World::beginOblivionPhysicalAction(const Ptr& actor)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return 0;
        const bool player = actor == getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            return 0;
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto* life = mOblivionCombat->findActorLife(key);
        if (!mOblivionCombat->findActorValues(key) || !life || life->mPhase != ESM4::ActorLifePhase::Alive)
            return 0;
        // Validate the actual resident binding before issuing an identity.
        if (!player)
            static_cast<void>(mOblivionCombat->getNonPlayerValue(actor, 8));
        return mOblivionCombat->allocateAction(key);
    }

    bool World::cancelOblivionPhysicalAction(std::uint64_t id, const Ptr& actor)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const bool player = actor == getPlayerPtr();
        if (!player && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
            return false;
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        if (!mOblivionCombat->isActionPending(id, key))
            return false;
        if (!player)
            static_cast<void>(mOblivionCombat->getNonPlayerValue(actor, 8));
        return mOblivionCombat->consumeAction(id, key);
    }

    bool World::commitOblivionPhysicalContact(std::uint64_t id, const Ptr& attacker, const Ptr& victim,
        const MWMechanics::OblivionPhysicalContactDeltas& deltas)
    {
        if (!mOblivionCombat || attacker.isEmpty())
            return false;
        const bool player = attacker == getPlayerPtr();
        if (!player && attacker.getType() != ESM::REC_NPC_4 && attacker.getType() != ESM::REC_CREA4)
            return false;
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : attacker.getCellRef().getFormKey();
        if (!mOblivionCombat->isActionPending(id, key))
            return false;
        if (!victim.isEmpty() && victim != getPlayerPtr()
            && victim.getType() != ESM::REC_NPC_4 && victim.getType() != ESM::REC_CREA4)
            return false;
        const bool essential = !victim.isEmpty() && victim.getClass().isEssential(victim);
        const auto recovery = essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        return mOblivionCombat->commitPhysicalContact(id, attacker, victim, deltas, mPlayer.get(), essential,
            recovery, resolveOblivionPlayerDynamicBaseSettings(mStore), getGodModeState());
    }

    bool World::getOblivionKnockback(const Ptr& actor, ESM4::TimedKnockbackState& state) const
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const auto key = actor == getPlayerConstPtr() ? ESM::FormKey::dynamic("player", 1)
            : actor.getCellRef().getFormKey();
        const auto pulse = mOblivionCombat->actorKnockback(key);
        if (!pulse)
            return false;
        state = *pulse;
        return true;
    }

    void World::syncOblivionKnockback(const Ptr& actor, const ESM4::TimedKnockbackState& expected,
        const ESM4::TimedKnockbackState& updated)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return;
        const auto key = actor == getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : actor.getCellRef().getFormKey();
        mOblivionCombat->syncActorKnockback(key, expected, updated);
    }

    bool World::killOblivionActor(const Ptr& actor, const ESM::FormKey& killer)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const bool player = actor == getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        if (!mOblivionCombat->findActorValues(key))
            return false;
        const bool essential = actor.getClass().isEssential(actor);
        const auto settings = essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        auto adoption = mOblivionCombat->guardLifeAdoption(actor, player ? mPlayer.get() : nullptr);
        auto* reference = adoptOblivionActorLife(actor);
        const auto deadMarker = findLegacyDeathMarker(reference);
        if (player)
            mOblivionCombat->killPlayer(*mPlayer, killer, essential, settings, getGodModeState());
        else
            mOblivionCombat->killNonPlayer(actor, killer, essential, settings);
        if (deadMarker)
            reference->mCustomState.erase(*deadMarker);
        adoption.commit();
        return true;
    }

    bool World::executeOblivionActorValueCommand(const ESM::FormKey& actor, std::uint8_t value,
        ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source, std::int32_t requested)
    {
        if (!mOblivionCombat)
            return false;
        const auto key = ESM4::runtimeReferenceKey(actor);
        if (key == ESM::FormKey::dynamic("player", 1))
            return executeOblivionActorValueCommand(getPlayerPtr(), value, command, source, requested);
        const auto* values = mOblivionCombat->findActorValues(key);
        if (!values)
            return false;
        std::vector<Ptr> residents;
        for (const auto& ptr : mWorldModel.getResidentPtrs())
        {
            if (ptr.isEmpty() || ptr == getPlayerPtr())
                continue;
            if (ptr.getCellRef().getFormKey() == key)
                return executeOblivionActorValueCommand(ptr, value, command, source, requested);
            if (command == ESM4::ActorValueCommand::Set)
                if (const auto* native = mOblivionCombat->findActorValues(ptr.getCellRef().getFormKey());
                    native && native->mBase == values->mBase)
                    residents.push_back(ptr);
        }
        if (!mOblivionCombat->findActorLife(key))
            throw std::invalid_argument("unloaded native command requires initialized lifecycle");
        const auto* npc = mStore.search<ESM4::Npc>(values->mBase);
        const auto* creature = mStore.search<ESM4::Creature>(values->mBase);
        if ((!npc && !creature) || (npc && creature))
            throw std::invalid_argument("unloaded native command requires an unambiguous actor base");
        const bool essential = npc ? (npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Essential) != 0
                                  : (creature->mBaseConfig.tes4.flags & ESM4::Creature::TES4_Essential) != 0;
        const auto recovery = value == 8 && essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        mOblivionCombat->executeUnloadedValueCommand(key, mStore, value, command, source, requested,
            {false, npc != nullptr}, residents, essential, recovery);
        if (mOblivionRuntimeState)
            for (auto& reference : mOblivionRuntimeState->mReferences)
                if (reference.mKey == key)
                {
                    reference.mCustomState.erase("obscript.dead");
                    break;
                }
        return true;
    }

    bool World::executeOblivionActorValueCommand(const Ptr& actor, std::uint8_t value,
        ESM4::ActorValueCommand command, ESM4::ActorValueCommandSource source, std::int32_t requested)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const bool player = actor == getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto* values = mOblivionCombat->findActorValues(key);
        if (!values)
            return false;
        const auto playerSettings = resolveOblivionPlayerDynamicBaseSettings(mStore);
        const bool essential = actor.getClass().isEssential(actor);
        const auto recoverySettings = value == 8 && essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        std::vector<Ptr> residents;
        if (!player && command == ESM4::ActorValueCommand::Set)
        {
            for (const auto& resident : mWorldModel.getResidentPtrs())
            {
                if (resident.isEmpty() || resident == getPlayerPtr())
                    continue;
                const auto* native = mOblivionCombat->findActorValues(resident.getCellRef().getFormKey());
                if (native && native->mBase == values->mBase)
                    residents.push_back(resident);
            }
            if (std::find(residents.begin(), residents.end(), actor) == residents.end())
                residents.push_back(actor);
        }
        auto adoption = mOblivionCombat->guardLifeAdoption(actor, player ? mPlayer.get() : nullptr);
        auto* reference = adoptOblivionActorLife(actor);
        const auto deadMarker = findLegacyDeathMarker(reference);
        const ESM4::ActorValueCommandPolicy policy{player && getGodModeState(), actor.getType() != ESM::REC_CREA4};
        if (player)
            mOblivionCombat->executePlayerValueCommand(*mPlayer, value, command, source, requested, policy,
                playerSettings, essential, recoverySettings);
        else
            mOblivionCombat->executeNonPlayerValueCommand(actor, value, command, source, requested, policy,
                residents, essential, recoverySettings);
        if (deadMarker)
            reference->mCustomState.erase(*deadMarker);
        adoption.commit();
        return true;
    }

    bool World::updateOblivionBreath(const Ptr& actor, float duration)
    {
        if (!mOblivionCombat)
            return false;
        // Native high-process breath runs only for physically resident actors.
        if (!actor.isInCell() || !mPhysics->getActor(actor))
            return true;
        const auto* cell = actor.getCell();
        const float height = 2.f * mPhysics->getRenderingHalfExtents(actor).z();
        const bool deep = cell->getCell()->hasWater()
            && ESM4::actorWaterProbe(actor.getRefData().getPosition().pos[2], height, .875f, cell->getWaterLevel());
        // The shared character controller uses this same swimming predicate.
        const bool needsAir = ESM4::actorNeedsAir(actor.getClass().isPureWaterCreature(actor), deep, isSwimming(actor));
        const bool player = actor == getPlayerPtr();
        const bool essential = actor.getClass().isEssential(actor);
        const auto recovery = essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        const auto settings = resolveOblivionSwimBreathSettings(mStore);
        auto adoption = mOblivionCombat->guardLifeAdoption(actor, player ? mPlayer.get() : nullptr);
        adoptOblivionActorLife(actor);
        const auto update = player
            ? mOblivionCombat->updatePlayerBreath(*mPlayer, duration, needsAir, essential, settings,
                recovery, resolveOblivionPlayerDynamicBaseSettings(mStore), getGodModeState())
            : mOblivionCombat->updateNonPlayerBreath(actor, duration, needsAir, essential, settings, recovery);
        adoption.commit(); // Audio/UI notifications run after the native transaction.
        if (update && update->mDamage > 0.f)
        {
            const auto sound = MWBase::Environment::get().getSoundManager();
            if (const auto key = mStore.findEsm4FormKey("NPCHumanDrowning"))
                if (const auto* record = mStore.search<ESM4::Sound>(*key))
                    if (!sound->getSoundPlaying(actor, record->mId))
                        sound->playSound3D(actor, record->mId, 1.f, 1.f);
            if (player)
                MWBase::Environment::get().getWindowManager()->activateHitOverlay(false);
        }
        return true;
    }

    std::optional<std::pair<float, float>> World::getOblivionBreath(const Ptr& actor) const
    {
        if (!mOblivionCombat)
            return std::nullopt;
        const bool player = actor == mPlayer->getPlayer();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto remaining = mOblivionCombat->findActorBreath(key);
        if (!remaining)
            return std::nullopt;
        const auto endurance = player ? mOblivionCombat->getPlayerIntegerValue(5)
            : mOblivionCombat->getNonPlayerIntegerValue(actor, 5);
        return std::pair{*remaining,
            ESM4::swimBreathMaximum(endurance, resolveOblivionSwimBreathSettings(mStore))};
    }

    bool World::requestOblivionResourceCurrent(const Ptr& actor, std::uint8_t value, float requested)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const bool player = actor == getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        if (!mOblivionCombat->findActorValues(key))
            return false;
        if (value < 8 || value > 10 || !std::isfinite(requested))
            throw std::invalid_argument("native resource request requires a finite Health/Magicka/Fatigue value");
        const auto settings = resolveOblivionPlayerDynamicBaseSettings(mStore);
        const bool essential = actor.getClass().isEssential(actor);
        const auto recovery = value == 8 && essential
            ? resolveOblivionEssentialRecoverySettings(mStore) : ESM4::EssentialRecoverySettings{};
        auto adoption = mOblivionCombat->guardLifeAdoption(actor, player ? mPlayer.get() : nullptr);
        auto* reference = adoptOblivionActorLife(actor);
        const auto deadMarker = findLegacyDeathMarker(reference);
        if (player)
            mOblivionCombat->requestPlayerResourceCurrent(*mPlayer, value, requested, getGodModeState(),
                essential, recovery, settings);
        else
            mOblivionCombat->requestNonPlayerResourceCurrent(actor, value, requested,
                {false, actor.getType() != ESM::REC_CREA4}, essential, recovery);
        if (deadMarker)
            reference->mCustomState.erase(*deadMarker);
        adoption.commit();
        return true;
    }

    bool World::requestOblivionStatModifier(const Ptr& actor, std::uint8_t value, bool damage, float requested)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const bool player = actor == getPlayerPtr();
        const auto key = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        if (!mOblivionCombat->findActorValues(key))
            return false;
        const auto modifier = damage ? ESM4::ActorValueModifier::Damage : ESM4::ActorValueModifier::Maximum;
        // Lua damage is a positive debit; the native Damage channel is signed.
        const float target = damage ? -requested : requested;
        if (player)
            mOblivionCombat->requestPlayerStatModifier(*mPlayer, value, modifier, target,
                resolveOblivionPlayerDynamicBaseSettings(mStore));
        else
            mOblivionCombat->requestNonPlayerStatModifier(actor, value, modifier, target);
        return true;
    }

    bool World::requestOblivionStatBase(const Ptr& actor, std::uint8_t value, float requested)
    {
        if (!mOblivionCombat || actor.isEmpty())
            return false;
        const auto key = actor == getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
                                                : actor.getCellRef().getFormKey();
        if (!mOblivionCombat->findActorValues(key))
            return false;
        // The native CRT selects SSE conversion when CPU and OS support SSE2.
        // Use that modern reference behavior explicitly on every host, rather
        // than depending on this port's floating-point cast implementation.
        const auto integer = ESM4::convertActorBaseFloat(requested, ESM4::ActorValueConversionMode::Sse);
        return executeOblivionActorValueCommand(actor, value, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, integer);
    }

    void World::advanceOblivionActorClock(float duration)
    {
        if (mOblivionCombat)
            mOblivionCombat->advanceFrameClock(duration);
    }

    bool World::updateOblivionFrameResources(const Ptr& actor, float, bool running)
    {
        if (!mOblivionCombat)
            return false;
        const auto settings = resolveOblivionFrameSettings(mStore);
        const bool player = actor == getPlayerPtr();
        // Original Character/Player virtual +278 returns true. The Creature
        // constructor initializes its separate expenditure flag to false.
        const bool canSpend = actor.getType() != ESM::REC_CREA4 && (!player || !getGodModeState());
        const MWMechanics::OblivionActorMovement movement{
            ESM4::combatBaseValue(actor.getClass().getEncumbrance(actor)), running, canSpend};
        // Native spell/enchantment execution is an explicit M16 boundary below.
        // No native active casting item exists yet. M16 must wire its actual
        // item lifetime here; selected spell and animation flags are not that state.
        if (player)
            mOblivionCombat->updatePlayerFrameResourcesFromClock(*mPlayer, movement, false, settings);
        else
            mOblivionCombat->updateNonPlayerFrameResourcesFromClock(actor, movement, false, settings);
        return true;
    }

    bool World::spendOblivionJumpFatigue(const Ptr& actor)
    {
        if (!mOblivionCombat)
            return false;
        // Original jump expenditure is in PlayerCharacter update +228, not
        // the common actor update. Native nonplayers must bypass the TES3 debit.
        if (actor != getPlayerPtr())
        {
            mOblivionCombat->getNonPlayerValue(actor, 10); // Validate native ownership.
            return true;
        }
        const auto settings = resolveOblivionFatigueSettings(mStore);
        const auto encumbrance = ESM4::combatBaseValue(actor.getClass().getEncumbrance(actor));
        mOblivionCombat->spendPlayerJumpFatigue(*mPlayer, encumbrance, !getGodModeState(), settings);
        return true;
    }

    std::optional<bool> World::isOblivionInCombat(const Ptr& actor) const
    {
        if (!mOblivionCombat)
            return std::nullopt;
        if (actor.isEmpty())
            return false;
        const auto key = actor == mPlayer->getPlayer()
            ? ESM::FormKey::dynamic("player", 1) : ESM4::runtimeReferenceKey(actor.getCellRef().getFormKey());
        return mOblivionCombat->isInCombat(key);
    }

    std::optional<bool> World::isOblivionInCombatWith(const Ptr& actor, const Ptr& opponent) const
    {
        if (!mOblivionCombat)
            return std::nullopt;
        if (actor.isEmpty() || opponent.isEmpty())
            return false;
        const auto key = [&](const Ptr& ptr) {
            return ptr == mPlayer->getPlayer() ? ESM::FormKey::dynamic("player", 1)
                : ESM4::runtimeReferenceKey(ptr.getCellRef().getFormKey());
        };
        return mOblivionCombat->isInCombatWith(key(actor), key(opponent));
    }

    float World::getOblivionPlayerInventoryWeight() const
    {
        float result = 0.f;
        const Ptr player = const_cast<World*>(this)->getPlayerPtr();
        const ContainerStore& inventory = player.getClass().getContainerStore(player);
        for (auto iterator = inventory.cbegin(); iterator != inventory.cend(); ++iterator)
        {
            const ConstPtr item = *iterator;
            if (item.getCellRef().getCount() > 0)
                result += item.getClass().getWeight(item) * item.getCellRef().getCount();
        }
        return std::max(0.f, result);
    }

    void World::applyOblivionRuntimeState()
    {
        mSharedDefinitionsPrepared = false;
        if (!mPendingOblivionRuntimeState)
        {
            if (mOblivionRuntimeState)
                return; // An accepted snapshot must never be replayed by a second apply.
            // A legacy save may omit T4ST, but cannot then declare owners in
            // the native Lua companion. Do not silently orphan those scripts.
            MWBase::Environment::get().getLuaManager()->validateNativeState({});
            return;
        }
        const ESM4::RuntimeState& state = *mPendingOblivionRuntimeState;
        for (const auto& reference : state.mReferences)
            if (const auto marker = reference.mCustomState.find("obscript.dead");
                marker != reference.mCustomState.end() && !std::holds_alternative<bool>(marker->second))
                throw std::invalid_argument("invalid legacy native death marker");
        // This namespace is allocated exclusively by the native World. A
        // restored high-water mark must not reuse a live or deleted identity.
        for (const auto& reference : state.mReferences)
            if (reference.mKey.isDynamic() && reference.mKey.mNamespace == "native-reference"
                && reference.mKey.mValue >= state.mNextDynamicSerial)
                throw std::invalid_argument("native reference serial would reuse a saved identity");
        if (!mPlayer || getPlayerPtr().isEmpty())
            throw std::runtime_error("TES4 runtime-state apply requires ready Player data");
        // Native actor bindings and service allocation must fail before any
        // globals, inventories, serials or projected player stats are changed.
        MWBase::Environment::get().getLuaManager()->validateNativeState(state);
        std::optional<MWMechanics::OblivionCombatService> preparedCombat;
        std::optional<OblivionScriptManager::PreparedRestore> preparedScripts;
        std::optional<MWMechanics::OblivionAiService::PreparedRestore> preparedAi;
        auto retained = std::move(mPendingOblivionServices);
        if (retained)
        {
            // Admission prepared these allocations against immutable content.
            // Consume them only now; pending state was never a live authority.
            preparedCombat = std::move(retained->mCombat);
            preparedScripts = std::move(retained->mScripts);
            preparedAi = std::move(retained->mAi);
        }
        else
        {
            // Direct record readers and retries after a later preparation
            // failure still use the synchronous detached preparation path.
            if (mOblivionCombat)
            {
                preparedCombat.emplace();
                preparedCombat->restore(state, mStore);
                preparedCombat->validateRestoredPlayerBinding();
            }
            else
                state.validate();
            if (mOblivionScriptManager)
                preparedScripts.emplace(mOblivionScriptManager->prepareRestore(state));
            if (mOblivionAi)
                preparedAi.emplace(mOblivionAi->prepareRestore(state));
        }
        const bool nativePlayerValues = preparedCombat
            && preparedCombat->findActorValues(ESM::FormKey::dynamic("player", 1));
        static constexpr std::array attributeNames{ "strength", "intelligence", "willpower", "agility", "speed",
            "endurance", "personality", "luck" };
        static constexpr std::array skillNames{ "armorer", "athletics", "blade", "block", "blunt", "handtohand",
            "heavyarmor", "alchemy", "alteration", "conjuration", "destruction", "illusion", "mysticism",
            "restoration", "acrobatics", "lightarmor", "marksman", "mercantile", "security", "sneak",
            "speechcraft" };
        const auto validateFloatInput = [](double value, std::string_view name) {
            if (!std::isfinite(value) || value < -std::numeric_limits<float>::max()
                || value > std::numeric_limits<float>::max())
                throw std::runtime_error("TES4 runtime-state " + std::string(name)
                    + " exceeds the finite float domain");
        };
        validateFloatInput(state.mClock.mTimeScale, "time scale");
        const auto validatePlayerFloat = [&](std::string_view name) {
            const auto found = state.mPlayer.mActorValues.find(std::string(name));
            if (found != state.mPlayer.mActorValues.end())
                validateFloatInput(found->second, name);
        };
        validatePlayerFloat("breath_time.current");
        if (const auto level = state.mPlayer.mActorValues.find("level"); level != state.mPlayer.mActorValues.end())
        {
            const double integral = std::trunc(level->second);
            if (integral < std::numeric_limits<int>::min() || integral > std::numeric_limits<int>::max())
                throw std::runtime_error("TES4 runtime-state level exceeds the integer conversion domain");
        }
        if (!nativePlayerValues)
        {
            for (const auto* name : {"health", "magicka", "fatigue"})
            {
                const std::string prefix(name);
                validatePlayerFloat(prefix + ".base");
                validatePlayerFloat(prefix + ".current");
                if (state.mPlayer.mActorValues.contains(prefix + ".modifier"))
                    validatePlayerFloat(prefix + ".modifier");
                else
                    validatePlayerFloat(prefix + ".modified");
            }
            const auto validateBaseAndModifier = [&](const auto& names) {
                for (const auto* name : names)
                {
                    const std::string prefix(name);
                    validatePlayerFloat(prefix + ".base");
                    validatePlayerFloat(prefix + ".modifier");
                }
            };
            validateBaseAndModifier(attributeNames);
            validateBaseAndModifier(skillNames);
        }
        std::optional<std::array<MWMechanics::DynamicStat<float>, 3>> preparedLegacyResources;
        if (!nativePlayerValues)
        {
            const auto prepareDynamicStat = [&](std::string_view name,
                                                const MWMechanics::DynamicStat<float>& current) {
                const std::string prefix(name);
                const auto value = [&](std::string_view suffix, double fallback) {
                    const auto found = state.mPlayer.mActorValues.find(prefix + std::string(suffix));
                    const double selected = found != state.mPlayer.mActorValues.end() ? found->second : fallback;
                    validateFloatInput(selected, prefix + std::string(suffix));
                    return static_cast<float>(selected);
                };
                const float base = value(".base", current.getBase());
                float modifier = value(".modifier", current.getModifier());
                if (!state.mPlayer.mActorValues.contains(prefix + ".modifier"))
                {
                    const auto oldModified = state.mPlayer.mActorValues.find(prefix + ".modified");
                    if (oldModified != state.mPlayer.mActorValues.end())
                        modifier = static_cast<float>(oldModified->second) - base;
                }
                validateFloatInput(modifier, prefix + " computed modifier");
                MWMechanics::DynamicStat<float> result(base, modifier, value(".current", current.getCurrent()));
                validateFloatInput(result.getModified(false), prefix + " computed maximum");
                return result;
            };
            const auto player = getPlayerPtr();
            const auto& current = player.getClass().getCreatureStats(player);
            // Legacy snapshots have no authority that can own an existing
            // native projection. Normal load reconstructs plain Player data;
            // reject an in-place downgrade before publishing any World state.
            if (current.getHealth().isNativeProjection() || current.getMagicka().isNativeProjection()
                || current.getFatigue().isNativeProjection())
                throw std::invalid_argument("legacy native restore requires reconstructed Player data");
            preparedLegacyResources.emplace(std::array{
                prepareDynamicStat("health", current.getHealth()), prepareDynamicStat("magicka", current.getMagicka()),
                prepareDynamicStat("fatigue", current.getFatigue())});
        }
        const ESM::FormKeyResolver resolver(mContentFiles);
        std::optional<ESMStore::PreparedPlayerRecord> preparedPlayerRecord;
        ESM::RefId preparedBirthSign;
        if (state.mVersion >= 3)
        {
            const auto race = resolver.toFormId(state.mPlayer.mRace);
            const auto characterClass = resolver.toFormId(state.mPlayer.mClass);
            ESM::NPC candidate = *mStore.get<ESM::NPC>().find(ESM::RefId::stringRefId("Player"));
            if (!characterClass && !state.mPlayer.mClass.isDynamic())
                throw std::runtime_error("TES4 runtime-state player class cannot be resolved");
            const ESM::RefId classId = characterClass ? ESM::RefId(*characterClass) : candidate.mClass;
            if (!race || !mStore.get<ESM::Race>().search(ESM::RefId(*race))
                || !mStore.get<ESM::Class>().search(classId))
                throw std::runtime_error("TES4 runtime-state player race/class cannot be resolved");
            if (!state.mPlayer.mBirthSign.isNull())
            {
                const auto sign = resolver.toFormId(state.mPlayer.mBirthSign);
                if (!sign || !mStore.get<ESM::BirthSign>().search(ESM::RefId(*sign)))
                    throw std::runtime_error("TES4 runtime-state player birthsign cannot be resolved");
                preparedBirthSign = ESM::RefId(*sign);
            }
            candidate.mName = state.mPlayer.mName;
            candidate.mRace = ESM::RefId(*race);
            candidate.mClass = classId;
            candidate.setIsMale(!state.mPlayer.mFemale);
            preparedPlayerRecord.emplace(mStore.preparePlayerRecord(candidate));
        }
        const std::optional<ESM::FormId> playerCellId = resolver.toFormId(state.mPlayer.mCell);
        if (!playerCellId || !playerCellId->hasContentFile())
            throw std::runtime_error("TES4 runtime-state player cell cannot be resolved");
        CellStore& playerCell = mWorldModel.getCell(ESM::RefId(*playerCellId));
        // Resolve all target cells before indexing references so moved actors
        // remain independent of record order. Cell caches may load during preflight.
        for (const auto& reference : state.mReferences)
        {
            const auto cellId = resolver.toFormId(reference.mCell);
            if (!cellId || !cellId->hasContentFile())
                throw std::runtime_error("TES4 runtime-state reference cell cannot be resolved: "
                    + reference.mCell.serialize());
            mWorldModel.getCell(ESM::RefId(*cellId));
        }
        std::map<ESM::FormKey, Ptr> references;
        mWorldModel.forEachLoadedCellStore([&](CellStore& cell) {
            cell.forEach(
                [&](const Ptr& ptr) {
                    const auto key = ptr.getCellRef().getFormKey();
                    if (!key.isNull())
                    {
                        const auto [owner, inserted] = references.emplace(key, ptr);
                        if (!inserted && owner->second != ptr)
                            throw std::invalid_argument("TES4 runtime-state has multiple live owners for reference: "
                                + key.serialize());
                    }
                    return true;
                }, true);
        });
        struct PreparedReferenceBinding
        {
            const ESM4::RuntimeReferenceState* mState;
            Ptr mReference;
            CellStore* mCell;
            std::optional<ESM::RefId> mOwner;
            std::optional<bool> mLocked;
            std::optional<float> mScale;
            std::optional<ESM::AnimationState> mAnimation;
            std::optional<CellRef> mItemCellRef;
        };
        std::vector<PreparedReferenceBinding> preparedReferences;
        std::vector<std::unique_ptr<PreparedSavedNativeReference>> reconstructed;
        preparedReferences.reserve(state.mReferences.size());
        for (const auto& reference : state.mReferences)
        {
            auto found = references.find(reference.mKey);
            if (found == references.end() && reference.mKey.isDynamic()
                && reference.mKey.mNamespace == "native-reference")
            {
                const auto base = resolver.toFormId(reference.mBase);
                if (!base)
                    throw std::invalid_argument("native saved reference base cannot be resolved");
                const auto cellId = resolver.toFormId(reference.mCell);
                CellStore& cell = mWorldModel.getCell(ESM::RefId(*cellId));
                ESM4::Reference placed{};
                placed.mFormKey = reference.mKey;
                placed.mParent = ESM::RefId(*cellId);
                placed.mParentKey = reference.mCell;
                placed.mBaseObj = *base;
                placed.mBaseKey = reference.mBase;
                placed.mPos = reference.mPosition;
                reconstructed.push_back(prepareSavedNativeReference(mStore, cell, placed));
                found = references.emplace(reference.mKey, reconstructed.back()->get()).first;
            }
            if (found == references.end())
            {
                if (reference.mKey.isDynamic())
                    continue; // Retain unprojected dynamic state for its future MWClass.
                throw std::runtime_error("TES4 runtime-state reference is not present: " + reference.mKey.serialize());
            }
            const auto baseId = resolver.toFormId(reference.mBase);
            const auto actualBaseId = found->second.getCellRef().getRefId();
            const auto* actualBase = actualBaseId.getIf<ESM::FormId>();
            if (!baseId || actualBase == nullptr || *baseId != *actualBase)
                throw std::runtime_error("TES4 runtime-state reference base mismatch: " + reference.mKey.serialize());
            if (reference.mActorDrawState)
                savedActorDrawState(mStore, found->second, state); // Validate incoming state, not the accepted cache.
            std::optional<CellRef> itemCellRef;
            const bool hasItemExtras = reference.mItemCondition || reference.mItemCharge;
            const auto& itemClass = found->second.getClass();
            if (hasItemExtras && !itemClass.isItem(found->second))
                throw std::invalid_argument("TES4 loose item extras require a native takeable reference");
            if (itemClass.isItem(found->second))
            {
                const auto definition = OblivionProfileServices::itemDefinition(mStore, actualBaseId);
                if (!definition || (reference.mItemCondition && definition->mMaxCondition < 0)
                    || (reference.mItemCharge && definition->mMaxCharge < 0.f))
                    throw std::invalid_argument("TES4 loose item extras disagree with the winning item category");
                itemCellRef.emplace(found->second.getCellRef());
                itemCellRef->resetNativeItemCondition();
                if (reference.mItemCondition)
                    itemCellRef->setNativeItemCondition(*reference.mItemCondition);
                itemCellRef->setEnchantmentCharge(reference.mItemCharge.value_or(-1.f));
            }

            if (state.mVersion >= 41)
            {
                if (!itemCellRef) itemCellRef.emplace(found->second.getCellRef());
                const auto global = OblivionProfileServices::resolveOwnershipGlobal(mStore, resolver,
                    reference.mOwnershipGlobal);
                itemCellRef->setNativeOwnershipGlobal(global);
                if (itemCellRef->getNativeReference() || !itemClass.isActor())
                    itemCellRef->setNativeOwnershipRank(reference.mOwnershipRank);
                else if (reference.mOwnershipRank)
                    throw std::invalid_argument("Native actor reference cannot own a REFR rank extra");
            }

            std::optional<ESM::RefId> owner;
            if (reference.mOwner)
            {
                const auto ownerId = resolver.toFormId(*reference.mOwner);
                if (!ownerId || !ownerId->hasContentFile())
                    throw std::runtime_error("TES4 runtime-state owner cannot be resolved: "
                        + reference.mOwner->serialize());
                owner.emplace(*ownerId);
            }
            std::optional<bool> locked;
            if (const auto saved = reference.mCustomState.find("locked"); saved != reference.mCustomState.end())
            {
                const auto* value = std::get_if<bool>(&saved->second);
                if (value == nullptr)
                    throw std::runtime_error(
                        "TES4 runtime-state locked value is not boolean: " + reference.mKey.serialize());
                locked = *value;
            }
            std::optional<float> scale;
            if (const auto saved = reference.mCustomState.find("scale"); saved != reference.mCustomState.end())
                if (const auto* value = std::get_if<double>(&saved->second))
                {
                    validateFloatInput(*value, "reference scale");
                    scale = static_cast<float>(*value);
                }
            std::optional<ESM::AnimationState> animationState;
            const auto animationGroup = reference.mCustomState.find("obscript.animation_group");
            const auto animationProgress = reference.mCustomState.find("obscript.animation_progress");
            const auto animationScripted = reference.mCustomState.find("obscript.animation_scripted");
            bool scriptedAnimation = true;
            if (animationScripted != reference.mCustomState.end())
            {
                const auto* value = std::get_if<bool>(&animationScripted->second);
                if (value == nullptr)
                    throw std::runtime_error("TES4 runtime-state animation scripted flag has the wrong type: "
                        + reference.mKey.serialize());
                scriptedAnimation = *value;
            }
            std::optional<double> progress;
            if (animationProgress != reference.mCustomState.end())
            {
                if (const auto* value = std::get_if<double>(&animationProgress->second))
                    progress = *value;
                else
                    throw std::runtime_error(
                        "TES4 runtime-state animation value has the wrong type: " + reference.mKey.serialize());
            }
            else if (const auto playing = reference.mCustomState.find("obscript.animation_playing");
                playing != reference.mCustomState.end())
            {
                // Migration for M7 development saves that recorded the group
                // before animation progress became part of native state.
                if (const auto* value = std::get_if<bool>(&playing->second); value != nullptr && *value)
                    progress = 1.0;
            }
            if (animationGroup != reference.mCustomState.end() && progress && scriptedAnimation)
            {
                const auto* group = std::get_if<std::string>(&animationGroup->second);
                if (group == nullptr || group->empty())
                    throw std::runtime_error(
                        "TES4 runtime-state animation group has the wrong type: " + reference.mKey.serialize());
                ESM::AnimationState::ScriptedAnimation animation;
                animation.mGroup = *group;
                animation.mTime = static_cast<float>(std::clamp(*progress, 0.0, 1.0));
                if (const auto loopCount
                    = reference.mCustomState.find("obscript.animation_loop_count");
                    loopCount != reference.mCustomState.end())
                    if (const auto* value = std::get_if<std::int64_t>(&loopCount->second))
                        animation.mLoopCount = static_cast<std::uint64_t>(std::max<std::int64_t>(0, *value));
                if (const auto absolute = reference.mCustomState.find("obscript.animation_absolute");
                    absolute != reference.mCustomState.end())
                    if (const auto* value = std::get_if<bool>(&absolute->second))
                        animation.mAbsolute = *value;
                animationState.emplace();
                animationState->mScriptedAnims.push_back(std::move(animation));
            }
            const auto cellId = resolver.toFormId(reference.mCell);
            preparedReferences.push_back({&reference, found->second,
                &mWorldModel.getCell(ESM::RefId(*cellId)), owner, locked, scale,
                std::move(animationState), std::move(itemCellRef)});
        }
        // Construct detached replacement items before changing globals, player
        // identity or live inventories. Content/owner/projection errors must not
        // leave an earlier inventory cleared or only partly reconstructed.
        std::vector<std::pair<Ptr, CellStore*>> cellMoves;
        std::vector<Ptr> relocatedReferences;
        std::set<LiveCellRefBase*> relocatedBases;
        for (auto& binding : preparedReferences)
            if (binding.mReference.getCell() != binding.mCell)
            {
                cellMoves.emplace_back(binding.mReference, binding.mCell);
                binding.mReference = Ptr(binding.mReference.getBase(), binding.mCell);
                relocatedReferences.push_back(binding.mReference);
                relocatedBases.insert(binding.mReference.getBase());
            }
        auto preparedCellMoves = CellStore::prepareMoves(cellMoves);
        auto playerInventory = retained ? std::move(retained->mPlayerInventory)
            : OblivionProfileServices::stageActorInventory(OblivionProfileServices::prepareActorInventory(
                mStore, resolver, state.mPlayer.mInventory));
        std::map<ESM::FormKey, std::unique_ptr<InventoryStore>> actorInventories;
        if (retained)
            actorInventories = std::move(retained->mActorInventories);
        else
            for (const auto& reference : state.mReferences)
            {
                const auto base = resolver.toFormId(reference.mBase);
                if (base && (mStore.get<ESM4::Npc>().search(ESM::RefId(*base))
                    || mStore.get<ESM4::Creature>().search(ESM::RefId(*base))))
                {
                    auto inventory = reference.mInventory;
                    if (state.mVersion < 4)
                        migrateLegacyActorEquipment(inventory, mStore, resolver);
                    actorInventories.emplace(reference.mKey, OblivionProfileServices::stageActorInventory(
                        OblivionProfileServices::prepareActorInventory(mStore, resolver, inventory)));
                }
            }
        struct PreparedInventoryBinding
        {
            InventoryStore* mTarget;
            std::unique_ptr<InventoryStore> mContents;
        };
        std::vector<PreparedInventoryBinding> inventories;
        inventories.reserve(preparedReferences.size() + 1);
        std::vector<Ptr> removedItems, insertedItems;
        const auto stageInventory = [&](const Ptr& actor, std::unique_ptr<InventoryStore> contents) {
            auto& target = actor.getClass().getInventoryStore(actor);
            for (auto item = target.begin(); item != target.end(); ++item)
            {
                const Ptr ptr = *item;
                if (ptr.mRef->mWorldModel)
                    removedItems.push_back(ptr);
            }
            for (auto item = contents->begin(); item != contents->end(); ++item)
            {
                Ptr ptr = *item;
                ptr.setContainerStore(&target);
                insertedItems.push_back(ptr);
            }
            inventories.push_back({&target, std::move(contents)});
        };
        stageInventory(getPlayerPtr(), std::move(playerInventory));
        for (const auto& binding : preparedReferences)
            if (binding.mReference.getType() == ESM::REC_NPC_4 || binding.mReference.getType() == ESM::REC_CREA4)
                stageInventory(binding.mReference, std::move(actorInventories.at(binding.mState->mKey)));

        const auto runtimeGlobalName = [](std::string_view nativeName) -> std::string_view {
            if (Misc::StringUtils::ciEqual(nativeName, "GameDaysPassed"))
                return Globals::sDaysPassed.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameDay"))
                return Globals::sDay.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameMonth"))
                return Globals::sMonth.getValue();
            if (Misc::StringUtils::ciEqual(nativeName, "GameYear"))
                return Globals::sYear.getValue();
            return nativeName;
        };
        const auto setVariant = [&validateFloatInput](ESM::Variant& target, const ESM4::RuntimeValue& value) {
            if (target.getType() == ESM::VT_String)
            {
                if (const auto* text = std::get_if<std::string>(&value))
                    target.setString(*text);
                else
                    throw std::runtime_error("TES4 runtime-state string global has a numeric value");
            }
            else if (target.getType() == ESM::VT_Float)
            {
                const double number = std::visit(
                    [](const auto& item) -> double {
                        using T = std::decay_t<decltype(item)>;
                        if constexpr (std::is_same_v<T, std::string>)
                            throw std::runtime_error("TES4 runtime-state numeric global has a string value");
                        else
                            return static_cast<double>(item);
                    },
                    value);
                validateFloatInput(number, "global");
                target.setFloat(static_cast<float>(number));
            }
            else
            {
                const std::int64_t number = std::visit(
                    [](const auto& item) -> std::int64_t {
                        using T = std::decay_t<decltype(item)>;
                        if constexpr (std::is_same_v<T, std::string>)
                            throw std::runtime_error("TES4 runtime-state numeric global has a string value");
                        else if constexpr (std::is_same_v<T, double>)
                        {
                            const double integral = std::trunc(item);
                            // INT64_MAX rounds to the excluded upper bound
                            // when represented as double. Check before casting.
                            if (integral < -0x1p63 || integral >= 0x1p63)
                                throw std::runtime_error("TES4 runtime-state global exceeds the integer conversion domain");
                            return static_cast<std::int64_t>(integral);
                        }
                        else
                            return item;
                    },
                    value);
                target.setInteger(static_cast<std::int32_t>(std::clamp<std::int64_t>(number,
                    std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max())));
            }
        };
        std::vector<std::pair<ESM::Variant*, ESM::Variant>> preparedGlobals;
        preparedGlobals.reserve(state.mGlobals.size());
        for (const auto& [key, value] : state.mGlobals)
        {
            const std::optional<ESM::FormId> formId = resolver.toFormId(key);
            if (!formId || !formId->hasContentFile())
                throw std::runtime_error("TES4 runtime-state global cannot be resolved: " + key.serialize());
            const ESM4::GlobalVariable* global
                = mStore.get<ESM4::GlobalVariable>().search(ESM::RefId(*formId));
            if (global == nullptr || global->mEditorId.empty())
                throw std::runtime_error("TES4 runtime-state global is not present: " + key.serialize());
            auto& target = mGlobalVariables[GlobalVariableName(runtimeGlobalName(global->mEditorId))];
            ESM::Variant prepared = target;
            setVariant(prepared, value);
            preparedGlobals.emplace_back(&target, std::move(prepared));
        }
        std::unique_ptr<MWPhysics::PreparedNativeRagdollSnapshotRestore> preparedPhysics;
        if (mPhysics)
        {
            NativePhysicalBindings bound;
            if (preparedCombat)
                bound = resolveNativePhysicalBindings(
                    *mPhysics, *preparedCombat, state, getPlayerPtr(), mResourceSystem->getVFS());
            else if (!mPhysics->actorRagdollOwners().empty())
                throw std::runtime_error("native physical restore requires combat authority");
            // Bare reference relocation/scale changes cannot migrate an already
            // admitted physical graph. Scene unload/readmission owns that path.
            for (const auto& binding : bound.mBindings)
            {
                if (binding.mPtr == getPlayerPtr())
                {
                    const auto* current = binding.mPtr.get<ESM::NPC>()->mBase;
                    const auto* target = mStore.get<ESM::NPC>().find(ESM::RefId::stringRefId("Player"));
                    const auto race = resolver.toFormId(state.mPlayer.mRace);
                    if (binding.mPtr.getCell() != &playerCell
                        || (state.mVersion >= 3 && (!race || ESM::RefId(*race) != current->mRace
                            || current->isMale() == state.mPlayer.mFemale
                            || current->mModel.getNormalized() != target->mModel.getNormalized())))
                        throw std::runtime_error("native physical Player restore requires scene readmission");
                }
                else
                {
                    const auto reference = std::find_if(preparedReferences.begin(), preparedReferences.end(),
                        [&](const auto& item) { return item.mState->mKey == binding.mActor; });
                    if (reference == preparedReferences.end() || reference->mReference != binding.mPtr
                        || reference->mCell != binding.mPtr.getCell() || !reference->mState->mEnabled
                        || reference->mState->mDeleted
                        || (reference->mScale && *reference->mScale != binding.mPtr.getCellRef().getScale()))
                        throw std::runtime_error("native physical actor restore requires scene readmission");
                }
            }
            MWPhysics::NativeRagdollSnapshotGroup physical;
            physical.mTimeCache = state.mNativePhysicalBlendTimeCache;
            for (const auto& binding : bound.mBindings)
                physical.mActors.emplace(binding.mActor, state.mNativeActorRagdolls.at(binding.mActor));
            preparedPhysics = mPhysics->prepareActorRagdollSnapshots(physical, bound.mBindings);
        }
        std::optional<MWMechanics::OblivionCombatService::PreparedRestoredActorState> preparedActorState;
        if (preparedCombat)
        {
            auto residents = mWorldModel.getResidentPtrs();
            std::erase_if(residents, [&](const Ptr& ptr) { return relocatedBases.contains(ptr.mRef); });
            for (const auto& binding : preparedReferences)
                residents.push_back(binding.mReference);
            preparedActorState.emplace(mOblivionCombat->prepareRestoredActorState(
                std::move(*preparedCombat), residents, mPlayer.get()));
        }
        // Prepare one registry replacement after all class/view construction.
        // Incoming item Ptrs already name their final live inventory owner.
        for (const auto& reference : reconstructed)
        {
            if (!reference->isValid())
                throw std::invalid_argument("stale native reference reconstruction");
            insertedItems.push_back(reference->get());
        }
        auto preparedRegistry = mWorldModel.preparePtrReplacement(removedItems, insertedItems, relocatedReferences);
        // Registry preparation assigns IDs to reconstructed references. Their
        // prepared metadata copies must carry those same IDs through the swap.
        for (auto& binding : preparedReferences)
            if (binding.mItemCellRef)
                binding.mItemCellRef->setRefNum(binding.mReference.getCellRef().getRefNum());
        // Character, inventory, registry and native-view preparation completes
        // before any global changes. No acquisition/equipment callbacks run.
        // Preserve FormKey ordering even for editor-ID aliases.
        static_assert(std::is_nothrow_move_assignable_v<ESM::Variant>);
        if (!preparedRegistry.isValid() || !preparedCellMoves.isValid())
            throw std::logic_error("native reference publication preparation is stale");
        if (!preparedCellMoves.commit())
            throw std::logic_error("native cell movement preparation is stale");
        mOblivionRuntimeState = std::move(mPendingOblivionRuntimeState);
        preparedRegistry.commit();
        for (auto& reference : reconstructed)
            reference->commit();
        for (auto& inventory : inventories)
            inventory.mTarget->swapPreparedContents(*inventory.mContents);
        mOblivionDynamicReferenceIdentity.reset();
        mNextOblivionDynamicSerial = state.mNextDynamicSerial;
        for (auto& [target, prepared] : preparedGlobals)
            *target = std::move(prepared);
        mGlobalVariables[Globals::sYear].setInteger(state.mClock.mYear);
        mGlobalVariables[Globals::sMonth].setInteger(state.mClock.mMonth);
        mGlobalVariables[Globals::sDay].setInteger(state.mClock.mDay);
        mGlobalVariables[Globals::sGameHour].setFloat(static_cast<float>(state.mClock.mHour));
        mGlobalVariables[Globals::sTimeScale].setFloat(static_cast<float>(state.mClock.mTimeScale));
        mTimeManager->setup(mGlobalVariables);
        // Typed script globals may intentionally project these floats as integers.
        // Restore the clock owner directly instead of recovering it from that projection.
        mTimeManager->setHour(state.mClock.mHour);
        mTimeManager->updateGlobalFloat(Globals::sTimeScale, static_cast<float>(state.mClock.mTimeScale));
        synchronizeOblivionCalendarGlobals(mGlobalVariables);

        if (preparedPlayerRecord)
        {
            static_assert(std::is_nothrow_copy_assignable_v<ESM::RefId>);
            mPlayer->set(preparedPlayerRecord->commit());
            mPlayer->setBirthSign(preparedBirthSign);
            mPlayer->setOblivionCharacterGenerationFlags(state.mPlayer.mCharacterGenerationFlags);
        }

        mPlayer->setCell(&playerCell);
        const Ptr player = getPlayerPtr();
        player.getRefData().setPosition(state.mPlayer.mPosition);
        MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
        // Native channels are installed by the prepared authority/view commit.
        // Legacy telemetry is not an alternate source for those same values.
        if (!nativePlayerValues)
        {
            stats.setHealth((*preparedLegacyResources)[0]);
            stats.setMagicka((*preparedLegacyResources)[1]);
            stats.setFatigue((*preparedLegacyResources)[2]);
        }
        if (const auto level = state.mPlayer.mActorValues.find("level"); level != state.mPlayer.mActorValues.end())
            stats.setLevel(static_cast<int>(level->second));
        if (!nativePlayerValues)
        {
            for (std::size_t i = 0; i < attributeNames.size(); ++i)
            {
                MWMechanics::AttributeValue value = stats.getAttribute(ESM::Attribute::indexToRefId(i));
                const std::string prefix(attributeNames[i]);
                if (const auto found = state.mPlayer.mActorValues.find(prefix + ".base");
                    found != state.mPlayer.mActorValues.end())
                    value.setBase(static_cast<float>(found->second), true);
                if (const auto found = state.mPlayer.mActorValues.find(prefix + ".modifier");
                    found != state.mPlayer.mActorValues.end())
                    value.setModifier(static_cast<float>(found->second));
                stats.setAttribute(ESM::Attribute::indexToRefId(i), value);
            }
        }
        static const std::array skillIds{ ESM::Skill::Armorer, ESM::Skill::Athletics, ESM::Skill::LongBlade,
            ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand, ESM::Skill::HeavyArmor,
            ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration, ESM::Skill::Destruction,
            ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration, ESM::Skill::Acrobatics,
            ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile, ESM::Skill::Security,
            ESM::Skill::Sneak, ESM::Skill::Speechcraft };
        MWMechanics::NpcStats& npcStats = player.getClass().getNpcStats(player);
        if (const auto breath = state.mPlayer.mActorValues.find("breath_time.current");
            breath != state.mPlayer.mActorValues.end())
            npcStats.setTimeToStartDrowning(static_cast<float>(breath->second));
        if (!nativePlayerValues)
        {
            for (std::size_t i = 0; i < skillIds.size(); ++i)
            {
                MWMechanics::SkillValue& value = npcStats.getSkill(ESM::RefId(skillIds[i]));
                const std::string prefix(skillNames[i]);
                if (const auto found = state.mPlayer.mActorValues.find(prefix + ".base");
                    found != state.mPlayer.mActorValues.end())
                    value.setBase(static_cast<float>(found->second), true);
                if (const auto found = state.mPlayer.mActorValues.find(prefix + ".modifier");
                    found != state.mPlayer.mActorValues.end())
                    value.setModifier(static_cast<float>(found->second));
            }
        }

        for (auto& binding : preparedReferences)
        {
            const auto& reference = *binding.mState;
            Ptr ptr = binding.mReference;
            if (binding.mItemCellRef)
            {
                static_assert(std::is_nothrow_swappable_v<CellRef>);
                std::swap(ptr.getCellRef(), *binding.mItemCellRef);
            }
            ptr.getRefData().setPosition(reference.mPosition);
            reference.mEnabled ? ptr.getRefData().enable() : ptr.getRefData().disable();
            ptr.getCellRef().setOwner(binding.mOwner.value_or(ESM::RefId()));
            try
            {
                ptr.getCellRef().setLockLevel(reference.mLockLevel);
            }
            catch (const std::logic_error&)
            {
                if (reference.mLockLevel != 0)
                    throw;
            }
            if (binding.mLocked)
                ptr.getCellRef().setLocked(*binding.mLocked);
            int count = reference.mDeleted ? 0 : 1;
            if (const auto savedCount = reference.mCustomState.find("count");
                savedCount != reference.mCustomState.end())
            {
                if (const auto* number = std::get_if<std::int64_t>(&savedCount->second))
                    count = static_cast<int>(std::clamp<std::int64_t>(*number,
                        std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
            }
            ptr.getCellRef().setCount(reference.mDeleted ? 0 : count);
            if (binding.mScale)
                ptr.getCellRef().setScale(*binding.mScale);

            if (ptr.getClass().getType() == ESM::REC_NPC_4 || ptr.getClass().getType() == ESM::REC_CREA4)
            {
                restoreOblivionActorDrawState(ptr);
            }

            if (binding.mAnimation)
            {
                static_assert(std::is_nothrow_move_assignable_v<ESM::AnimationState>);
                ptr.getRefData().getAnimationState() = std::move(*binding.mAnimation);
            }
        }
        Log(Debug::Info) << "Applied TES4 runtime state: " << state.mGlobals.size() << " globals, "
                         << state.mReferences.size() << " references, next dynamic serial "
                         << state.mNextDynamicSerial << ", player fatigue=" << stats.getFatigue().getCurrent() << "/"
                         << stats.getFatigue().getBase() << " speed="
                         << stats.getAttribute(ESM::Attribute::Speed).getBase();
        if (preparedScripts)
            preparedScripts->commit();
        if (preparedAi)
            preparedAi->commit();
        if (preparedActorState)
            preparedActorState->commit();
        // All physical data was prepared before World publication. Commit the
        // complete loaded projection and optional shared clock as one group.
        // Retained unowned poses remain in the restored native authority.
        if (preparedPhysics)
            mPhysics->commitActorRagdollSnapshots(*preparedPhysics);
    }

    void World::runOblivionScripts(double secondsPassed)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionScriptManager)
            return;
        mOblivionScriptManager->update(secondsPassed);
        if (mDispatchingOblivionDeathEvents)
            return;
        mDispatchingOblivionDeathEvents = true;
        try
        {
            while (mOblivionCombat && mOblivionScriptManager)
            {
                const auto event = mOblivionCombat->takeNextDeathEvent();
                if (!event)
                    break;
                // Consumption precedes callback execution. Callback-created
                // deaths append behind existing work; callback saves cannot
                // replay this event after a restart.
                const bool handled = mOblivionScriptManager->dispatchObjectEvent(
                    event->mActor, "ondeath", event->mKiller);
                Log(Debug::Info) << "M15 native death callback: id=" << event->mId
                    << " actor=" << event->mActor.serialize() << " killer=" << event->mKiller.serialize()
                    << " handled=" << (handled ? "true" : "false");
            }
        }
        catch (...)
        {
            mDispatchingOblivionDeathEvents = false;
            throw;
        }
        mDispatchingOblivionDeathEvents = false;
    }

    bool World::dispatchOblivionActivation(const Ptr& ptr, const Ptr& actor)
    {
        return mGameProfile == ESM::GameProfile::Oblivion && mOblivionScriptManager
            && mOblivionScriptManager->dispatchActivation(ptr, actor);
    }

    void World::activateOblivionReferenceDefault(const Ptr& ptr, const Ptr& actor)
    {
        const bool previous = mOblivionDefaultActivation;
        mOblivionDefaultActivation = true;
        try
        {
            std::unique_ptr<Action> action = ptr.getClass().activate(ptr, actor);
            mOblivionDefaultActivation = previous;
            if (action)
                action->execute(actor, true);
        }
        catch (...)
        {
            mOblivionDefaultActivation = previous;
            throw;
        }
    }

    void World::write(ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        if (mGameProfile == ESM::GameProfile::Oblivion)
        {
            const ESM4::RuntimeState state = captureOblivionRuntimeState();
            writer.startRecord(ESM4::RuntimeState::sRecordId);
            state.save(writer);
            writer.endRecord(ESM4::RuntimeState::sRecordId);
        }

        writer.startRecord(ESM::REC_RAND);
        writer.writeHNOString("RAND", Misc::Rng::serialize(mPrng));
        writer.endRecord(ESM::REC_RAND);

        // Active cells could have a dirty fog of war, sync it to the CellStore first
        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            MWBase::Environment::get().getWindowManager()->writeFog(cellstore);
        }

        mStore.write(writer, progress); // dynamic Store must be written (and read) before Cells, so that
                                        // references to custom made records will be recognized
        mWorldModel.write(writer, progress); // the player's cell needs to be loaded before the player
        mPlayer->write(writer, progress);
        mGlobalVariables.write(writer, progress);
        mWeatherManager->write(writer, progress);
        mProjectileManager->write(writer, progress);

        writer.startRecord(ESM::REC_ENAB);
        writer.writeHNT("TELE", mTeleportEnabled);
        writer.writeHNT("LEVT", mLevitationEnabled);
        writer.endRecord(ESM::REC_ENAB);

        writer.startRecord(ESM::REC_CAM_);
        writer.writeHNT("FIRS", isFirstPerson());
        writer.endRecord(ESM::REC_CAM_);
    }

    namespace
    {
        void validateOblivionSnapshotContent(const ESM4::RuntimeState& state,
            const std::vector<std::pair<std::string, std::string>>& identities)
        {
            state.validate();
            std::vector<ESM4::RuntimeContentIdentity> content;
            content.reserve(identities.size());
            for (const auto& [plugin, fingerprint] : identities)
                content.push_back({ plugin, fingerprint });
            state.validateContent(content);
        }
    }

    std::unique_ptr<MWBase::World::PreparedOblivionSaveState> World::prepareOblivionSaveState(
        const ESM4::RuntimeState& state, std::unique_ptr<ESMStore> definitions)
    {
        if (mOblivionClearGeneration == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("TES4 restore clear generation exhausted");
        return std::make_unique<PreparedOblivionSaveStateImpl>(*this, state, std::move(definitions));
    }

    void World::validateOblivionSaveState(const ESM4::RuntimeState& state) const
    {
        validateOblivionSaveStateImpl(state, nullptr);
    }

    void World::validateOblivionSaveStateImpl(
        const ESM4::RuntimeState& state, PreparedOblivionServices* prepared, const ESMStore* incoming) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            throw std::runtime_error("TES4 runtime state encountered while the Morrowind profile is active");
        validateOblivionSnapshotContent(state, mOblivionContentIdentities);
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto validateFloat = [](double value, std::string_view name) {
            if (!std::isfinite(value) || value < -std::numeric_limits<float>::max()
                || value > std::numeric_limits<float>::max())
                throw std::runtime_error("TES4 runtime-state " + std::string(name)
                    + " exceeds the finite float domain");
        };
        validateFloat(state.mClock.mTimeScale, "time scale");
        if (state.mVersion >= 3)
        {
            const auto race = resolver.toFormId(state.mPlayer.mRace);
            const auto characterClass = resolver.toFormId(state.mPlayer.mClass);
            if (!race || !mStore.get<ESM::Race>().search(ESM::RefId(*race))
                || (characterClass && !mStore.get<ESM::Class>().search(ESM::RefId(*characterClass)))
                || (!characterClass && !state.mPlayer.mClass.isDynamic()))
                throw std::runtime_error("TES4 runtime-state player race/class cannot be resolved");
            // Dynamic class records are supplied by shared save restoration.
            if (!state.mPlayer.mBirthSign.isNull())
            {
                const auto sign = resolver.toFormId(state.mPlayer.mBirthSign);
                if (!sign || !mStore.get<ESM::BirthSign>().search(ESM::RefId(*sign)))
                    throw std::runtime_error("TES4 runtime-state player birthsign cannot be resolved");
            }
        }
        const auto validateInventory = [&](const auto& items, bool actor, const ESM::FormKey& key) {
            auto migrated = items;
            if (actor && state.mVersion < 4)
                migrateLegacyActorEquipment(migrated, mStore, resolver);
            auto refs = OblivionProfileServices::prepareActorInventory(mStore, resolver, migrated, incoming);
            if (prepared)
            {
                auto contents = OblivionProfileServices::stageActorInventory(refs);
                if (actor)
                    prepared->mActorInventories.emplace(key, std::move(contents));
                else
                    prepared->mPlayerInventory = std::move(contents);
            }
        };
        validateInventory(state.mPlayer.mInventory, false, {});
        const auto validateCell = [&](const ESM::FormKey& key) {
            if (!mStore.get<ESM4::Cell>().search(key))
                throw std::runtime_error("TES4 runtime-state cell is not present: " + key.serialize());
        };
        validateCell(state.mPlayer.mCell);
        if (!state.mNativeCrime.mIncidents.empty() || !state.mNativeCrime.mArrests.empty()
            || !state.mNativeCrime.mJails.empty())
        {
            std::map<ESM::FormKey, const ESM4::RuntimeReferenceState*> references;
            for (const auto& reference : state.mReferences) references.emplace(reference.mKey, &reference);
            const auto playerReference = [&](const ESM::FormKey& key) {
                const auto identity = ESM4::runtimeReferenceKey(key);
                return identity == ESM4::runtimeReferenceKey(state.mPlayer.mReference)
                    || identity == ESM::FormKey::dynamic("player", 1);
            };
            const auto actor = [&](const ESM::FormKey& key) {
                const auto identity = ESM4::runtimeReferenceKey(key);
                if (key.isNull() || playerReference(key))
                    return;
                const auto* character = mStore.search<ESM4::ActorCharacter>(identity);
                const auto* creature = mStore.search<ESM4::ActorCreature>(identity);
                const auto validBase = [&](const ESM::FormKey& base, std::optional<bool> expectedCreature) {
                    const auto* npc = mStore.search<ESM4::Npc>(base);
                    const auto* crea = mStore.search<ESM4::Creature>(base);
                    return (npc != nullptr) != (crea != nullptr)
                        && (!expectedCreature || *expectedCreature == (crea != nullptr))
                        && (!npc || (npc->mIsTES4 && npc->mFormKey == base))
                        && (!crea || (crea->mAttackReach && crea->mFormKey == base));
                };
                if (character || creature)
                {
                    const auto* placed = character ? character : creature;
                    if (!(character && creature) && placed->mFormKey == identity
                        && validBase(placed->mBaseKey, creature != nullptr))
                        return;
                }
                else if (const auto found = references.find(identity); found != references.end()
                    && validBase(found->second->mBase, std::nullopt))
                    return;
                throw std::runtime_error("Native crime actor has no winning actor binding: " + key.serialize());
            };
            const auto reference = [&](const ESM::FormKey& key) {
                if (key.isNull() || playerReference(key)) return;
                if (!references.contains(key) && !mStore.get<ESM4::Reference>().search(key)
                    && !mStore.get<ESM4::ActorCharacter>().search(key) && !mStore.get<ESM4::ActorCreature>().search(key))
                    throw std::runtime_error("Native crime reference has no winning binding: " + key.serialize());
            };
            const auto owner = [&](const ESM::FormKey& key) {
                if (key.isNull()) return;
                const auto id = resolver.toFormId(key);
                if (!id || (!mStore.get<ESM4::Npc>().search(ESM::RefId(*id))
                    && !mStore.get<ESM4::Faction>().search(ESM::RefId(*id))))
                    throw std::runtime_error("Native crime owner has no winning NPC/faction: " + key.serialize());
            };
            for (const auto& incident : state.mNativeCrime.mIncidents)
            {
                actor(incident.mRequest.mPerpetrator);
                actor(incident.mRequest.mVictim);
                reference(incident.mRequest.mAffectedReference);
                validateCell(incident.mRequest.mCell);
                owner(incident.mRequest.mOwnership.mOwner);
                OblivionProfileServices::resolveOwnershipGlobal(mStore, resolver, incident.mRequest.mOwnership.mGlobal);
                for (const auto& witness : incident.mOutcome.mWitnesses) actor(witness.mWitness);
                for (const auto& delta : incident.mOutcome.mFactionDeltas)
                    if (!mStore.get<ESM4::Faction>().search(delta.mFaction))
                        throw std::runtime_error("Native crime faction delta has no winning faction");
            }
            for (const auto& arrest : state.mNativeCrime.mArrests)
            {
                actor(arrest.mActor);
                actor(arrest.mAuthority);
                if (!arrest.mDestination.isNull()) validateCell(arrest.mDestination);
            }
            for (const auto& jail : state.mNativeCrime.mJails)
            {
                actor(jail.mActor);
                for (const auto& key : {jail.mPrison, jail.mEvidence, jail.mBelongings, jail.mRelease}) reference(key);
                for (const auto& item : jail.mProperty)
                {
                    // Instance/owner metadata describes original property; an
                    // instance may no longer exist after a committed transfer.
                    ESM4::RuntimeInventoryItem metadata;
                    metadata.mBase = item.mBase;
                    metadata.mCount = item.mCount;
                    metadata.mCondition = item.mCondition.value_or(-1.f);
                    metadata.mCharge = item.mCharge.value_or(-1.f);
                    (void)OblivionProfileServices::prepareActorInventory(mStore, resolver, {metadata}, incoming);
                    owner(item.mOriginalOwner);
                    OblivionProfileServices::resolveOwnershipGlobal(mStore, resolver, item.mOriginalOwnershipGlobal);
                }
            }
        }
        for (const auto& reference : state.mReferences)
        {
            validateCell(reference.mCell);
            if (reference.mKey.isDynamic() && reference.mKey.mNamespace == "native-reference"
                && reference.mKey.mValue >= state.mNextDynamicSerial)
                throw std::invalid_argument("native reference serial would reuse a saved identity");
            const auto baseId = resolver.toFormId(reference.mBase);
            if (reference.mKey.isDynamic() && reference.mKey.mNamespace == "native-reference")
            {
                if (!baseId)
                    throw std::invalid_argument("native saved reference base cannot be resolved");
                const auto type = savedNativeReferenceType(mStore, ESM::RefId(*baseId));
                if ((type == ESM::REC_NPC_4 || type == ESM::REC_CREA4) && reference.mOwnershipRank)
                    throw std::invalid_argument("Native actor reference cannot own a REFR rank extra");
            }
            const bool actor = baseId && (mStore.get<ESM4::Npc>().search(ESM::RefId(*baseId))
                || mStore.get<ESM4::Creature>().search(ESM::RefId(*baseId)));
            if (actor)
                validateInventory(reference.mInventory, true, reference.mKey);
            else
                for (const auto& item : reference.mInventory)
                {
                    // Retained container contents can still be leveled templates.
                    // Their expansion belongs to the shared inventory authority.
                    if (item.mBase.isDynamic())
                        continue; // Requires incoming shared dynamic records.
                    const auto id = resolver.toFormId(item.mBase);
                    if (!id || (!OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id))
                        && !mStore.get<ESM4::LevelledItem>().search(ESM::RefId(*id))))
                        throw std::runtime_error("TES4 retained inventory item has no winning item/list: "
                            + item.mBase.serialize());
                    OblivionProfileServices::resolveOwnershipGlobal(mStore, resolver, item.mOwnershipGlobal);
                }
            OblivionProfileServices::resolveOwnershipGlobal(mStore, resolver, reference.mOwnershipGlobal);
            if (reference.mOwner)
            {
                const auto owner = resolver.toFormId(*reference.mOwner);
                if (!owner || !owner->hasContentFile())
                    throw std::runtime_error("TES4 runtime-state owner cannot be resolved: "
                        + reference.mOwner->serialize());
            }
            for (const auto name : {"obscript.dead", "locked", "obscript.animation_scripted"})
                if (const auto value = reference.mCustomState.find(name); value != reference.mCustomState.end()
                    && !std::holds_alternative<bool>(value->second))
                    throw std::invalid_argument("TES4 runtime-state " + std::string(name) + " is not boolean");
            if (const auto scale = reference.mCustomState.find("scale"); scale != reference.mCustomState.end())
                if (const auto* value = std::get_if<double>(&scale->second))
                    validateFloat(*value, "reference scale");
            const auto progress = reference.mCustomState.find("obscript.animation_progress");
            if (progress != reference.mCustomState.end() && !std::holds_alternative<double>(progress->second))
                throw std::runtime_error("TES4 runtime-state animation value has the wrong type: "
                    + reference.mKey.serialize());
            const auto group = reference.mCustomState.find("obscript.animation_group");
            const auto scripted = reference.mCustomState.find("obscript.animation_scripted");
            const auto playing = reference.mCustomState.find("obscript.animation_playing");
            const bool hasProgress = progress != reference.mCustomState.end()
                || (playing != reference.mCustomState.end() && std::get_if<bool>(&playing->second)
                    && std::get<bool>(playing->second));
            if (group != reference.mCustomState.end() && hasProgress
                && (scripted == reference.mCustomState.end() || std::get<bool>(scripted->second)))
            {
                const auto* name = std::get_if<std::string>(&group->second);
                if (!name || name->empty())
                    throw std::runtime_error("TES4 runtime-state animation group has the wrong type: "
                        + reference.mKey.serialize());
            }
            if (reference.mItemCondition || reference.mItemCharge)
            {
                const auto item = baseId ? OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*baseId))
                                         : std::nullopt;
                if (!item || (reference.mItemCondition && item->mMaxCondition < 0)
                    || (reference.mItemCharge && item->mMaxCharge < 0.f))
                    throw std::invalid_argument("TES4 loose item extras disagree with the winning item category");
            }
            if (reference.mKey.isDynamic())
                continue; // Dynamic references are reconstructed from shared save records.
            const auto* npc = mStore.get<ESM4::ActorCharacter>().search(reference.mKey);
            const auto* creature = mStore.get<ESM4::ActorCreature>().search(reference.mKey);
            const auto* object = mStore.get<ESM4::Reference>().search(reference.mKey);
            const auto* base = npc ? &npc->mBaseKey : creature ? &creature->mBaseKey
                : object ? &object->mBaseKey : nullptr;
            if (!base)
                throw std::runtime_error("TES4 runtime-state reference is not present: " + reference.mKey.serialize());
            if (*base != reference.mBase)
                throw std::runtime_error("TES4 runtime-state reference base mismatch: " + reference.mKey.serialize());
            if ((npc && !mStore.get<ESM4::Npc>().search(*base))
                || (creature && !mStore.get<ESM4::Creature>().search(*base)))
                throw std::runtime_error("TES4 runtime-state actor base is not present: " + base->serialize());
            if (reference.mActorDrawState && !npc && !creature)
                throw std::runtime_error("TES4 runtime-state actor draw state requires an actor reference");
            if ((npc || creature) && reference.mOwnershipRank)
                throw std::invalid_argument("Native actor reference cannot own a REFR rank extra");
        }
        // Exercise the same detached preparations used by restore. Admission
        // retains these plans; validation-only callers discard them. Neither
        // path publishes services, graphs, registry entries or queued events.
        MWMechanics::OblivionCombatService combat;
        combat.restore(state, mStore);
        combat.validateRestoredPlayerBinding();
        const auto validatePlayerFloat = [&](std::string_view name) {
            if (const auto value = state.mPlayer.mActorValues.find(name); value != state.mPlayer.mActorValues.end())
                validateFloat(value->second, name);
        };
        validatePlayerFloat("breath_time.current");
        if (const auto level = state.mPlayer.mActorValues.find("level"); level != state.mPlayer.mActorValues.end())
            if (std::trunc(level->second) < std::numeric_limits<int>::min()
                || std::trunc(level->second) > std::numeric_limits<int>::max())
                throw std::runtime_error("TES4 runtime-state level exceeds the integer conversion domain");
        if (!combat.findActorValues(ESM::FormKey::dynamic("player", 1)))
        {
            for (const auto name : {"health", "magicka", "fatigue"})
            {
                const std::string prefix(name);
                validatePlayerFloat(prefix + ".base");
                validatePlayerFloat(prefix + ".current");
                validatePlayerFloat(prefix + (state.mPlayer.mActorValues.contains(prefix + ".modifier")
                    ? ".modifier" : ".modified"));
            }
            for (const auto name : {"strength", "intelligence", "willpower", "agility", "speed", "endurance",
                     "personality", "luck", "armorer", "athletics", "blade", "block", "blunt", "handtohand",
                     "heavyarmor", "alchemy", "alteration", "conjuration", "destruction", "illusion", "mysticism",
                     "restoration", "acrobatics", "lightarmor", "marksman", "mercantile", "security", "sneak",
                     "speechcraft"})
                for (const auto suffix : {".base", ".modifier"})
                    validatePlayerFloat(std::string(name) + suffix);
        }
        for (const auto& [key, value] : state.mGlobals)
        {
            const auto id = resolver.toFormId(key);
            if (!id || !mStore.get<ESM4::GlobalVariable>().search(ESM::RefId(*id)))
                throw std::runtime_error("TES4 runtime-state global is not present: " + key.serialize());
            if (std::holds_alternative<std::string>(value))
                throw std::runtime_error("TES4 runtime-state numeric global has a string value");
        }
        if (prepared && mOblivionCombat)
            prepared->mCombat.emplace(std::move(combat));
        if (mOblivionScriptManager)
        {
            auto scripts = mOblivionScriptManager->prepareRestore(state);
            if (prepared)
                prepared->mScripts.emplace(std::move(scripts));
        }
        if (mOblivionAi)
        {
            auto ai = mOblivionAi->prepareRestore(state, prepared != nullptr);
            if (prepared)
                prepared->mAi.emplace(std::move(ai));
        }
    }

    void World::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        if (mSharedDefinitionsPrepared && ESMStore::isSavedDynamicRecord(type))
        {
            reader.skipRecord();
            return;
        }
        switch (type)
        {
            case ESM::REC_ACTC:
                reader.skipRecord();
                return;
            case ESM::REC_ENAB:
                reader.getHNT(mTeleportEnabled, "TELE");
                reader.getHNT(mLevitationEnabled, "LEVT");
                return;
            case ESM::REC_RAND:
            {
                auto data = reader.getHNOString("RAND");
                Misc::Rng::deserialize(data, mPrng);
            }
            break;
            case ESM::REC_T4ST:
            {
                if (mGameProfile != ESM::GameProfile::Oblivion)
                    throw std::runtime_error("TES4 runtime state encountered while the Morrowind profile is active");
                auto state = std::make_unique<ESM4::RuntimeState>();
                state->load(reader);
                // Shared records are still being restored here. Admission already
                // checked immutable bindings; direct readers retain schema/content validation.
                validateOblivionSnapshotContent(*state, mOblivionContentIdentities);
                mPendingOblivionServices.reset();
                mSharedDefinitionsPrepared = false;
                mPendingOblivionRuntimeState = std::move(state);
            }
            break;
            case ESM::REC_PLAY:
                if (reader.getFormatVersion() <= ESM::MaxPlayerBeforeCellDataFormatVersion && !mIdsRebuilt)
                {
                    mStore.rebuildIdsIndex();
                    mIdsRebuilt = true;
                }

                mStore.checkPlayer();
                mPlayer->readRecord(reader, type);
                break;
            case ESM::REC_CSTA:
                // We need to rebuild the ESMStore index in order to be able to lookup dynamic records while loading the
                // WorldModel and, afterwards, the player.
                if (!mIdsRebuilt)
                {
                    mStore.rebuildIdsIndex();
                    mIdsRebuilt = true;
                }
                mWorldModel.readRecord(reader, type);
                break;
            default:
                if (!mStore.readRecord(reader, type) && !mGlobalVariables.readRecord(reader, type)
                    && !mWeatherManager->readRecord(reader, type) && !mProjectileManager->readRecord(reader, type))
                {
                    throw std::runtime_error("unknown record in saved game");
                }
                break;
        }
    }

    void World::ensureNeededRecords()
    {
        for (const auto& [name, value] : generateDefaultGlobals())
        {
            if (mStore.get<ESM::Global>().search(ESM::RefId::stringRefId(name.getValue())) == nullptr)
            {
                ESM::Global record;
                record.mId = ESM::RefId::stringRefId(name.getValue());
                record.mValue = value;
                record.mRecordFlags = 0;
                mStore.insertStatic(record);
            }
        }
    }

    void World::installOblivionProfileServices()
    {
        OblivionProfileServices::install(mStore);
    }

    World::~World()
    {
        // Must be cleared before mRendering is destroyed
        if (mProjectileManager)
            mProjectileManager->clear();

        if (Settings::navigator().mWaitForAllJobsOnExit && mNavigator != nullptr)
        {
            Log(Debug::Verbose) << "Waiting for all navmesh jobs to be done...";
            mNavigator->wait(DetourNavigator::WaitConditionType::allJobsDone, nullptr);
        }
    }

    void World::setRandomSeed(uint32_t seed)
    {
        mRandomSeed = seed;
    }

    void World::useDeathCamera()
    {
        mRendering->getCamera()->setMode(MWRender::Camera::Mode::ThirdPerson);
    }

    MWWorld::Player& World::getPlayer()
    {
        return *mPlayer;
    }

    const std::vector<int>& World::getESMVersions() const
    {
        return mESMVersions;
    }

    LocalScripts& World::getLocalScripts()
    {
        return mLocalScripts;
    }

    void World::setGlobalInt(GlobalVariableName name, int value)
    {
        const GlobalVariableName canonical = mGameProfile == ESM::GameProfile::Oblivion
            ? canonicalOblivionGlobal(name)
            : name;
        mTimeManager->updateGlobalInt(canonical, value);
        mGlobalVariables[canonical].setInteger(value);
        if (mGameProfile == ESM::GameProfile::Oblivion)
            synchronizeOblivionCalendarGlobals(mGlobalVariables);
    }

    void World::setGlobalFloat(GlobalVariableName name, float value)
    {
        const GlobalVariableName canonical = mGameProfile == ESM::GameProfile::Oblivion
            ? canonicalOblivionGlobal(name)
            : name;
        mTimeManager->updateGlobalFloat(canonical, value);
        mGlobalVariables[canonical].setFloat(value);
        if (mGameProfile == ESM::GameProfile::Oblivion)
            synchronizeOblivionCalendarGlobals(mGlobalVariables);
    }

    int World::getGlobalInt(GlobalVariableName name) const
    {
        const GlobalVariableName canonical = mGameProfile == ESM::GameProfile::Oblivion
            ? canonicalOblivionGlobal(name)
            : name;
        return mGlobalVariables[canonical].getInteger();
    }

    float World::getGlobalFloat(GlobalVariableName name) const
    {
        const GlobalVariableName canonical = mGameProfile == ESM::GameProfile::Oblivion
            ? canonicalOblivionGlobal(name)
            : name;
        return mGlobalVariables[canonical].getFloat();
    }

    char World::getGlobalVariableType(GlobalVariableName name) const
    {
        return mGlobalVariables.getType(name);
    }

    std::string_view World::getCellName(const MWWorld::CellStore* cell) const
    {
        if (!cell)
            cell = mWorldScene->getCurrentCell();
        return getCellName(*cell->getCell());
    }

    std::string_view World::getCellName(const MWWorld::Cell& cell) const
    {
        if (!cell.isExterior() || !cell.getDisplayName().empty())
            return cell.getDisplayName();

        if (!cell.getRegion().empty())
        {
            std::string_view regionName
                = ESM::visit(ESM::VisitOverload{
                                 [&](const ESM::Cell& cellIn) -> std::string_view {
                                     if (const ESM::Region* region = mStore.get<ESM::Region>().search(cell.getRegion()))
                                         return !region->mName.empty() ? region->mName : region->mId.getRefIdString();
                                     return {};
                                 },
                                 [&](const ESM4::Cell& cellIn) -> std::string_view { return {}; },
                             },
                    cell);
            if (!regionName.empty())
                return regionName;
        }

        if (!cell.getWorldSpace().empty() && ESM::isEsm4Ext(cell.getWorldSpace()))
        {
            if (const ESM4::World* worldspace = mStore.get<ESM4::World>().search(cell.getWorldSpace()))
                if (!worldspace->mFullName.empty())
                    return worldspace->mFullName;
        }

        return mStore.get<ESM::GameSetting>().find("sDefaultCellname")->mValue.getString();
    }

    void World::removeRefScript(const MWWorld::CellRef* ref)
    {
        mLocalScripts.remove(ref);
    }

    Ptr World::searchPtr(const ESM::RefId& name, bool activeOnly, bool searchInContainers)
    {
        Ptr ret;
        // the player is always in an active cell.
        if (name == "Player")
        {
            return mPlayer->getPlayer();
        }

        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            // TODO: caching still doesn't work efficiently here (only works for the one CellStore that the reference is
            // in)
            Ptr ptr = cellstore->getPtr(name);

            if (!ptr.isEmpty())
                return ptr;
        }

        if (!activeOnly)
        {
            ret = mWorldModel.getPtrByRefId(name);
            if (!ret.isEmpty())
                return ret;
        }

        if (searchInContainers)
        {
            for (CellStore* cellstore : mWorldScene->getActiveCells())
            {
                Ptr ptr = cellstore->searchInContainer(name);
                if (!ptr.isEmpty())
                    return ptr;
            }
        }

        Ptr ptr = mPlayer->getPlayer().getClass().getContainerStore(mPlayer->getPlayer()).search(name);

        return ptr;
    }

    Ptr World::getPtr(const ESM::RefId& name, bool activeOnly)
    {
        Ptr ret = searchPtr(name, activeOnly);
        if (!ret.isEmpty())
            return ret;
        std::string error = "Failed to find an instance of object " + name.toDebugString();
        if (activeOnly)
            error += " in active cells";
        throw std::runtime_error(error);
    }

    struct FindContainerVisitor
    {
        ConstPtr mContainedPtr;
        Ptr mResult;

        FindContainerVisitor(const ConstPtr& containedPtr)
            : mContainedPtr(containedPtr)
        {
        }

        bool operator()(const Ptr& ptr)
        {
            if (mContainedPtr.getContainerStore() == &ptr.getClass().getContainerStore(ptr))
            {
                mResult = ptr;
                return false;
            }

            return true;
        }
    };

    Ptr World::findContainer(const ConstPtr& ptr)
    {
        if (ptr.isInCell())
            return Ptr();

        Ptr player = getPlayerPtr();
        if (ptr.getContainerStore() == &player.getClass().getContainerStore(player))
            return player;

        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            FindContainerVisitor visitor(ptr);
            cellstore->forEachType<ESM::Container>(visitor);
            if (visitor.mResult.isEmpty())
                cellstore->forEachType<ESM::Creature>(visitor);
            if (visitor.mResult.isEmpty())
                cellstore->forEachType<ESM::NPC>(visitor);

            if (!visitor.mResult.isEmpty())
                return visitor.mResult;
        }

        return Ptr();
    }

    void World::addContainerScripts(const Ptr& reference, CellStore* cell)
    {
        if (reference.getType() == ESM::Container::sRecordId || reference.getType() == ESM::NPC::sRecordId
            || reference.getType() == ESM::Creature::sRecordId)
        {
            MWWorld::ContainerStore& container = reference.getClass().getContainerStore(reference);
            for (MWWorld::ContainerStoreIterator it = container.begin(); it != container.end(); ++it)
            {
                const auto& script = it->getClass().getScript(*it);
                if (!script.empty())
                {
                    MWWorld::Ptr item = *it;
                    item.mCell = cell;
                    mLocalScripts.add(script, item);
                }
            }
        }
    }

    void World::enable(const Ptr& reference)
    {
        if (!reference.isInCell())
            return;

        if (!reference.getRefData().isEnabled())
        {
            reference.getRefData().enable();

            if (mWorldScene->getActiveCells().find(reference.getCell()) != mWorldScene->getActiveCells().end()
                && reference.getCellRef().getCount())
                mWorldScene->addObjectToScene(reference);

            if (reference.getCellRef().getRefNum().hasContentFile())
            {
                int type = mStore.find(reference.getCellRef().getRefId());
                if (mRendering->pagingEnableObject(type, reference, true))
                    mWorldScene->reloadTerrain();
            }
        }
    }

    void World::removeContainerScripts(const Ptr& reference)
    {
        if (reference.getType() == ESM::Container::sRecordId || reference.getType() == ESM::NPC::sRecordId
            || reference.getType() == ESM::Creature::sRecordId)
        {
            MWWorld::ContainerStore& container = reference.getClass().getContainerStore(reference);
            for (MWWorld::ContainerStoreIterator it = container.begin(); it != container.end(); ++it)
            {
                const ESM::RefId& script = it->getClass().getScript(*it);
                if (!script.empty())
                {
                    MWWorld::Ptr item = *it;
                    mLocalScripts.remove(item);
                }
            }
        }
    }

    void World::disable(const Ptr& reference)
    {
        if (!reference.getRefData().isEnabled())
            return;

        // disable is a no-op for items in containers
        if (!reference.isInCell())
            return;

        if (reference == getPlayerPtr())
            throw std::runtime_error("can not disable player object");

        reference.getRefData().disable();

        if (reference.getCellRef().getRefNum().hasContentFile())
        {
            int type = mStore.find(reference.getCellRef().getRefId());
            if (mRendering->pagingEnableObject(type, reference, false))
                mWorldScene->reloadTerrain();
        }

        if (mWorldScene->getActiveCells().find(reference.getCell()) != mWorldScene->getActiveCells().end()
            && reference.getCellRef().getCount())
        {
            mWorldScene->removeObjectFromScene(reference);
            mWorldScene->addPostponedPhysicsObjects();
        }
    }

    void World::advanceTime(double hours, bool incremental)
    {
        if (!incremental)
        {
            // When we fast-forward time, we should recharge magic items
            // in all loaded cells, using game world time
            float duration = static_cast<float>(hours * 3600);
            const float timeScaleFactor = mTimeManager->getGameTimeScale();
            if (timeScaleFactor != 0.0f)
                duration /= timeScaleFactor;

            rechargeItems(duration, false);
        }

        mWeatherManager->advanceTime(hours, incremental);
        mTimeManager->advanceTime(hours, mGlobalVariables);
        if (mGameProfile == ESM::GameProfile::Oblivion)
            synchronizeOblivionCalendarGlobals(mGlobalVariables);

        if (!incremental)
        {
            mRendering->notifyWorldSpaceChanged();
            mProjectileManager->clear();
            mDiscardMovements = true;
        }
    }

    TimeStamp World::getTimeStamp() const
    {
        return mTimeManager->getTimeStamp();
    }

    bool World::toggleSky()
    {
        mSky = !mSky;
        mRendering->setSkyEnabled(mSky);
        return mSky;
    }

    int World::getMasserPhase() const
    {
        return mRendering->skyGetMasserPhase();
    }

    int World::getSecundaPhase() const
    {
        return mRendering->skyGetSecundaPhase();
    }

    std::vector<MWWorld::Moon> World::getCurrentMoons() const
    {
        return mWeatherManager->getCurrentMoons(getTimeStamp());
    }

    void World::setMoonColour(bool red)
    {
        mRendering->skySetMoonColour(red);
    }

    void World::changeToInteriorCell(
        const std::string_view cellName, const ESM::Position& position, bool adjustPlayerPos, bool changeEvent)
    {
        mPhysics->clearQueuedMovement();
        mDiscardMovements = true;

        if (changeEvent && mCurrentWorldSpace != cellName)
        {
            // changed worldspace
            mProjectileManager->clear();
            mRendering->notifyWorldSpaceChanged();

            mCurrentWorldSpace = cellName;
        }

        removeContainerScripts(getPlayerPtr());
        mWorldScene->changeToInteriorCell(cellName, position, adjustPlayerPos, changeEvent);
        addContainerScripts(getPlayerPtr(), getPlayerPtr().getCell());
    }

    void World::changeToCell(
        const ESM::RefId& cellId, const ESM::Position& position, bool adjustPlayerPos, bool changeEvent)
    {
        const MWWorld::Cell* destinationCell = getWorldModel().getCell(cellId).getCell();
        bool exteriorCell = destinationCell->isExterior();

        mPhysics->clearQueuedMovement();
        mDiscardMovements = true;

        if (changeEvent && mCurrentWorldSpace != destinationCell->getNameId())
        {
            // changed worldspace
            mProjectileManager->clear();
            mRendering->notifyWorldSpaceChanged();
            mCurrentWorldSpace = destinationCell->getNameId();
        }
        removeContainerScripts(getPlayerPtr());
        if (exteriorCell)
            mWorldScene->changeToExteriorCell(cellId, position, adjustPlayerPos, changeEvent);
        else
            mWorldScene->changeToInteriorCell(destinationCell->getNameId(), position, adjustPlayerPos, changeEvent);
        addContainerScripts(getPlayerPtr(), getPlayerPtr().getCell());
    }

    float World::getMaxActivationDistance() const
    {
        if (mActivationDistanceOverride >= 0)
            return static_cast<float>(mActivationDistanceOverride);

        static const int iMaxActivateDist
            = mStore.get<ESM::GameSetting>().find("iMaxActivateDist")->mValue.getInteger();
        return static_cast<float>(iMaxActivateDist);
    }

    MWWorld::Ptr World::getFocusObject()
    {
        if (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame)
            return {};

        float maxDistance;
        const bool inGui = MWBase::Environment::get().getWindowManager()->isGuiMode();
        if (inGui)
        {
            if (MWBase::Environment::get().getWindowManager()->isConsoleMode())
                return getFocusObject(getMaxActivationDistance() * 50, false);
            static const int iMaxInfoDist = mStore.get<ESM::GameSetting>().find("iMaxInfoDist")->mValue.getInteger();
            maxDistance = static_cast<float>(iMaxInfoDist);
        }
        else
            maxDistance = getMaxActivationDistance();

        const MWWorld::Ptr player = mPlayer->getPlayer();
        const float telekinesisMagnitude = player.getClass()
                                               .getCreatureStats(player)
                                               .getMagicEffects()
                                               .getOrDefault(ESM::MagicEffect::Telekinesis)
                                               .getMagnitude();
        MWWorld::Ptr focusObject = getFocusObject(maxDistance + feetToGameUnits(telekinesisMagnitude), true);

        if (!focusObject.isEmpty() && mDistanceToFocusObject > maxDistance
            && !focusObject.getClass().allowTelekinesis(focusObject) && !inGui)
            return {};
        return focusObject;
    }

    float World::getDistanceToFocusObject()
    {
        return mDistanceToFocusObject;
    }

    osg::Matrixf World::getActorHeadTransform(const MWWorld::ConstPtr& actor) const
    {
        const MWRender::Animation* anim = mRendering->getAnimation(actor);
        if (anim)
        {
            const osg::Node* node = anim->getNode("Head");
            if (!node)
                node = anim->getNode("Bip01 Head");
            if (node)
            {
                osg::NodePathList nodepaths = node->getParentalNodePaths();
                if (!nodepaths.empty())
                    return osg::computeLocalToWorld(nodepaths[0]);
            }
        }
        return osg::Matrixf::translate(actor.getRefData().getPosition().asVec3());
    }

    void World::deleteObject(const Ptr& ptr)
    {
        if (!ptr.mRef->isDeleted() && ptr.getContainerStore() == nullptr)
        {
            if (ptr == getPlayerPtr())
                throw std::runtime_error("can not delete player object");

            ptr.getCellRef().setCount(0);

            if (ptr.isInCell()
                && mWorldScene->getActiveCells().find(ptr.getCell()) != mWorldScene->getActiveCells().end()
                && ptr.getRefData().isEnabled())
            {
                mWorldScene->removeObjectFromScene(ptr);
                mLocalScripts.remove(ptr);
                removeContainerScripts(ptr);
            }
        }
    }

    void World::undeleteObject(const Ptr& ptr)
    {
        if (!ptr.getCellRef().hasContentFile())
            return;
        if (ptr.mRef->isDeleted())
        {
            ptr.getCellRef().setCount(1);
            if (mWorldScene->getActiveCells().find(ptr.getCell()) != mWorldScene->getActiveCells().end()
                && ptr.getRefData().isEnabled())
            {
                mWorldScene->addObjectToScene(ptr);
                const auto& script = ptr.getClass().getScript(ptr);
                if (!script.empty())
                    mLocalScripts.add(script, ptr);
                addContainerScripts(ptr, ptr.getCell());
            }
        }
    }

    MWWorld::Ptr World::moveObject(
        const Ptr& ptr, CellStore* newCell, const osg::Vec3f& position, bool movePhysics, bool keepActive)
    {
        ESM::Position pos = ptr.getRefData().getPosition();
        std::memcpy(pos.pos, &position, sizeof(osg::Vec3f));
        ptr.getRefData().setPosition(pos);

        CellStore* currCell = ptr.isInCell()
            ? ptr.getCell()
            : nullptr; // currCell == nullptr should only happen for player, during initial startup
        bool isPlayer = ptr == mPlayer->getPlayer();
        bool haveToMove = isPlayer || (currCell && mWorldScene->isCellActive(*currCell));
        MWWorld::Ptr newPtr = ptr;

        if (!isPlayer && !currCell)
            throw std::runtime_error("Can not move actor " + ptr.getCellRef().getRefId().toDebugString()
                + " to another cell: current cell is nullptr");

        if (!newCell)
            throw std::runtime_error("Can not move actor " + ptr.getCellRef().getRefId().toDebugString()
                + " to another cell: new cell is nullptr");

        if (currCell != newCell)
        {
            removeContainerScripts(ptr);

            if (isPlayer)
            {
                if (!newCell->isExterior())
                {
                    changeToInteriorCell(newCell->getCell()->getNameId(), pos, false);
                    removeContainerScripts(getPlayerPtr());
                }
                else
                {
                    if (mWorldScene->isCellActive(*newCell))
                        mWorldScene->changePlayerCell(*newCell, pos, false);
                    else
                        mWorldScene->changeToExteriorCell(newCell->getCell()->getId(), pos, false);
                }
                addContainerScripts(getPlayerPtr(), newCell);
                newPtr = getPlayerPtr();
            }
            else
            {
                bool currCellActive = mWorldScene->isCellActive(*currCell);
                bool newCellActive = mWorldScene->isCellActive(*newCell);
                if (!currCellActive && newCellActive)
                {
                    newPtr = currCell->moveTo(ptr, newCell);
                    if (newPtr.getRefData().isEnabled())
                        mWorldScene->addObjectToScene(newPtr);

                    const auto& script = newPtr.getClass().getScript(newPtr);
                    if (!script.empty())
                    {
                        mLocalScripts.add(script, newPtr);
                    }
                    addContainerScripts(newPtr, newCell);
                }
                else if (!newCellActive && currCellActive)
                {
                    mWorldScene->removeObjectFromScene(ptr, keepActive);
                    mLocalScripts.remove(ptr);
                    removeContainerScripts(ptr);
                    haveToMove = false;

                    newPtr = currCell->moveTo(ptr, newCell);
                    newPtr.getRefData().setBaseNode(nullptr);
                }
                else if (!currCellActive && !newCellActive)
                    newPtr = currCell->moveTo(ptr, newCell);
                else // both cells active
                {
                    newPtr = currCell->moveTo(ptr, newCell);

                    mRendering->updatePtr(ptr, newPtr);
                    MWBase::Environment::get().getSoundManager()->updatePtr(ptr, newPtr);
                    mPhysics->updatePtr(ptr, newPtr);

                    MWBase::MechanicsManager* mechMgr = MWBase::Environment::get().getMechanicsManager();
                    mechMgr->updateCell(ptr, newPtr);

                    const auto& script = ptr.getClass().getScript(ptr);
                    if (!script.empty())
                    {
                        mLocalScripts.remove(ptr);
                        removeContainerScripts(ptr);
                        mLocalScripts.add(script, newPtr);
                        addContainerScripts(newPtr, newCell);
                    }
                }
            }

            MWBase::Environment::get().getWindowManager()->updateConsoleObjectPtr(ptr, newPtr);
            MWBase::Environment::get().getScriptManager()->getGlobalScripts().updatePtrs(ptr, newPtr);
        }
        if (haveToMove && newPtr.getRefData().getBaseNode())
        {
            mRendering->moveObject(newPtr, position);
            if (movePhysics)
            {
                mPhysics->updatePosition(newPtr);
                if (const MWPhysics::Object* object = mPhysics->getObject(newPtr))
                    updateNavigatorObject(*object);
            }
        }

        if (isPlayer)
            mWorldScene->playerMoved(position);
        else
        {
            mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
            mWorldScene->removeFromPagedRefs(newPtr);
        }

        return newPtr;
    }

    MWWorld::Ptr World::moveObject(const Ptr& ptr, const osg::Vec3f& position, bool movePhysics, bool moveToActive)
    {
        CellStore* cell = ptr.getCell();
        ESM::RefId worldspaceId
            = cell->isExterior() ? cell->getCell()->getWorldSpace() : ESM::Cell::sDefaultWorldspaceId;
        const ESM::ExteriorCellLocation index
            = ESM::positionToExteriorCellLocation(position.x(), position.y(), worldspaceId);

        CellStore* newCell = cell->isExterior() ? &mWorldModel.getExterior(index) : nullptr;
        bool isCellActive = getPlayerPtr().isInCell() && getPlayerPtr().getCell()->isExterior()
            && (newCell && mWorldScene->isCellActive(*newCell));

        if (cell->isExterior() || (moveToActive && isCellActive && ptr.getClass().isActor()))
            cell = newCell;

        return moveObject(ptr, cell, position, movePhysics);
    }

    MWWorld::Ptr World::moveObjectBy(const Ptr& ptr, const osg::Vec3f& vec, bool moveToActive)
    {
        auto* actor = mPhysics->getActor(ptr);
        osg::Vec3f newpos = ptr.getRefData().getPosition().asVec3() + vec;
        if (actor)
            actor->adjustPosition(vec);
        if (ptr.getClass().isActor())
            return moveObject(ptr, newpos, false, moveToActive && ptr != getPlayerPtr());
        return moveObject(ptr, newpos);
    }

    void World::scaleObject(const Ptr& ptr, float scale, bool force)
    {
        if (!force && scale == ptr.getCellRef().getScale())
            return;
        if (mPhysics->getActor(ptr))
            mNavigator->removeAgent(getPathfindingAgentBounds(ptr));

        ptr.getCellRef().setScale(scale);
        mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
        mWorldScene->removeFromPagedRefs(ptr);

        if (ptr.getRefData().getBaseNode() != nullptr)
            mWorldScene->updateObjectScale(ptr);

        if (mPhysics->getActor(ptr))
        {
            const DetourNavigator::AgentBounds agentBounds = getPathfindingAgentBounds(ptr);
            if (!mNavigator->addAgent(agentBounds))
                Log(Debug::Warning) << "Scaled agent bounds are not supported by navigator: " << agentBounds;
        }
        else if (const auto object = mPhysics->getObject(ptr))
            updateNavigatorObject(*object);
    }

    void World::rotateObject(const Ptr& ptr, const osg::Vec3f& rot, MWBase::RotationFlags flags)
    {
        ESM::Position pos = ptr.getRefData().getPosition();
        float* objRot = pos.rot;
        if (flags & MWBase::RotationFlag_adjust)
        {
            objRot[0] += rot.x();
            objRot[1] += rot.y();
            objRot[2] += rot.z();
        }
        else
        {
            objRot[0] = rot.x();
            objRot[1] = rot.y();
            objRot[2] = rot.z();
        }

        if (ptr.getClass().isActor())
        {
            /* HACK? Actors shouldn't really be rotating around X (or Y), but
             * currently it's done so for rotating the camera, which needs
             * clamping.
             */
            objRot[0] = std::clamp(objRot[0], -osg::PI_2f, osg::PI_2f);
            objRot[1] = static_cast<float>(Misc::normalizeAngle(objRot[1]));
            objRot[2] = static_cast<float>(Misc::normalizeAngle(objRot[2]));
        }

        ptr.getRefData().setPosition(pos);

        mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
        mWorldScene->removeFromPagedRefs(ptr);

        if (ptr.getRefData().getBaseNode() != nullptr)
        {
            const auto order
                = flags & MWBase::RotationFlag_inverseOrder ? RotationOrder::inverse : RotationOrder::direct;
            mWorldScene->updateObjectRotation(ptr, order);

            if (const auto object = mPhysics->getObject(ptr))
                updateNavigatorObject(*object);
        }
    }

    void World::adjustPosition(const Ptr& ptr, bool force)
    {
        if (ptr.isEmpty())
        {
            Log(Debug::Warning) << "Unable to adjust position for empty object";
            return;
        }

        osg::Vec3f pos(ptr.getRefData().getPosition().asVec3());

        if (!ptr.getRefData().getBaseNode())
        {
            // will be adjusted when Ptr's cell becomes active
            return;
        }

        if (!ptr.isInCell())
        {
            Log(Debug::Warning) << "Unable to adjust position for object '" << ptr.getCellRef().getRefId()
                                << "' - it has no cell";
            return;
        }

        const float terrainHeight = ptr.getCell()->isExterior()
            ? getTerrainHeightAt(pos, ptr.getCell()->getCell()->getWorldSpace())
            : -std::numeric_limits<float>::max();
        pos.z() = std::max(pos.z(), terrainHeight)
            + 20; // place slightly above terrain. will snap down to ground with code below

        // We still should trace down dead persistent actors - they do not use the "swimdeath" animation.
        bool swims = ptr.getClass().isActor() && isSwimming(ptr)
            && !(ptr.getClass().isPersistent(ptr) && ptr.getClass().getCreatureStats(ptr).isDeathAnimationFinished());
        if (force || !ptr.getClass().isActor() || (!isFlying(ptr) && !swims && isActorCollisionEnabled(ptr)))
        {
            float height = static_cast<float>(ESM::getCellSize(ptr.getCell()->getCell()->getWorldSpace()));
            osg::Vec3f traced = mPhysics->traceDown(ptr, pos, height);
            pos.z() = std::min(pos.z(), traced.z());
        }

        moveObject(ptr, ptr.getCell(), pos);
    }

    void World::fixPosition()
    {
        const MWWorld::Ptr actor = getPlayerPtr();
        const float distance = 128.f;
        ESM::Position esmPos = actor.getRefData().getPosition();
        osg::Quat orientation(esmPos.rot[2], osg::Vec3f(0, 0, -1));
        osg::Vec3f pos(esmPos.asVec3());

        int direction = 0;
        int fallbackDirections[4] = { direction, (direction + 3) % 4, (direction + 2) % 4, (direction + 1) % 4 };

        osg::Vec3f targetPos = pos;
        for (int i = 0; i < 4; ++i)
        {
            direction = fallbackDirections[i];
            if (direction == 0)
                targetPos = pos + (orientation * osg::Vec3f(0, 1, 0)) * distance;
            else if (direction == 1)
                targetPos = pos - (orientation * osg::Vec3f(0, 1, 0)) * distance;
            else if (direction == 2)
                targetPos = pos - (orientation * osg::Vec3f(1, 0, 0)) * distance;
            else if (direction == 3)
                targetPos = pos + (orientation * osg::Vec3f(1, 0, 0)) * distance;

            // destination is free
            if (!mPhysics->castRay(pos, targetPos, MWPhysics::CollisionType_World | MWPhysics::CollisionType_Door).mHit)
                break;
        }
        targetPos.z() += distance / 2.f; // move up a bit to get out from geometry, will snap down later
        float height = static_cast<float>(ESM::getCellSize(actor.getCell()->getCell()->getWorldSpace()));
        osg::Vec3f traced = mPhysics->traceDown(actor, targetPos, height);
        if (traced != pos)
        {
            esmPos.pos[0] = traced.x();
            esmPos.pos[1] = traced.y();
            esmPos.pos[2] = traced.z();
            ESM::RefId cell = actor.getCell()->getCell()->getId();
            MWWorld::ActionTeleport(cell, esmPos, false).execute(actor);
        }
    }

    void World::rotateWorldObject(const Ptr& ptr, const osg::Quat& rotate)
    {
        if (ptr.getRefData().getBaseNode() != nullptr)
        {
            mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
            mWorldScene->removeFromPagedRefs(ptr);

            mRendering->rotateObject(ptr, rotate);
            mPhysics->updateRotation(ptr, rotate);

            if (const auto object = mPhysics->getObject(ptr))
                updateNavigatorObject(*object);
        }
    }

    MWWorld::Ptr World::placeObject(const MWWorld::ConstPtr& ptr, MWWorld::CellStore* cell, const ESM::Position& pos)
    {
        return copyObjectToCell(ptr, cell, pos, ptr.getCellRef().getCount(), false);
    }

    MWWorld::Ptr World::safePlaceObject(const ConstPtr& ptr, const ConstPtr& referenceObject,
        MWWorld::CellStore* referenceCell, int direction, float distance)
    {
        ESM::Position ipos = referenceObject.getRefData().getPosition();
        osg::Vec3f pos(ipos.asVec3());
        osg::Quat orientation(ipos.rot[2], osg::Vec3f(0, 0, -1));

        int fallbackDirections[4] = { direction, (direction + 3) % 4, (direction + 2) % 4, (direction + 1) % 4 };

        osg::Vec3f spawnPoint = pos;

        for (int i = 0; i < 4; ++i)
        {
            direction = fallbackDirections[i];
            if (direction == 0)
                spawnPoint = pos + (orientation * osg::Vec3f(0, 1, 0)) * distance;
            else if (direction == 1)
                spawnPoint = pos - (orientation * osg::Vec3f(0, 1, 0)) * distance;
            else if (direction == 2)
                spawnPoint = pos - (orientation * osg::Vec3f(1, 0, 0)) * distance;
            else if (direction == 3)
                spawnPoint = pos + (orientation * osg::Vec3f(1, 0, 0)) * distance;

            if (!ptr.getClass().isActor())
                break;

            // check if spawn point is safe, fall back to another direction if not
            spawnPoint.z() += 30; // move up a little to account for slopes, will snap down later

            if (!mPhysics
                     ->castRay(spawnPoint, osg::Vec3f(pos.x(), pos.y(), pos.z() + 20),
                         MWPhysics::CollisionType_World | MWPhysics::CollisionType_Door)
                     .mHit)
            {
                // safe
                break;
            }
        }
        ipos.pos[0] = spawnPoint.x();
        ipos.pos[1] = spawnPoint.y();
        ipos.pos[2] = spawnPoint.z();

        if (referenceObject.getClass().isActor())
        {
            ipos.rot[0] = 0;
            ipos.rot[1] = 0;
        }

        MWWorld::Ptr placed = copyObjectToCell(ptr, referenceCell, ipos, ptr.getCellRef().getCount(), false);
        adjustPosition(placed, true); // snap to ground
        return placed;
    }

    void World::queueMovement(const Ptr& ptr, const osg::Vec3f& velocity)
    {
        mPhysics->queueObjectMovement(ptr, velocity);
        if (ptr == MWMechanics::getPlayer())
            MWBase::Environment::get().getSoundManager()->setListenerVel(velocity);
    }

    void World::updateAnimatedCollisionShape(const Ptr& ptr)
    {
        mPhysics->updateAnimatedCollisionShape(ptr);
    }

    void World::doPhysics(float duration, osg::Timer_t frameStart, unsigned int frameNumber, osg::Stats& stats)
    {
        processDoors(duration);
        mProjectileManager->update(duration);
        mPhysics->stepSimulation(duration, mDiscardMovements, frameStart, frameNumber, stats);
        mProjectileManager->processHits();
        mDiscardMovements = false;
        mPhysics->moveActors();
    }

    void World::updateNavigator()
    {
        auto navigatorUpdateGuard = mNavigator->makeUpdateGuard();

        mPhysics->forEachAnimatedObject([&](const auto& pair) {
            const auto [object, changed] = pair;
            if (changed)
                updateNavigatorObject(*object, navigatorUpdateGuard.get());
        });

        for (const auto& door : mDoorStates)
            if (const auto object = mPhysics->getObject(door.first))
                updateNavigatorObject(*object, navigatorUpdateGuard.get());

        mNavigator->update(getPlayerPtr().getRefData().getPosition().asVec3(), navigatorUpdateGuard.get());
    }

    void World::updateNavigatorObject(
        const MWPhysics::Object& object, const DetourNavigator::UpdateGuard* navigatorUpdateGuard)
    {
        if (object.getShapeInstance()->mVisualCollisionType != Resource::VisualCollisionType::None)
            return;
        const MWWorld::Ptr ptr = object.getPtr();
        const DetourNavigator::ObjectShapes shapes(object.getShapeInstance(),
            DetourNavigator::ObjectTransform{ ptr.getRefData().getPosition(), ptr.getCellRef().getScale() });
        mNavigator->updateObject(
            DetourNavigator::ObjectId(&object), shapes, object.getTransform(), navigatorUpdateGuard);
    }

    const MWPhysics::RayCastingInterface* World::getRayCasting() const
    {
        return mPhysics.get();
    }

    bool World::rotateDoor(const Ptr door, MWWorld::DoorState state, float duration)
    {
        const ESM::Position& objPos = door.getRefData().getPosition();
        auto oldRot = objPos.asRotationVec3();
        auto newRot = oldRot;

        float minRot = door.getCellRef().getPosition().rot[2];
        float maxRot = minRot + osg::DegreesToRadians(90.f);

        float diff = duration * osg::DegreesToRadians(90.f) * (state == MWWorld::DoorState::Opening ? 1 : -1);
        float targetRot = std::clamp(oldRot.z() + diff, minRot, maxRot);
        newRot.z() = targetRot;
        rotateObject(door, newRot, MWBase::RotationFlag_none);

        bool reached = (targetRot == maxRot && state != MWWorld::DoorState::Idle) || targetRot == minRot;

        /// \todo should use convexSweepTest here
        bool collisionWithActor = false;
        for (auto& [ptr, point, normal] :
            mPhysics->getCollisionsPoints(door, MWPhysics::CollisionType_Door, MWPhysics::CollisionType_Actor))
        {

            if (ptr.getClass().isActor())
            {
                auto localPoint = objPos.asVec3() - point;
                osg::Vec3f direction = osg::Quat(diff, osg::Vec3f(0, 0, 1)) * localPoint - localPoint;
                direction.normalize();
                mPhysics->reportCollision(Misc::Convert::toBullet(point), Misc::Convert::toBullet(normal));
                if (direction * normal < 0) // door is turning away from actor
                    continue;

                collisionWithActor = true;

                // Collided with actor, ask actor to try to avoid door
                if (ptr != getPlayerPtr())
                {
                    MWMechanics::AiSequence& seq = ptr.getClass().getCreatureStats(ptr).getAiSequence();
                    if (seq.getTypeId() != MWMechanics::AiPackageTypeId::AvoidDoor) // Only add it once
                        seq.stack(MWMechanics::AiAvoidDoor(door), ptr);
                }

                // we need to undo the rotation
                reached = false;
            }
        }

        // Cancel door closing sound if collision with actor is detected
        if (collisionWithActor)
        {
            // TES4 door sounds are selected by ESM4Door::activate. Avoid the
            // TES3-only record cast when collision reverses a native door.
            const ESM::Door* ref = door.getClass().getType() == ESM::REC_DOOR
                ? door.get<ESM::Door>()->mBase
                : nullptr;

            if (ref && state == MWWorld::DoorState::Opening)
            {
                const ESM::RefId& openSound = ref->mOpenSound;
                if (!openSound.empty()
                    && MWBase::Environment::get().getSoundManager()->getSoundPlaying(door, openSound))
                    MWBase::Environment::get().getSoundManager()->stopSound3D(door, openSound);
            }
            else if (ref && state == MWWorld::DoorState::Closing)
            {
                const ESM::RefId& closeSound = ref->mCloseSound;
                if (!closeSound.empty()
                    && MWBase::Environment::get().getSoundManager()->getSoundPlaying(door, closeSound))
                    MWBase::Environment::get().getSoundManager()->stopSound3D(door, closeSound);
            }

            rotateObject(door, oldRot, MWBase::RotationFlag_none);
        }

        return reached;
    }

    void World::processDoors(float duration)
    {
        auto it = mDoorStates.begin();
        while (it != mDoorStates.end())
        {
            if (!mWorldScene->isCellActive(*it->first.getCell()) || !it->first.getRefData().getBaseNode())
            {
                // The door is no longer in an active cell, or it was disabled.
                // Erase from mDoorStates, since we no longer need to move it.
                // Once we load the door's cell again (or re-enable the door), Door::insertObject will reinsert to
                // mDoorStates.
                mDoorStates.erase(it++);
            }
            else
            {
                bool reached = rotateDoor(it->first, it->second, duration);

                if (reached)
                {
                    // Mark as non-moving
                    it->first.getClass().setDoorState(it->first, MWWorld::DoorState::Idle);
                    mDoorStates.erase(it++);
                }
                else
                    ++it;
            }
        }
    }

    void World::setActorCollisionMode(const MWWorld::Ptr& ptr, bool internal, bool external)
    {
        MWPhysics::Actor* physicActor = mPhysics->getActor(ptr);
        if (physicActor && physicActor->getCollisionMode() != internal)
        {
            physicActor->enableCollisionMode(internal);
            physicActor->enableCollisionBody(external);
        }
    }

    bool World::isActorCollisionEnabled(const MWWorld::Ptr& ptr)
    {
        MWPhysics::Actor* physicActor = mPhysics->getActor(ptr);
        return physicActor && physicActor->getCollisionMode();
    }

    bool World::toggleCollisionMode()
    {
        if (mPhysics->toggleCollisionMode())
        {
            adjustPosition(getPlayerPtr(), true);
            return true;
        }

        return false;
    }

    bool World::toggleRenderMode(MWRender::RenderMode mode)
    {
        switch (mode)
        {
            case MWRender::Render_CollisionDebug:
                return mPhysics->toggleDebugRendering();
            default:
                return mRendering->toggleRenderMode(mode);
        }
    }

    void World::update(float duration, bool paused)
    {
        if (mGoToJail && !paused)
            goToJail();

        // Reset "traveling" flag - there was a frame to detect traveling.
        mPlayerTraveling = false;

        // The same thing for "in jail" flag: reset it if:
        // 1. Player was in jail
        // 2. Jailing window was closed
        if (mPlayerInJail && !mGoToJail && !MWBase::Environment::get().getWindowManager()->containsMode(MWGui::GM_Jail))
            mPlayerInJail = false;

        updateWeather(duration, paused);

        updateNavigator();

        mPlayer->update();

        mPhysics->debugDraw();

        mWorldScene->update(duration);

        mRendering->update(duration, paused);

        updateSoundListener();

        mSpellPreloadTimer -= duration;
        if (mSpellPreloadTimer <= 0.f)
        {
            mSpellPreloadTimer = 0.1f;
            preloadSpells();
        }

        if (mWorldScene->hasCellLoaded())
        {
            mNavigator->wait(DetourNavigator::WaitConditionType::requiredTilesPresent,
                MWBase::Environment::get().getWindowManager()->getLoadingScreen());
            mWorldScene->resetCellLoaded();
        }
    }

    void World::updatePhysics(
        float duration, bool paused, osg::Timer_t frameStart, unsigned int frameNumber, osg::Stats& stats)
    {
        if (!paused)
        {
            doPhysics(duration, frameStart, frameNumber, stats);
        }
        else
        {
            // zero the async stats if we are paused
            stats.setAttribute(frameNumber, "physicsworker_time_begin", 0);
            stats.setAttribute(frameNumber, "physicsworker_time_taken", 0);
            stats.setAttribute(frameNumber, "physicsworker_time_end", 0);
        }
    }

    void World::preloadSpells()
    {
        const ESM::RefId& selectedSpell = MWBase::Environment::get().getWindowManager()->getSelectedSpell();
        if (!selectedSpell.empty())
        {
            const ESM::Spell* spell = mStore.get<ESM::Spell>().search(selectedSpell);
            if (spell)
                preloadEffects(&spell->mEffects);
        }
        const MWWorld::Ptr& selectedEnchantItem
            = MWBase::Environment::get().getWindowManager()->getSelectedEnchantItem();
        if (!selectedEnchantItem.isEmpty())
        {
            const ESM::RefId& enchantId = selectedEnchantItem.getClass().getEnchantment(selectedEnchantItem);
            if (!enchantId.empty())
            {
                const ESM::Enchantment* ench = mStore.get<ESM::Enchantment>().search(enchantId);
                if (ench)
                    preloadEffects(&ench->mEffects);
            }
        }
        const MWWorld::Ptr& selectedWeapon = MWBase::Environment::get().getWindowManager()->getSelectedWeapon();
        if (!selectedWeapon.isEmpty())
        {
            const ESM::RefId& enchantId = selectedWeapon.getClass().getEnchantment(selectedWeapon);
            if (!enchantId.empty())
            {
                const ESM::Enchantment* ench = mStore.get<ESM::Enchantment>().search(enchantId);
                if (ench && ench->mData.mType == ESM::Enchantment::WhenStrikes)
                    preloadEffects(&ench->mEffects);
            }
        }
    }

    void World::updateSoundListener()
    {
        const MWRender::Camera* camera = mRendering->getCamera();
        const auto& player = getPlayerPtr();
        const ESM::Position& refpos = player.getRefData().getPosition();
        osg::Vec3f listenerPos, up, forward;
        osg::Quat listenerOrient;

        if (isFirstPerson() || Settings::sound().mCameraListener)
            listenerPos = camera->getPosition();
        else
            listenerPos = refpos.asVec3() + osg::Vec3f(0, 0, 1.85f * mPhysics->getHalfExtents(player).z());

        if (isFirstPerson() || Settings::sound().mCameraListener)
            listenerOrient = camera->getOrient();
        else
            listenerOrient = osg::Quat(refpos.rot[1], osg::Vec3f(0, -1, 0))
                * osg::Quat(refpos.rot[0], osg::Vec3f(-1, 0, 0)) * osg::Quat(refpos.rot[2], osg::Vec3f(0, 0, -1));

        forward = listenerOrient * osg::Vec3f(0, 1, 0);
        up = listenerOrient * osg::Vec3f(0, 0, 1);

        bool underwater = isUnderwater(player.getCell(), camera->getPosition());

        MWBase::Environment::get().getSoundManager()->setListenerPosDir(listenerPos, forward, up, underwater);
    }

    void World::updateFocusObject()
    {
        try
        {
            // inform the GUI about focused object
            MWWorld::Ptr object = getFocusObject();

            // retrieve the object's top point's screen position so we know where to place the floating label
            if (!object.isEmpty())
            {
                osg::BoundingBox bb = mPhysics->getBoundingBox(object);
                if (!bb.valid() && object.getRefData().getBaseNode())
                {
                    osg::ComputeBoundsVisitor computeBoundsVisitor;
                    computeBoundsVisitor.setTraversalMask(~(MWRender::Mask_ParticleSystem | MWRender::Mask_Effect));
                    object.getRefData().getBaseNode()->accept(computeBoundsVisitor);
                    bb = computeBoundsVisitor.getBoundingBox();
                }
                const osg::Vec2f pos = mRendering->getScreenCoords(bb);
                MWBase::Environment::get().getWindowManager()->setFocusObjectScreenCoords(pos.x(), pos.y());
            }

            MWBase::Environment::get().getWindowManager()->setFocusObject(object);
        }
        catch (std::exception& e)
        {
            Log(Debug::Error) << "Error updating focus object: " << e.what();
        }
    }

    MWWorld::Ptr World::getFocusObject(float maxDistance, bool ignorePlayer)
    {
        const float camDist = mRendering->getCamera()->getCameraDistance();
        maxDistance += camDist;
        MWWorld::Ptr focusObject;
        MWRender::RenderingManager::RayResult rayToObject;

        const bool ignoreTerrain = !Settings::game().mTerrainObstructsFocus;

        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
        {
            float x, y;
            MWBase::Environment::get().getWindowManager()->getMousePosition(x, y);
            rayToObject = mRendering->castCameraToViewportRay(x, y, maxDistance, ignorePlayer, false, ignoreTerrain);
        }
        else
            rayToObject
                = mRendering->castCameraToViewportRay(0.5f, 0.5f, maxDistance, ignorePlayer, false, ignoreTerrain);

        focusObject = rayToObject.mHitObject;
        if (focusObject.isEmpty() && rayToObject.mHitRefnum.isSet())
            focusObject = MWBase::Environment::get().getWorldModel()->getPtr(rayToObject.mHitRefnum);
        bool usedOblivionFocusFallback = false;
        if (mGameProfile == ESM::GameProfile::Oblivion
            && (focusObject.isEmpty() || !focusObject.getClass().hasToolTip(focusObject)))
        {
            const osg::Vec3f cameraPosition = mRendering->getCamera()->getPosition();
            const osg::Vec3f cameraForward
                = mRendering->getCamera()->getOrient() * osg::Vec3f(0.f, 1.f, 0.f);
            float bestScore = 0.9f;
            for (CellStore* cell : mWorldScene->getActiveCells())
            {
                cell->forEach(
                    [&](const Ptr& candidate) {
                        if (!candidate.getRefData().isEnabled() || !candidate.getClass().hasToolTip(candidate))
                            return true;
                        const osg::Vec3f delta
                            = candidate.getRefData().getPosition().asVec3() - cameraPosition;
                        const float distance = delta.length();
                        if (distance <= 0.f || distance > maxDistance)
                            return true;
                        const float score = (delta / distance) * cameraForward;
                        if (score > bestScore)
                        {
                            bestScore = score;
                            mDistanceToFocusObject = distance - camDist;
                            focusObject = candidate;
                            usedOblivionFocusFallback = true;
                        }
                        return true;
                    },
                    false);
            }
        }
        if (rayToObject.mHit && !usedOblivionFocusFallback)
            mDistanceToFocusObject = (rayToObject.mRatio * maxDistance) - camDist;
        else if (!usedOblivionFocusFallback)
            mDistanceToFocusObject = -1;
        return focusObject;
    }

    bool World::castRenderingRay(MWPhysics::RayCastingResult& res, const osg::Vec3f& from, const osg::Vec3f& to,
        bool ignorePlayer, bool ignoreActors, bool ignoreTerrain, std::span<const MWWorld::Ptr> ignoreList)
    {
        MWRender::RenderingManager::RayResult rayRes
            = mRendering->castRay(from, to, ignorePlayer, ignoreActors, ignoreTerrain, ignoreList);
        res.mHit = rayRes.mHit;
        res.mHitPos = rayRes.mHitPointWorld;
        res.mHitNormal = rayRes.mHitNormalWorld;
        res.mHitObject = rayRes.mHitObject;
        if (res.mHitObject.isEmpty() && rayRes.mHitRefnum.isSet())
            res.mHitObject = MWBase::Environment::get().getWorldModel()->getPtr(rayRes.mHitRefnum);
        return res.mHit;
    }

    bool World::isCellExterior() const
    {
        const CellStore* currentCell = mWorldScene->getCurrentCell();
        if (currentCell)
        {
            return currentCell->getCell()->isExterior();
        }
        return false;
    }

    bool World::isCellQuasiExterior() const
    {
        const CellStore* currentCell = mWorldScene->getCurrentCell();
        if (currentCell)
        {
            return currentCell->getCell()->isQuasiExterior();
        }
        return false;
    }

    ESM::RefId World::getCurrentWorldspace() const
    {
        const CellStore* cellStore = mWorldScene->getCurrentCell();
        if (cellStore)
            return cellStore->getCell()->getWorldSpace();
        return ESM::Cell::sDefaultWorldspaceId;
    }

    const std::vector<MWWorld::Weather>& World::getAllWeather() const
    {
        return mWeatherManager->getAllWeather();
    }

    int World::getCurrentWeatherScriptId() const
    {
        return mWeatherManager->getWeather().mScriptId;
    }

    const MWWorld::Weather& World::getCurrentWeather() const
    {
        return mWeatherManager->getWeather();
    }

    const MWWorld::Weather* World::getWeather(size_t index) const
    {
        return mWeatherManager->getWeather(index);
    }

    const MWWorld::Weather* World::getWeather(const ESM::RefId& id) const
    {
        return mWeatherManager->getWeather(id);
    }

    int World::getNextWeatherScriptId() const
    {
        auto next = mWeatherManager->getNextWeather();
        if (next == nullptr)
            return -1;

        return next->mScriptId;
    }

    const MWWorld::Weather* World::getNextWeather() const
    {
        return mWeatherManager->getNextWeather();
    }

    float World::getWeatherTransition() const
    {
        return mWeatherManager->getTransitionFactor();
    }

    unsigned int World::getNightDayMode() const
    {
        return mWeatherManager->getNightDayMode();
    }

    void World::changeWeather(const ESM::RefId& region, const unsigned int id)
    {
        mWeatherManager->changeWeather(region, id);
    }

    void World::changeWeather(const ESM::RefId& region, const ESM::RefId& id)
    {
        mWeatherManager->changeWeather(region, id);
    }

    void World::modRegion(const ESM::RefId& regionid, std::span<const uint8_t> chances)
    {
        mWeatherManager->modRegion(regionid, chances);
    }

    std::span<const uint8_t> World::getRegionWeatherChances(const ESM::RefId& regionid) const
    {
        return mWeatherManager->getRegionChances(regionid);
    }

    struct GetDoorMarkerVisitor
    {
        std::vector<World::DoorMarker>& mOut;

        bool operator()(const MWWorld::Ptr& ptr)
        {
            MWWorld::LiveCellRef<ESM::Door>& ref = *static_cast<MWWorld::LiveCellRef<ESM::Door>*>(ptr.getBase());

            if (!ref.mData.isEnabled() || ref.isDeleted())
                return true;

            if (ref.mRef.getTeleport())
            {
                World::DoorMarker newMarker;
                newMarker.name = MWClass::Door::getDestination(ref);
                newMarker.dest = ref.mRef.getDestCell();

                ESM::Position pos = ref.mData.getPosition();

                newMarker.x = pos.pos[0];
                newMarker.y = pos.pos[1];
                mOut.push_back(std::move(newMarker));
            }
            return true;
        }
    };

    void World::getDoorMarkers(CellStore& cell, std::vector<World::DoorMarker>& out)
    {
        GetDoorMarkerVisitor visitor{ out };
        cell.forEachType<ESM::Door>(visitor);
    }

    void World::setWaterHeight(const float height)
    {
        mPhysics->setWaterHeight(height);
        mRendering->setWaterHeight(height);
    }

    bool World::toggleWater()
    {
        return mRendering->toggleRenderMode(MWRender::Render_Water);
    }

    bool World::toggleWorld()
    {
        return mRendering->toggleRenderMode(MWRender::Render_Scene);
    }

    bool World::toggleBorders()
    {
        return mRendering->toggleBorders();
    }

    void World::PCDropped(const Ptr& item)
    {
        const auto& script = item.getClass().getScript(item);

        // Set OnPCDrop Variable on item's script, if it has a script with that variable declared
        if (!script.empty())
            item.getRefData().getLocals().setVarByInt(script, "onpcdrop", 1);
    }

    MWWorld::Ptr World::placeObject(const MWWorld::Ptr& object, float cursorX, float cursorY, int amount, bool copy)
    {
        const float maxDist = 200.f;

        MWRender::RenderingManager::RayResult result
            = mRendering->castCameraToViewportRay(cursorX, cursorY, maxDist, true, true, false);

        CellStore* cell = getPlayerPtr().getCell();

        ESM::Position pos = getPlayerPtr().getRefData().getPosition();
        if (result.mHit)
        {
            pos.pos[0] = result.mHitPointWorld.x();
            pos.pos[1] = result.mHitPointWorld.y();
            pos.pos[2] = result.mHitPointWorld.z();
        }
        // We want only the Z part of the player's rotation
        pos.rot[0] = 0;
        pos.rot[1] = 0;

        // copy the object and set its count
        Ptr dropped
            = copy ? copyObjectToCell(object, cell, pos, amount, true) : moveObjectToCell(object, cell, pos, true);

        // only the player place items in the world, so no need to check actor
        PCDropped(dropped);

        MWBase::Environment::get().getLuaManager()->objectPlaced(
            dropped, getPlayerPtr(), pos.asVec3(), Misc::Convert::makeOsgQuat(pos.rot));

        return dropped;
    }

    bool World::canPlaceObject(float cursorX, float cursorY)
    {
        const float maxDist = 200.f;
        MWRender::RenderingManager::RayResult result
            = mRendering->castCameraToViewportRay(cursorX, cursorY, maxDist, true, true, false);

        if (result.mHit)
        {
            // check if the wanted position is on a flat surface, and not e.g. against a vertical wall
            if (std::acos((result.mHitNormalWorld / result.mHitNormalWorld.length()) * osg::Vec3f(0, 0, 1))
                >= osg::DegreesToRadians(30.f))
                return false;

            return true;
        }
        else
            return false;
    }

    Ptr World::copyObjectToCell(const ConstPtr& object, CellStore* cell, ESM::Position pos, int count, bool adjustPos)
    {
        if (!cell)
            throw std::runtime_error("copyObjectToCell(): cannot copy object to null cell");
        if (cell->isExterior())
        {
            const ESM::ExteriorCellLocation index
                = ESM::positionToExteriorCellLocation(pos.pos[0], pos.pos[1], cell->getCell()->getWorldSpace());
            cell = &mWorldModel.getExterior(index);
        }

        MWWorld::Ptr dropped = object.getClass().copyToCell(object, *cell, pos, count);

        initObjectInCell(dropped, *cell, adjustPos);

        return dropped;
    }

    Ptr World::moveObjectToCell(const Ptr& object, CellStore* cell, ESM::Position pos, bool adjustPos)
    {
        if (!cell)
            throw std::runtime_error("moveObjectToCell(): cannot move object to null cell");
        if (cell->isExterior())
        {
            const ESM::ExteriorCellLocation index
                = ESM::positionToExteriorCellLocation(pos.pos[0], pos.pos[1], cell->getCell()->getWorldSpace());
            cell = &mWorldModel.getExterior(index);
        }

        MWWorld::Ptr dropped = object.getClass().moveToCell(object, *cell, pos);

        initObjectInCell(dropped, *cell, adjustPos);

        return dropped;
    }

    void World::initObjectInCell(const Ptr& object, CellStore& cell, bool adjustPos)
    {
        if (mWorldScene->isCellActive(cell))
        {
            if (object.getRefData().isEnabled())
            {
                mWorldScene->addObjectToScene(object);
            }
            const auto& script = object.getClass().getScript(object);
            if (!script.empty())
            {
                mLocalScripts.add(script, object);
            }
            addContainerScripts(object, &cell);
        }

        if (!object.getClass().isActor() && adjustPos && object.getRefData().getBaseNode())
        {
            // Adjust position so the location we wanted ends up in the middle of the object bounding box
            osg::ComputeBoundsVisitor computeBounds;
            computeBounds.setTraversalMask(~MWRender::Mask_ParticleSystem);
            object.getRefData().getBaseNode()->accept(computeBounds);
            osg::BoundingBox bounds = computeBounds.getBoundingBox();
            if (bounds.valid())
            {
                ESM::Position pos = object.getRefData().getPosition();
                bounds.set(bounds._min - pos.asVec3(), bounds._max - pos.asVec3());

                osg::Vec3f adjust(
                    (bounds.xMin() + bounds.xMax()) / 2, (bounds.yMin() + bounds.yMax()) / 2, bounds.zMin());
                pos.pos[0] -= adjust.x();
                pos.pos[1] -= adjust.y();
                pos.pos[2] -= adjust.z();
                moveObject(object, pos.asVec3());
            }
        }
    }

    MWWorld::Ptr World::dropObjectOnGround(const Ptr& actor, const Ptr& object, int amount, bool copy)
    {
        MWWorld::CellStore* cell = actor.getCell();

        ESM::Position pos = actor.getRefData().getPosition();
        // We want only the Z part of the actor's rotation
        pos.rot[0] = 0;
        pos.rot[1] = 0;

        osg::Vec3f orig = pos.asVec3();
        orig.z() += 20;
        osg::Vec3f dir(0, 0, -1);

        float len = 1000000.0;

        MWRender::RenderingManager::RayResult result = mRendering->castRay(orig, orig + dir * len, true, true, false);
        if (result.mHit)
            pos.pos[2] = result.mHitPointWorld.z();

        // copy the object and set its count
        Ptr dropped
            = copy ? copyObjectToCell(object, cell, pos, amount, true) : moveObjectToCell(object, cell, pos, true);

        if (actor == mPlayer->getPlayer()) // Only call if dropped by player
            PCDropped(dropped);

        MWBase::Environment::get().getLuaManager()->objectDropped(
            dropped, actor, pos.asVec3(), Misc::Convert::makeOsgQuat(pos.rot));

        return dropped;
    }

    void World::processChangedSettings(const Settings::CategorySettingVector& settings)
    {
        mRendering->processChangedSettings(settings);
    }

    bool World::isFlying(const MWWorld::Ptr& ptr) const
    {
        if (!ptr.getClass().isActor())
            return false;

        const MWMechanics::CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);

        if (stats.isDead())
            return false;

        const bool isPlayer = ptr == getPlayerConstPtr();
        if (!(isPlayer && mGodMode)
            && stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Paralyze).getModifier() > 0)
            return false;

        if (ptr.getClass().canFly(ptr))
            return true;

        if (stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Levitate).getMagnitude() > 0
            && isLevitationEnabled())
            return true;

        const MWPhysics::Actor* actor = mPhysics->getActor(ptr);
        if (!actor)
            return true;

        return false;
    }

    bool World::isSlowFalling(const MWWorld::Ptr& ptr) const
    {
        if (!ptr.getClass().isActor())
            return false;

        const MWMechanics::CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);
        if (stats.getMagicEffects().getOrDefault(ESM::MagicEffect::SlowFall).getMagnitude() > 0)
            return true;

        return false;
    }

    bool World::isSubmerged(const MWWorld::ConstPtr& object) const
    {
        return isUnderwater(object, 1.0f / mSwimHeightScale);
    }

    bool World::isSwimming(const MWWorld::ConstPtr& object) const
    {
        return isUnderwater(object, mSwimHeightScale);
    }

    bool World::isWading(const MWWorld::ConstPtr& object) const
    {
        const float kneeDeep = 0.25f;
        return isUnderwater(object, kneeDeep);
    }

    bool World::isUnderwater(const MWWorld::ConstPtr& object, const float heightRatio) const
    {
        osg::Vec3f pos(object.getRefData().getPosition().asVec3());

        pos.z() += heightRatio * 2 * mPhysics->getRenderingHalfExtents(object).z();

        const CellStore* currCell = object.isInCell()
            ? object.getCell()
            : nullptr; // currCell == nullptr should only happen for player, during initial startup

        return isUnderwater(currCell, pos);
    }

    bool World::isUnderwater(const MWWorld::CellStore* cell, const osg::Vec3f& pos) const
    {
        if (!cell)
            return false;

        if (!(cell->getCell()->hasWater()))
        {
            return false;
        }
        return pos.z() < cell->getWaterLevel();
    }

    bool World::isWaterWalkingCastableOnTarget(const MWWorld::ConstPtr& target) const
    {
        const MWWorld::CellStore* cell = target.getCell();
        if (!cell->getCell()->hasWater())
            return true;

        float waterlevel = cell->getWaterLevel();

        // SwimHeightScale affects the upper z position an actor can swim to
        // while in water. Based on observation from the original engine,
        // the upper z position you get with a +1 SwimHeightScale is the depth
        // limit for being able to cast water walking on an underwater target.
        if (isUnderwater(target, mSwimHeightScale + 1)
            || (isUnderwater(cell, target.getRefData().getPosition().asVec3())
                && !mPhysics->canMoveToWaterSurface(target, waterlevel)))
            return false; // not castable if too deep or if not enough room to move actor to surface
        else
            return true;
    }

    bool World::isOnGround(const MWWorld::Ptr& ptr) const
    {
        return mPhysics->isOnGround(ptr);
    }

    void World::togglePOV(bool force)
    {
        mRendering->getCamera()->toggleViewMode(force);
    }

    bool World::isFirstPerson() const
    {
        return mRendering->getCamera()->getMode() == MWRender::Camera::Mode::FirstPerson;
    }

    bool World::isPreviewModeEnabled() const
    {
        return mRendering->getCamera()->getMode() == MWRender::Camera::Mode::Preview;
    }

    bool World::toggleVanityMode(bool enable)
    {
        return mRendering->getCamera()->toggleVanityMode(enable);
    }

    void World::disableDeferredPreviewRotation()
    {
        mRendering->getCamera()->disableDeferredPreviewRotation();
    }

    void World::applyDeferredPreviewRotationToPlayer(float dt)
    {
        mRendering->getCamera()->applyDeferredPreviewRotationToPlayer(dt);
    }

    MWRender::Camera* World::getCamera()
    {
        return mRendering->getCamera();
    }

    bool World::vanityRotateCamera(const float* rot)
    {
        auto* camera = mRendering->getCamera();
        if (!camera->isVanityOrPreviewModeEnabled())
            return false;

        camera->setPitch(camera->getPitch() + rot[0]);
        camera->setYaw(camera->getYaw() + rot[2]);
        return true;
    }

    void World::saveLoaded(const ESM::ESMReader& reader)
    {
        mSharedDefinitionsPrepared = false;
        mStore.rebuildIdsIndex();
        mStore.validateDynamic();
        mTimeManager->setup(mGlobalVariables);
        mProjectileManager->saveLoaded(reader);
        if (mGameProfile == ESM::GameProfile::Oblivion)
            applyOblivionRuntimeState();
    }

    void World::setupPlayer()
    {
        const ESM::NPC* player = mStore.get<ESM::NPC>().find(ESM::RefId::stringRefId("Player"));
        if (!mPlayer)
            mPlayer = std::make_unique<MWWorld::Player>(player);
        else
        {
            if (mRendering)
            {
                // Remove the old CharacterController and scene attachment.
                MWBase::Environment::get().getMechanicsManager()->remove(getPlayerPtr(), true);
                mNavigator->removeAgent(getPathfindingAgentBounds(getPlayerConstPtr()));
                mPhysics->remove(getPlayerPtr());
                mRendering->removePlayer(getPlayerPtr());
                MWBase::Environment::get().getLuaManager()->objectRemovedFromScene(getPlayerPtr());
            }

            mPlayer->set(player);
        }

        Ptr ptr = mPlayer->getPlayer();
        // Player data also supports construction/load validation before scene
        // initialization. Normal engine startup attaches the renderer here.
        if (mRendering)
            mRendering->setupPlayer(ptr);
        MWBase::Environment::get().getLuaManager()->setupPlayer(ptr);
    }

    void World::renderPlayer()
    {
        MWBase::Environment::get().getMechanicsManager()->remove(getPlayerPtr(), true);

        MWWorld::Ptr player = getPlayerPtr();

        mRendering->renderPlayer(player);
        MWRender::NpcAnimation* anim = static_cast<MWRender::NpcAnimation*>(mRendering->getAnimation(player));
        player.getClass().getInventoryStore(player).setInvListener(anim);
        player.getClass().getInventoryStore(player).setContListener(anim);

        scaleObject(player, player.getCellRef().getScale(), true); // apply race height
        rotateObject(player, osg::Vec3f(), MWBase::RotationFlag_inverseOrder | MWBase::RotationFlag_adjust);

        MWBase::Environment::get().getMechanicsManager()->add(getPlayerPtr());
        MWBase::Environment::get().getWindowManager()->watchActor(getPlayerPtr());

        mPhysics->remove(getPlayerPtr());
        mPhysics->addActor(getPlayerPtr(), getPlayerPtr().getClass().getCorrectedModel(getPlayerPtr()));

        applyLoopingParticles(player);

        const DetourNavigator::AgentBounds agentBounds = getPathfindingAgentBounds(getPlayerConstPtr());
        if (!mNavigator->addAgent(agentBounds))
            Log(Debug::Warning) << "Player agent bounds are not supported by navigator: " << agentBounds;
    }

    int World::canRest() const
    {
        int result = 0;

        CellStore* currentCell = mWorldScene->getCurrentCell();

        Ptr player = mPlayer->getPlayer();

        const MWPhysics::Actor* actor = mPhysics->getActor(player);
        if (!actor)
            throw std::runtime_error("can't find player");

        const osg::Vec3f playerPos(player.getRefData().getPosition().asVec3());
        if (isUnderwater(currentCell, playerPos) || isWalkingOnWater(player))
            result |= Rest_PlayerIsUnderwater;

        float fallHeight = player.getClass().getCreatureStats(player).getFallHeight();
        float epsilon = 1e-4f;
        if ((actor->getCollisionMode() && (!mPhysics->isOnSolidGround(player) || fallHeight >= epsilon))
            || isFlying(player))
            result |= Rest_PlayerIsInAir;

        if (mPlayer->enemiesNearby())
            result |= Rest_EnemiesAreNearby;

        if (!currentCell->getCell()->noSleep() && !player.getClass().getNpcStats(player).isWerewolf())
            result |= Rest_CanSleep;

        return result;
    }

    MWRender::Animation* World::getAnimation(const MWWorld::Ptr& ptr)
    {
        if (!mRendering)
            return nullptr;
        auto* animation = mRendering->getAnimation(ptr);
        if (!animation)
        {
            mWorldScene->removeFromPagedRefs(ptr);
            animation = mRendering->getAnimation(ptr);
            if (animation)
                mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
        }
        return animation;
    }

    const MWRender::Animation* World::getAnimation(const MWWorld::ConstPtr& ptr) const
    {
        return mRendering ? mRendering->getAnimation(ptr) : nullptr;
    }

    void World::screenshot(osg::Image* image, int w, int h)
    {
        mRendering->screenshot(image, w, h);
    }

    void World::activateDoor(const MWWorld::Ptr& door)
    {
        auto state = door.getClass().getDoorState(door);
        switch (state)
        {
            case MWWorld::DoorState::Idle:
                if (door.getRefData().getPosition().rot[2] == door.getCellRef().getPosition().rot[2])
                    state = MWWorld::DoorState::Opening; // if closed, then open
                else
                    state = MWWorld::DoorState::Closing; // if open, then close
                break;
            case MWWorld::DoorState::Closing:
                state = MWWorld::DoorState::Opening; // if closing, then open
                break;
            case MWWorld::DoorState::Opening:
            default:
                state = MWWorld::DoorState::Closing; // if opening, then close
                break;
        }
        door.getClass().setDoorState(door, state);
        mDoorStates[door] = state;
    }

    void World::activateDoor(const Ptr& door, MWWorld::DoorState state)
    {
        door.getClass().setDoorState(door, state);
        mDoorStates[door] = state;
        if (state == MWWorld::DoorState::Idle)
        {
            mDoorStates.erase(door);
            rotateDoor(door, state, 1);
        }
    }

    bool World::getPlayerStandingOn(const MWWorld::ConstPtr& object)
    {
        MWWorld::Ptr player = getPlayerPtr();
        return mPhysics->isActorStandingOn(player, object);
    }

    bool World::getActorStandingOn(const MWWorld::ConstPtr& object)
    {
        std::vector<MWWorld::Ptr> actors;
        mPhysics->getActorsStandingOn(object, actors);
        return !actors.empty();
    }

    void World::getActorsStandingOn(const MWWorld::ConstPtr& object, std::vector<MWWorld::Ptr>& actors)
    {
        mPhysics->getActorsStandingOn(object, actors);
    }

    bool World::getPlayerCollidingWith(const MWWorld::ConstPtr& object)
    {
        return mPhysics->isObjectCollidingWith(object, MWPhysics::ScriptedCollisionType_Player);
    }

    bool World::getActorCollidingWith(const MWWorld::ConstPtr& object)
    {
        return mPhysics->isObjectCollidingWith(object, MWPhysics::ScriptedCollisionType_Actor);
    }

    void World::hurtStandingActors(const ConstPtr& object, float healthPerSecond)
    {
        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
            return;

        std::vector<MWWorld::Ptr> actors;
        mPhysics->getActorsStandingOn(object, actors);
        for (const Ptr& actor : actors)
        {
            MWMechanics::CreatureStats& stats = actor.getClass().getCreatureStats(actor);
            if (stats.isDead())
                continue;

            mPhysics->markAsNonSolid(object);

            if (actor == getPlayerPtr() && mGodMode)
                continue;

            MWMechanics::DynamicStat<float> health = stats.getHealth();
            health.setCurrent(health.getCurrent() - healthPerSecond * MWBase::Environment::get().getFrameDuration());
            stats.setHealth(health);

            if (healthPerSecond > 0.0f)
            {
                if (actor == getPlayerPtr())
                    MWBase::Environment::get().getWindowManager()->activateHitOverlay(false);

                auto healthDamage = ESM::RefId::stringRefId("Health Damage");
                if (!MWBase::Environment::get().getSoundManager()->getSoundPlaying(actor, healthDamage))
                    MWBase::Environment::get().getSoundManager()->playSound3D(actor, healthDamage, 1.0f, 1.0f);
            }
        }
    }

    void World::hurtCollidingActors(const ConstPtr& object, float healthPerSecond)
    {
        if (MWBase::Environment::get().getWindowManager()->isGuiMode())
            return;

        std::vector<Ptr> actors;
        mPhysics->getActorsCollidingWith(object, actors);
        for (const Ptr& actor : actors)
        {
            MWMechanics::CreatureStats& stats = actor.getClass().getCreatureStats(actor);
            if (stats.isDead())
                continue;

            mPhysics->markAsNonSolid(object);

            if (actor == getPlayerPtr() && mGodMode)
                continue;

            MWMechanics::DynamicStat<float> health = stats.getHealth();
            health.setCurrent(health.getCurrent() - healthPerSecond * MWBase::Environment::get().getFrameDuration());
            stats.setHealth(health);

            if (healthPerSecond > 0.0f)
            {
                if (actor == getPlayerPtr())
                    MWBase::Environment::get().getWindowManager()->activateHitOverlay(false);

                auto healthDamage = ESM::RefId::stringRefId("Health Damage");
                if (!MWBase::Environment::get().getSoundManager()->getSoundPlaying(actor, healthDamage))
                    MWBase::Environment::get().getSoundManager()->playSound3D(actor, healthDamage, 1.0f, 1.0f);
            }
        }
    }

    float World::getWindSpeed() const
    {
        if (isCellExterior() || isCellQuasiExterior())
            return mWeatherManager->getWindSpeed();
        else
            return 0.f;
    }

    bool World::isInStorm() const
    {
        if (isCellExterior() || isCellQuasiExterior())
            return mWeatherManager->isInStorm();
        else
            return false;
    }

    osg::Vec3f World::getStormDirection() const
    {
        if (isCellExterior() || isCellQuasiExterior())
            return mWeatherManager->getStormDirection();
        else
            return osg::Vec3f(0, 1, 0);
    }

    struct GetContainersOwnedByVisitor
    {
        GetContainersOwnedByVisitor(const MWWorld::ConstPtr& owner, std::vector<MWWorld::Ptr>& out)
            : mOwner(owner)
            , mOut(out)
        {
        }

        MWWorld::ConstPtr mOwner;
        std::vector<MWWorld::Ptr>& mOut;

        bool operator()(const MWWorld::Ptr& ptr)
        {
            if (ptr.mRef->isDeleted())
                return true;

            // vanilla Morrowind does not allow to sell items from containers with zero capacity
            if (ptr.getClass().getCapacity(ptr) <= 0.f)
                return true;

            if (ptr.getCellRef().getOwner() == mOwner.getCellRef().getRefId())
                mOut.push_back(ptr);

            return true;
        }
    };

    void World::getContainersOwnedBy(const MWWorld::ConstPtr& owner, std::vector<MWWorld::Ptr>& out)
    {
        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            GetContainersOwnedByVisitor visitor(owner, out);
            cellstore->forEachType<ESM::Container>(visitor);
        }
    }

    void World::getItemsOwnedBy(const MWWorld::ConstPtr& npc, std::vector<MWWorld::Ptr>& out)
    {
        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            cellstore->forEach([&](const auto& ptr) {
                if (ptr.getRefData().getBaseNode() && ptr.getCellRef().getOwner() == npc.getCellRef().getRefId())
                    out.push_back(ptr);
                return true;
            });
        }
    }

    bool World::getLOS(const MWWorld::ConstPtr& actor, const MWWorld::ConstPtr& targetActor)
    {
        if (!targetActor.getRefData().isEnabled() || !actor.getRefData().isEnabled())
            return false; // cannot get LOS unless both NPC's are enabled
        if (!targetActor.getRefData().getBaseNode() || !actor.getRefData().getBaseNode())
            return false; // not in active cell

        return mPhysics->getLineOfSight(actor, targetActor);
    }

    float World::getDistToNearestRayHit(const osg::Vec3f& from, const osg::Vec3f& dir, float maxDist, bool includeWater)
    {
        osg::Vec3f to(dir);
        to.normalize();
        to = from + (to * maxDist);

        int collisionTypes
            = MWPhysics::CollisionType_World | MWPhysics::CollisionType_HeightMap | MWPhysics::CollisionType_Door;
        if (includeWater)
        {
            collisionTypes |= MWPhysics::CollisionType_Water;
        }
        MWPhysics::RayCastingResult result
            = mPhysics->castRay(from, to, { MWWorld::Ptr() }, std::vector<MWWorld::Ptr>(), collisionTypes);

        if (!result.mHit)
            return maxDist;
        else
            return (result.mHitPos - from).length();
    }

    void World::enableActorCollision(const MWWorld::Ptr& actor, bool enable)
    {
        MWPhysics::Actor* physicActor = mPhysics->getActor(actor);
        if (physicActor)
            physicActor->enableCollisionBody(enable);
    }

    static std::optional<ESM::Position> searchMarkerPosition(const CellStore& cellStore, std::string_view editorId)
    {
        for (const MWWorld::LiveCellRef<ESM4::Static>& stat4 : cellStore.getReadOnlyEsm4Statics().mList)
        {
            if (Misc::StringUtils::lowerCase(stat4.mBase->mEditorId) == editorId)
                return stat4.mRef.getPosition();
        }
        return std::nullopt;
    }

    static std::optional<ESM::Position> searchPlacedReferencePosition(
        const CellStore& cellStore, std::string_view editorId)
    {
        std::optional<ESM::Position> result;
        cellStore.forEachConst(
            [&](const ConstPtr& ptr) {
                if (Misc::StringUtils::ciEqual(ptr.getCellRef().getEditorId(), editorId))
                {
                    result = ptr.getCellRef().getPosition();
                    return false;
                }
                return true;
            },
            true);
        return result;
    }

    static std::optional<ESM::Position> searchDoorDestInCell(const CellStore& cellStore)
    {
        ESM::RefId cellId = cellStore.getCell()->getId();
        std::vector<const MWWorld::CellRef*> sortedDoors;
        for (const MWWorld::LiveCellRef<ESM::Door>& door : cellStore.getReadOnlyDoors().mList)
        {
            if (!door.mRef.getTeleport())
                continue;
            sortedDoors.push_back(&door.mRef);
        }
        for (const MWWorld::LiveCellRef<ESM4::Door>& door : cellStore.getReadOnlyEsm4Doors().mList)
        {
            if (!door.mRef.getTeleport())
                continue;
            sortedDoors.push_back(&door.mRef);
        }

        // Sort teleporting doors alphabetically, first by ID, then by destination cell to make search consistent
        std::sort(sortedDoors.begin(), sortedDoors.end(), [](const MWWorld::CellRef* lhs, const MWWorld::CellRef* rhs) {
            if (lhs->getRefId() != rhs->getRefId())
                return lhs->getRefId() < rhs->getRefId();

            return lhs->getDestCell() < rhs->getDestCell();
        });

        WorldModel* worldModel = MWBase::Environment::get().getWorldModel();
        for (const MWWorld::CellRef* door : sortedDoors)
        {
            const MWWorld::CellStore& source = worldModel->getCell(door->getDestCell());

            // Find door leading to our current teleport door
            // and use its destination to position inside cell.
            // \note Using _any_ door pointed to the cell,
            // not the one pointed to current door.
            for (const MWWorld::LiveCellRef<ESM::Door>& destDoor : source.getReadOnlyDoors().mList)
            {
                if (cellId == destDoor.mRef.getDestCell())
                {
                    ESM::Position doorDest = destDoor.mRef.getDoorDest();
                    doorDest.rot[0] = doorDest.rot[1] = doorDest.rot[2] = 0;
                    return doorDest;
                }
            }
            for (const MWWorld::LiveCellRef<ESM4::Door>& destDoor : source.getReadOnlyEsm4Doors().mList)
            {
                if (cellId == destDoor.mRef.getDestCell())
                    return destDoor.mRef.getDoorDest();
            }
        }

        return std::nullopt;
    }

    ESM::RefId World::findInteriorPosition(std::string_view name, ESM::Position& pos)
    {
        pos.rot[0] = pos.rot[1] = pos.rot[2] = 0;
        pos.pos[0] = pos.pos[1] = pos.pos[2] = 0;

        const MWWorld::CellStore* cellStore = mWorldModel.findInterior(name);
        if (!cellStore)
            return ESM::RefId();
        ESM::RefId cellId = cellStore->getCell()->getId();

        // The Oblivion tutorial cell has many generic XMarkerHeading records. The
        // authored player marker is the only deterministic, grounded entry point
        // for the first interactive slice; choosing the first generic marker can
        // place the compatibility player in another cell or below geometry.
        if (mGameProfile == ESM::GameProfile::Oblivion && Misc::StringUtils::ciEqual(name, "ImperialDungeon01"))
        {
            if (std::optional<ESM::Position> destPos
                = searchPlacedReferencePosition(*cellStore, "CGPlayerStartMarker"))
            {
                pos = *destPos;
                return cellId;
            }
        }

        if (std::optional<ESM::Position> destPos = searchMarkerPosition(*cellStore, "cocmarkerheading"))
        {
            pos = *destPos;
            return cellId;
        }
        if (std::optional<ESM::Position> destPos = searchDoorDestInCell(*cellStore))
        {
            pos = *destPos;
            return cellId;
        }
        if (std::optional<ESM::Position> destPos = searchMarkerPosition(*cellStore, "xmarkerheading"))
        {
            pos = *destPos;
            return cellId;
        }

        // Fall back to the first static location.
        const MWWorld::CellRefList<ESM4::Static>::List& statics4 = cellStore->getReadOnlyEsm4Statics().mList;
        if (!statics4.empty())
        {
            pos = statics4.begin()->mRef.getPosition();
            pos.rot[0] = pos.rot[1] = pos.rot[2] = 0;
            return cellId;
        }
        const MWWorld::CellRefList<ESM::Static>::List& statics = cellStore->getReadOnlyStatics().mList;
        if (!statics.empty())
        {
            pos = statics.begin()->mRef.getPosition();
            pos.rot[0] = pos.rot[1] = pos.rot[2] = 0;
            return cellId;
        }

        return ESM::RefId();
    }

    ESM::RefId World::findExteriorPosition(std::string_view nameId, ESM::Position& pos)
    {
        pos.rot[0] = pos.rot[1] = pos.rot[2] = 0;

        const MWWorld::CellStore* cellStore = mWorldModel.findCell(nameId);

        if (cellStore != nullptr && !cellStore->isExterior())
            return ESM::RefId();

        if (!cellStore)
        {
            size_t comma = nameId.find(',');
            if (comma != std::string::npos)
            {
                int x, y;
                std::from_chars_result xResult = std::from_chars(nameId.data(), nameId.data() + comma, x);
                std::from_chars_result yResult
                    = std::from_chars(nameId.data() + comma + 1, nameId.data() + nameId.size(), y);
                if (xResult.ec == std::errc::result_out_of_range || yResult.ec == std::errc::result_out_of_range)
                    throw std::runtime_error("Cell coordinates out of range.");
                else if (xResult.ec == std::errc{} && yResult.ec == std::errc{})
                {
                    ESM::RefId worldspace = ESM::Cell::sDefaultWorldspaceId;
                    if (mGameProfile == ESM::GameProfile::Oblivion)
                    {
                        // Oblivion's ordinary exterior grid is the TES4
                        // Tamriel worldspace, not the legacy TES3 default
                        // worldspace used by the coordinate shorthand.
                        // Keep the shorthand stable while selecting the real
                        // ESM4 CELL/PGRD records behind it.
                        if (const std::optional<ESM::FormKey> key = mStore.findEsm4FormKey("Tamriel"))
                        {
                            const ESM::FormKeyResolver resolver(mContentFiles);
                            if (const std::optional<ESM::FormId> id = resolver.toFormId(*key))
                                worldspace = ESM::RefId(*id);
                        }
                    }
                    cellStore = &mWorldModel.getExterior(ESM::ExteriorCellLocation(x, y, worldspace));
                }
                // ignore std::errc::invalid_argument, as this means that name probably refers to a interior cell
                // instead of comma separated coordinates
            }
        }

        if (!cellStore)
            return ESM::RefId();
        const MWWorld::Cell* ext = cellStore->getCell();

        if (std::optional<ESM::Position> destPos = searchMarkerPosition(*cellStore, "cocmarkerheading"))
        {
            pos = *destPos;
            return ext->getId();
        }
        if (std::optional<ESM::Position> destPos = searchMarkerPosition(*cellStore, "xmarkerheading"))
        {
            pos = *destPos;
            return ext->getId();
        }

        int x = ext->getGridX();
        int y = ext->getGridY();
        const osg::Vec2f posFromIndex = indexToPosition(ESM::ExteriorCellLocation(x, y, ext->getWorldSpace()), true);
        pos.pos[0] = posFromIndex.x();
        pos.pos[1] = posFromIndex.y();

        // Note: Z pos will be adjusted by adjustPosition later
        pos.pos[2] = 0;

        return ext->getId();
    }

    void World::enableTeleporting(bool enable)
    {
        mTeleportEnabled = enable;
    }

    bool World::isTeleportingEnabled() const
    {
        return mTeleportEnabled;
    }

    void World::enableLevitation(bool enable)
    {
        mLevitationEnabled = enable;
    }

    bool World::isLevitationEnabled() const
    {
        return mLevitationEnabled;
    }

    void World::reattachPlayerCamera()
    {
        mRendering->rebuildPtr(getPlayerPtr());
    }

    bool World::getGodModeState() const
    {
        return mGodMode;
    }

    bool World::toggleGodMode()
    {
        mGodMode = !mGodMode;

        return mGodMode;
    }

    bool World::toggleScripts()
    {
        mScriptsEnabled = !mScriptsEnabled;
        return mScriptsEnabled;
    }

    bool World::getScriptsEnabled() const
    {
        return mScriptsEnabled;
    }

    void World::loadContentFiles(const Files::Collections& fileCollections, const std::vector<std::string>& content,
        ToUTF8::Utf8Encoder* encoder, Loading::Listener* listener)
    {
        GameContentLoader gameContentLoader;
        EsmLoader esmLoader(mStore, mReaders, encoder, mESMVersions, mRequestedGameProfile);

        gameContentLoader.addLoader(".esm", esmLoader);
        gameContentLoader.addLoader(".esp", esmLoader);
        gameContentLoader.addLoader(".omwgame", esmLoader);
        gameContentLoader.addLoader(".omwaddon", esmLoader);
        gameContentLoader.addLoader(".project", esmLoader);

        OMWScriptsLoader omwScriptsLoader(mStore);
        gameContentLoader.addLoader(".omwscripts", omwScriptsLoader);

        int idx = 0;
        for (const std::string& file : content)
        {
            const Files::MultiDirCollection& col = fileCollections.getCollection(Misc::getFileExtension(file));
            if (col.doesExist(file))
            {
                gameContentLoader.load(col.getPath(file), idx, listener);
            }
            else
            {
                std::string message = "Failed loading " + file + ": the content file does not exist";
                throw std::runtime_error(message);
            }
            idx++;
        }

        mGameProfile = esmLoader.getGameProfile();
        if (mGameProfile == ESM::GameProfile::Auto)
            throw std::runtime_error("Unable to detect a game profile because no ESM content file was loaded");
        Log(Debug::Info) << "Selected game profile: " << ESM::toString(mGameProfile);

        if (const auto v = esmLoader.getMasterFileFormat(); v.has_value() && *v == 0)
            ensureNeededRecords(); // Insert records that may not be present in all versions of master files.
    }

    void World::loadGroundcoverFiles(const Files::Collections& fileCollections,
        const std::vector<std::string>& groundcoverFiles, ToUTF8::Utf8Encoder* encoder, Loading::Listener* listener)
    {
        if (!Settings::groundcover().mEnabled)
            return;

        Log(Debug::Info) << "Loading groundcover:";

        mGroundcoverStore.init(mStore.get<ESM::Static>(), fileCollections, groundcoverFiles, encoder, listener);
    }

    MWWorld::SpellCastState World::startSpellCast(const Ptr& actor)
    {
        if (mGameProfile == ESM::GameProfile::Oblivion)
        {
            Log(Debug::Warning) << "M16 native magic: spell execution is unsupported";
            return MWWorld::SpellCastState::Unsupported;
        }

        MWMechanics::CreatureStats& stats = actor.getClass().getCreatureStats(actor);

        std::string_view message;
        MWWorld::SpellCastState result = MWWorld::SpellCastState::Success;
        bool isPlayer = (actor == getPlayerPtr());

        const ESM::RefId& selectedSpell = stats.getSpells().getSelectedSpell();

        if (!selectedSpell.empty())
        {
            const ESM::Spell* spell = mStore.get<ESM::Spell>().find(selectedSpell);
            int spellCost = MWMechanics::calcSpellCost(*spell);

            // Check mana
            bool godmode = (isPlayer && mGodMode);
            MWMechanics::DynamicStat<float> magicka = stats.getMagicka();
            if (spellCost > 0 && magicka.getCurrent() < spellCost && !godmode)
            {
                message = "#{sMagicInsufficientSP}";
                result = MWWorld::SpellCastState::InsufficientMagicka;
            }

            // If this is a power, check if it was already used in the last 24h
            if (result == MWWorld::SpellCastState::Success && spell->mData.mType == ESM::Spell::ST_Power
                && !stats.getSpells().canUsePower(spell))
            {
                message = "#{sPowerAlreadyUsed}";
                result = MWWorld::SpellCastState::PowerAlreadyUsed;
            }

            if (result == MWWorld::SpellCastState::Success && !godmode)
            {
                // Reduce mana
                magicka.setCurrent(magicka.getCurrent() - spellCost);
                stats.setMagicka(magicka);

                // Reduce fatigue (note that in the vanilla game, both GMSTs are 0, and there's no fatigue loss)
                static const float fFatigueSpellBase
                    = mStore.get<ESM::GameSetting>().find("fFatigueSpellBase")->mValue.getFloat();
                static const float fFatigueSpellMult
                    = mStore.get<ESM::GameSetting>().find("fFatigueSpellMult")->mValue.getFloat();
                MWMechanics::DynamicStat<float> fatigue = stats.getFatigue();
                const float normalizedEncumbrance = actor.getClass().getNormalizedEncumbrance(actor);

                float fatigueLoss = spellCost * (fFatigueSpellBase + normalizedEncumbrance * fFatigueSpellMult);
                fatigue.setCurrent(fatigue.getCurrent() - fatigueLoss);
                stats.setFatigue(fatigue);
            }
        }

        if (isPlayer && result != MWWorld::SpellCastState::Success)
            MWBase::Environment::get().getWindowManager()->messageBox(message);

        return result;
    }

    void World::castSpell(const Ptr& actor, bool scriptedSpell)
    {
        if (mGameProfile == ESM::GameProfile::Oblivion)
        {
            Log(Debug::Warning) << "M16 native magic: spell execution is unsupported";
            return;
        }

        MWMechanics::CreatureStats& stats = actor.getClass().getCreatureStats(actor);

        const bool casterIsPlayer = actor == MWMechanics::getPlayer();
        MWWorld::Ptr target;
        // For scripted spells we should not use hit contact
        if (scriptedSpell)
        {
            if (!casterIsPlayer)
            {
                for (const auto& package : stats.getAiSequence())
                {
                    if (package->getTypeId() == MWMechanics::AiPackageTypeId::Cast)
                    {
                        target = package->getTarget();
                        break;
                    }
                }
            }
        }
        else
        {
            if (casterIsPlayer)
                target = getFocusObject();

            if (target.isEmpty() || !target.getClass().hasToolTip(target))
            {
                // For actor targets, we want to use melee hit contact.
                // This is to give a slight tolerance for errors, especially with creatures like the Skeleton that would
                // be very hard to aim at otherwise.
                // For object targets, we want the detailed shapes (rendering raycast).
                // If we used the bounding boxes for static objects, then we would not be able to target e.g.
                // objects lying on a shelf.
                const float fCombatDistance = mStore.get<ESM::GameSetting>().find("fCombatDistance")->mValue.getFloat();
                target = MWMechanics::getHitContact(actor, fCombatDistance).first;

                if (target.isEmpty())
                {
                    // Get the target using the facing direction from Head node
                    const osg::Vec3f origin = getActorHeadTransform(actor).getTrans();
                    const osg::Quat orient = osg::Quat(actor.getRefData().getPosition().rot[0], osg::Vec3f(-1, 0, 0))
                        * osg::Quat(actor.getRefData().getPosition().rot[2], osg::Vec3f(0, 0, -1));
                    const osg::Vec3f direction = orient * osg::Vec3f(0, 1, 0);
                    const osg::Vec3f dest = origin + direction * getMaxActivationDistance();
                    const MWRender::RenderingManager::RayResult result
                        = mRendering->castRay(origin, dest, true, true, false);
                    if (result.mHit)
                        target = result.mHitObject;
                }
            }
        }

        osg::Vec3f hitPosition = actor.getRefData().getPosition().asVec3();
        if (!target.isEmpty())
        {
            // Touch explosion placement doesn't depend on where the target was "touched".
            // In Morrowind, it's at 0.7 of the actor's AABB height for actors
            // or at 0.7 of the player's height for non-actors if the player is the caster
            // This is probably meant to prevent the explosion from being too far above on large objects
            // but it often puts the explosions way above small objects, so we'll deviate here
            // and use the object's bounds when reasonable (it's $CURRENT_YEAR, we can afford that)
            // Note collision object origin is intentionally not used
            hitPosition = target.getRefData().getPosition().asVec3();
            constexpr float explosionHeight = 0.7f;
            float targetHeight = getHalfExtents(target).z() * 2.f;
            if (!target.getClass().isActor() && casterIsPlayer)
            {
                const float playerHeight = getHalfExtents(actor).z() * 2.f;
                targetHeight = std::min(targetHeight, playerHeight);
            }
            hitPosition.z() += targetHeight * explosionHeight;
        }

        const ESM::RefId& selectedSpell = stats.getSpells().getSelectedSpell();

        MWMechanics::CastSpell cast(actor, target, false, scriptedSpell);
        cast.mHitPosition = hitPosition;

        if (!selectedSpell.empty())
        {
            const ESM::Spell* spell = mStore.get<ESM::Spell>().find(selectedSpell);
            cast.cast(spell);
        }
        else
        {
            MWWorld::ContainerStore& inv = actor.getClass().getContainerStore(actor);
            if (inv.getSelectedEnchantItem() != inv.end())
            {
                const auto& itemPtr = *inv.getSelectedEnchantItem();
                cast.cast(itemPtr);
            }
        }
    }

    void World::launchProjectile(MWWorld::Ptr& actor, MWWorld::Ptr& projectile, const osg::Vec3f& worldPos,
        const osg::Quat& orient, MWWorld::Ptr& bow, float speed, float attackStrength, float attackWindUp)
    {
        // An initial position of projectile can be outside shooter's collision box, so any object between shooter and
        // launch position will be ignored. To avoid this issue, we should check for impact immediately before launch
        // the projectile. So we cast a 1-yard-length ray from shooter to launch position and check if there are
        // collisions in this area.
        // TODO: as a better solutuon we should handle projectiles during physics update, not during world update.
        const osg::Vec3f sourcePos = worldPos + orient * osg::Vec3f(0, -1, 0) * 64.f;

        // For AI actors, get combat targets to use in the ray cast. Only those targets will return a positive hit
        // result.
        std::vector<MWWorld::Ptr> targetActors;
        if (!actor.isEmpty() && actor.getClass().isActor() && actor != MWMechanics::getPlayer())
            actor.getClass().getCreatureStats(actor).getAiSequence().getCombatTargets(targetActors);

        // Check for impact, if yes, handle hit
        MWPhysics::RayCastingResult result = mPhysics->castRay(
            sourcePos, worldPos, { actor }, targetActors, 0xff, MWPhysics::CollisionType_Projectile);

        if (result.mHit)
        {
            MWMechanics::projectileHit(
                actor, result.mHitObject, bow, projectile, result.mHitPos, attackStrength, attackWindUp);
            return;
        }

        // Bail out if the launch position is underwater
        if (isUnderwater(MWMechanics::getPlayer().getCell(), worldPos))
        {
            MWMechanics::projectileHit(actor, Ptr(), bow, projectile, worldPos, attackStrength, attackWindUp);
            mRendering->emitWaterRipple(worldPos);
            return;
        }

        mProjectileManager->launchProjectile(
            actor, projectile, worldPos, orient, bow, speed, attackStrength, attackWindUp);
    }

    void World::launchMagicBolt(
        const ESM::RefId& spellId, const MWWorld::Ptr& caster, const osg::Vec3f& fallbackDirection, ESM::RefNum item)
    {
        mProjectileManager->launchMagicBolt(spellId, caster, fallbackDirection, item);
    }

    void World::updateProjectilesCasters()
    {
        mProjectileManager->updateCasters();
    }

    void World::applyLoopingParticles(const MWWorld::Ptr& ptr) const
    {
        const MWWorld::Class& cls = ptr.getClass();
        if (cls.isActor())
        {
            std::set<ESM::RefId> playing;
            for (const auto& params : cls.getCreatureStats(ptr).getActiveSpells())
            {
                for (const auto& effect : params.getEffects())
                {
                    if (playing.insert(effect.mEffectId).second)
                    {
                        const auto magicEffect = mStore.get<ESM::MagicEffect>().find(effect.mEffectId);
                        if (magicEffect->mData.mFlags & ESM::MagicEffect::ContinuousVfx)
                            MWMechanics::playEffects(ptr, *magicEffect, false);
                    }
                }
            }
        }
    }

    const std::vector<std::string>& World::getContentFiles() const
    {
        return mContentFiles;
    }

    void World::breakInvisibility(const Ptr& actor)
    {
        actor.getClass().getCreatureStats(actor).getActiveSpells().purgeEffect(actor, ESM::MagicEffect::Invisibility);

        // Normally updated once per frame, but here it is kinda important to do it right away.
        MWBase::Environment::get().getMechanicsManager()->updateMagicEffects(actor);
    }

    bool World::useTorches() const
    {
        // If we are in exterior, check the weather manager.
        // In interiors there are no precipitations and sun, so check the ambient
        // Looks like pseudo-exteriors considered as interiors in this case
        MWWorld::CellStore* cell = mPlayer->getPlayer().getCell();
        if (cell->isExterior())
        {
            float hour = getTimeStamp().getHour();
            return mWeatherManager->useTorches(hour);
        }
        else
        {
            const MWWorld::Cell& cellVariant = *cell->getCell();
            uint32_t ambient = cellVariant.getMood().mAmbiantColor;
            int ambientTotal = (ambient & 0xff) + ((ambient >> 8) & 0xff) + ((ambient >> 16) & 0xff);
            return !cell->getCell()->noSleep() && ambientTotal <= 201;
        }
    }

    const osg::Vec4f& World::getSunLightPosition() const
    {
        return mRendering->getSunLightPosition();
    }

    float World::getSunVisibility() const
    {
        return mWeatherManager->getSunVisibility();
    }

    float World::getSunPercentage() const
    {
        return mWeatherManager->getSunPercentage(getTimeStamp().getHour());
    }

    float World::getPhysicsFrameRateDt() const
    {
        return mPhysics->mPhysicsDt;
    }

    bool World::findInteriorPositionInWorldSpace(const MWWorld::CellStore* cell, osg::Vec3f& result)
    {
        if (cell->isExterior())
            return false;

        // Search for a 'nearest' exterior, counting each cell between the starting
        // cell and the exterior as a distance of 1.  Will fail for isolated interiors.
        std::set<ESM::RefId> checkedCells;
        std::set<ESM::RefId> currentCells;
        std::set<ESM::RefId> nextCells;
        nextCells.insert(cell->getCell()->getId());

        while (!nextCells.empty())
        {
            currentCells = nextCells;
            nextCells.clear();
            for (const auto& currentCell : currentCells)
            {
                MWWorld::CellStore& next = mWorldModel.getCell(currentCell);

                // Check if any door in the cell leads to an exterior directly
                for (const MWWorld::LiveCellRef<ESM::Door>& ref : next.getReadOnlyDoors().mList)
                {
                    if (!ref.mRef.getTeleport())
                        continue;

                    if (ref.mRef.getDestCell().is<ESM::ESM3ExteriorCellRefId>())
                    {
                        ESM::Position pos = ref.mRef.getDoorDest();
                        result = pos.asVec3();
                        return true;
                    }
                    else
                    {
                        ESM::RefId dest = ref.mRef.getDestCell();
                        if (!checkedCells.count(dest) && !currentCells.count(dest))
                            nextCells.insert(dest);
                    }
                }

                checkedCells.insert(currentCell);
            }
        }

        // No luck :(
        return false;
    }

    MWWorld::ConstPtr World::getClosestMarker(const MWWorld::ConstPtr& ptr, const ESM::RefId& id)
    {
        if (ptr.getCell()->isExterior())
        {
            return getClosestMarkerFromExteriorPosition(mPlayer->getLastKnownExteriorPosition(), id);
        }

        // Search for a 'nearest' marker, counting each cell between the starting
        // cell and the exterior as a distance of 1.  If an exterior is found, jump
        // to the nearest exterior marker, without further interior searching.
        std::set<ESM::RefId> checkedCells;
        std::set<ESM::RefId> currentCells;
        std::set<ESM::RefId> nextCells;
        MWWorld::ConstPtr closestMarker;

        nextCells.insert(ptr.getCell()->getCell()->getId());
        while (!nextCells.empty())
        {
            currentCells.clear();
            std::swap(currentCells, nextCells);
            for (const auto& cell : currentCells)
            {
                MWWorld::CellStore& next = mWorldModel.getCell(cell);
                checkedCells.insert(cell);

                closestMarker = next.searchConst(id);
                if (!closestMarker.isEmpty())
                {
                    return closestMarker;
                }

                // Check if any door in the cell leads to an exterior directly
                for (const MWWorld::LiveCellRef<ESM::Door>& ref : next.getReadOnlyDoors().mList)
                {
                    if (!ref.mRef.getTeleport())
                        continue;

                    if (ref.mRef.getDestCell().is<ESM::ESM3ExteriorCellRefId>())
                    {
                        osg::Vec3f worldPos = ref.mRef.getDoorDest().asVec3();
                        return getClosestMarkerFromExteriorPosition(worldPos, id);
                    }
                    else
                    {
                        const auto& dest = ref.mRef.getDestCell();
                        if (!checkedCells.contains(dest) && !currentCells.contains(dest))
                            nextCells.insert(dest);
                    }
                }
            }
        }
        return MWWorld::Ptr();
    }

    MWWorld::ConstPtr World::getClosestMarkerFromExteriorPosition(const osg::Vec3f& worldPos, const ESM::RefId& id)
    {
        const ESM::ExteriorCellLocation posIndex = ESM::positionToExteriorCellLocation(worldPos.x(), worldPos.y());

        // Potential optimization: don't scan the entire world for markers and actually do the Todd spiral
        std::vector<Ptr> markers;
        mWorldModel.getExteriorPtrs(id, markers);

        struct MarkerInfo
        {
            Ptr mPtr;
            int mColumn, mRow; // Local coordinates in the valid marker grid
        };
        std::vector<MarkerInfo> validMarkers;
        validMarkers.reserve(markers.size());

        // The idea is to collect all markers that belong to the smallest possible square grid around worldPos
        // They are grouped with their position on that grid's edge where the origin is the SW corner
        int minGridSize = std::numeric_limits<int>::max();
        for (const Ptr& marker : markers)
        {
            const osg::Vec3f markerPos = marker.getRefData().getPosition().asVec3();
            const ESM::ExteriorCellLocation index = ESM::positionToExteriorCellLocation(markerPos.x(), markerPos.y());

            const int deltaX = index.mX - posIndex.mX;
            const int deltaY = index.mY - posIndex.mY;
            const int gridSize = std::max(std::abs(deltaX), std::abs(deltaY)) * 2;
            if (gridSize == 0)
                return marker;

            if (gridSize <= minGridSize)
            {
                if (gridSize < minGridSize)
                {
                    validMarkers.clear();
                    minGridSize = gridSize;
                }
                validMarkers.push_back({ marker, gridSize / 2 + deltaX, gridSize / 2 + deltaY });
            }
        }

        ConstPtr closestMarker;
        if (validMarkers.empty())
            return closestMarker;
        if (validMarkers.size() == 1)
            return validMarkers[0].mPtr;

        // All the markers are on the edge of the grid
        // Break ties by picking the earliest marker on SW -> SE -> NE -> NW -> SW path
        int earliestDistance = std::numeric_limits<int>::max();
        for (const MarkerInfo& marker : validMarkers)
        {
            int distance = 0;
            if (marker.mRow == 0) // South edge (plus SW and SE corners)
                distance = marker.mColumn;
            else if (marker.mColumn == minGridSize) // East edge and NE corner
                distance = minGridSize + marker.mRow;
            else if (marker.mRow == minGridSize) // North edge and NW corner
                distance = minGridSize * 3 - marker.mColumn;
            else // West edge
                distance = minGridSize * 4 - marker.mRow;
            if (distance < earliestDistance)
            {
                closestMarker = marker.mPtr;
                earliestDistance = distance;
            }
        }
        return closestMarker;
    }

    bool World::restOblivionHour(bool sleeping)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            return false;
        if (!mOblivionCombat)
            throw std::logic_error("native rest requires the native actor authority");
        if (isPlayerInJail())
            throw std::logic_error("native jail sentence requires its dedicated transition");
        mOblivionCombat->getPlayerValue(8); // Never silently skip an unregistered Player.
        const float timeScale = mTimeManager->getGameTimeScale();
        if (!std::isfinite(timeScale))
            throw std::invalid_argument("native hourly rest requires a finite time scale");
        // Compute without publishing: resources and every completed actor time
        // must commit together. The native setter resets overflow/nonfinite
        // division results, but retains a finite negative clock.
        const float nextManagerTime = ESM4::actorManagerTimeAfterHour(mOblivionCombat->actorManagerTime(), timeScale);
        const auto frameSettings = resolveOblivionFrameSettings(mStore);
        const MWMechanics::OblivionRestorationSettings settings{
            frameSettings.mMagicka, frameSettings.mFatigue.mRegeneration, frameSettings.mFatigue.mPlayerBase};
        const auto player = getPlayerPtr();
        const bool chargen = getGlobalInt(Globals::sCharGenState) > 0;
        const bool dispatchActors = !chargen && MWBase::Environment::get().getMechanicsManager()->isAIActive();
        const auto playerPosition = player.getRefData().getPosition().asVec3();
        const float range = Settings::game().mActorsProcessingRange;
        std::map<ESM::FormKey, Ptr> residents;
        for (const auto& [refNum, ptr] : mWorldModel.getPtrRegistryView())
            if (!ptr.isEmpty() && ptr != player && mOblivionCombat->findActorValues(ptr.getCellRef().getFormKey()))
                residents.emplace(ptr.getCellRef().getFormKey(), ptr);
        std::map<ESM::FormKey, const ESM4::RuntimeReferenceState*> savedReferences;
        if (mOblivionRuntimeState)
            for (const auto& reference : mOblivionRuntimeState->mReferences)
                savedReferences.emplace(reference.mKey, &reference);

        ESM4::RuntimeState snapshot;
        mOblivionCombat->capture(snapshot);
        std::vector<MWMechanics::OblivionActorRestoration> updates;
        std::vector<Ptr> projections;
        for (const auto& values : snapshot.mNativeActorValues)
        {
            MWMechanics::OblivionActorRestoration update{values.mActor, {}};
            if (values.mOwner == ESM4::ActorValueOwner::Player)
            {
                update.mUpdates.push_back({3600.f, true, false});
                // Ordinary rest restores Player resources even during chargen
                // or with AI disabled, then invokes the common +368 update.
                update.mUpdates.push_back({mOblivionCombat->elapsedSinceActorUpdate(values.mActor, nextManagerTime),
                    sleeping, false});
            }
            else
            {
                if (!dispatchActors)
                    continue;
                bool high = false;
                if (const auto found = residents.find(values.mActor); found != residents.end())
                {
                    const auto& ptr = found->second;
                    if (!ptr.getRefData().isEnabled() || ptr.getCellRef().getCount() <= 0)
                        continue;
                    high = ptr.getRefData().getBaseNode()
                        && (ptr.getRefData().getPosition().asVec3() - playerPosition).length2() <= range * range;
                    projections.push_back(ptr);
                }
                else if (const auto saved = savedReferences.find(values.mActor); saved != savedReferences.end())
                {
                    if (!saved->second->mEnabled || saved->second->mDeleted)
                        continue;
                }
                else
                    throw std::invalid_argument("native rest has no reference state for unloaded actor");
                // The ordinary actor update runs even for distant actors. The
                // High dispatcher additionally restores H/M/F for two seconds.
                // No native casting item exists before M16's effect integration.
                update.mUpdates.push_back({mOblivionCombat->elapsedSinceActorUpdate(values.mActor, nextManagerTime),
                    sleeping, false});
                if (high)
                    update.mUpdates.push_back({2.f, true, false});
            }
            if (!update.mUpdates.empty())
                updates.push_back(std::move(update));
        }
        mOblivionCombat->restoreResourceBatch(*mPlayer, updates, projections, settings, nextManagerTime);
        advanceTime(1);
        if (mOblivionAi && MWBase::Environment::get().getMechanicsManager()->isAIActive())
            mOblivionAi->fastForward(1.f);
        return true;
    }

    void World::rest(double hours)
    {
        mWorldModel.forEachLoadedCellStore([hours](CellStore& store) { store.rest(hours); });
    }

    void World::rechargeItems(float duration, bool activeOnly)
    {
        MWWorld::Ptr player = getPlayerPtr();
        player.getClass().getInventoryStore(player).rechargeItems(duration);

        if (activeOnly)
        {
            for (auto& cell : mWorldScene->getActiveCells())
            {
                cell->recharge(duration);
            }
        }
        else
            mWorldModel.forEachLoadedCellStore([duration](CellStore& store) { store.recharge(duration); });
    }

    void World::teleportToClosestMarker(const MWWorld::Ptr& ptr, const ESM::RefId& id)
    {
        MWWorld::ConstPtr closestMarker = getClosestMarker(ptr, id);

        if (closestMarker.isEmpty())
        {
            Log(Debug::Warning) << "Failed to teleport: no closest marker found";
            return;
        }

        ESM::RefId cellId = closestMarker.mCell->getCell()->getId();

        MWWorld::ActionTeleport action(cellId, closestMarker.getRefData().getPosition(), false);
        action.execute(ptr);
    }

    void World::updateWeather(float duration, bool paused)
    {
        bool isExterior = isCellExterior() || isCellQuasiExterior();
        if (mPlayer->wasTeleported())
        {
            mPlayer->setTeleported(false);

            const ESM::RefId& playerRegion = getPlayerPtr().getCell()->getCell()->getRegion();
            mWeatherManager->playerTeleported(playerRegion, isExterior);
        }

        const TimeStamp time = getTimeStamp();
        mWeatherManager->update(duration, paused, time, isExterior);
    }

    struct AddDetectedReferenceVisitor
    {
        std::vector<Ptr>& mOut;
        Ptr mDetector;
        float mSquaredDist;
        World::DetectionType mType;
        const MWWorld::ESMStore& mStore;

        bool operator()(const MWWorld::Ptr& ptr)
        {
            if ((ptr.getRefData().getPosition().asVec3() - mDetector.getRefData().getPosition().asVec3()).length2()
                >= mSquaredDist)
                return true;

            if (!ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                return true;

            // Consider references inside containers as well (except if we are looking for a Creature, they cannot be in
            // containers)
            bool isContainer = ptr.getClass().getType() == ESM::Container::sRecordId;
            if (mType != World::Detect_Creature && (ptr.getClass().isActor() || isContainer))
            {
                // but ignore containers without resolved content
                if (isContainer && ptr.getRefData().getCustomData() == nullptr)
                {
                    for (const auto& containerItem : ptr.get<ESM::Container>()->mBase->mInventory.mList)
                    {
                        if (containerItem.mCount)
                        {
                            try
                            {
                                ManualRef ref(mStore, containerItem.mItem, containerItem.mCount);
                                if (needToAdd(ref.getPtr(), mDetector))
                                {
                                    mOut.push_back(ptr);
                                    return true;
                                }
                            }
                            catch (const std::exception& e)
                            {
                                Log(Debug::Warning)
                                    << "Failed to process container item " << containerItem.mItem << ": " << e.what();
                            }
                        }
                    }
                    return true;
                }

                MWWorld::ContainerStore& store = ptr.getClass().getContainerStore(ptr);
                {
                    for (MWWorld::ContainerStoreIterator it = store.begin(); it != store.end(); ++it)
                    {
                        if (needToAdd(*it, mDetector))
                        {
                            mOut.push_back(ptr);
                            return true;
                        }
                    }
                }
            }

            if (needToAdd(ptr, mDetector))
                mOut.push_back(ptr);

            return true;
        }

        bool needToAdd(const MWWorld::Ptr& ptr, const MWWorld::Ptr& detector)
        {
            if (mType == World::Detect_Creature)
            {
                // If in werewolf form, this detects only NPCs, otherwise only creatures
                if (detector.getClass().isNpc() && detector.getClass().getNpcStats(detector).isWerewolf())
                {
                    if (ptr.getClass().getType() != ESM::NPC::sRecordId)
                        return false;
                }
                else if (ptr.getClass().getType() != ESM::Creature::sRecordId)
                    return false;

                if (ptr.getClass().getCreatureStats(ptr).isDead())
                    return false;
            }
            if (mType == World::Detect_Key && !ptr.getClass().isKey(ptr))
                return false;
            if (mType == World::Detect_Enchantment && ptr.getClass().getEnchantment(ptr).empty())
                return false;
            return true;
        }
    };

    void World::listDetectedReferences(const Ptr& ptr, std::vector<Ptr>& out, DetectionType type)
    {
        const MWMechanics::MagicEffects& effects = ptr.getClass().getCreatureStats(ptr).getMagicEffects();
        float dist = 0;
        if (type == World::Detect_Creature)
            dist = effects.getOrDefault(ESM::MagicEffect::DetectAnimal).getMagnitude();
        else if (type == World::Detect_Key)
            dist = effects.getOrDefault(ESM::MagicEffect::DetectKey).getMagnitude();
        else if (type == World::Detect_Enchantment)
            dist = effects.getOrDefault(ESM::MagicEffect::DetectEnchantment).getMagnitude();

        if (!dist)
            return;

        dist = feetToGameUnits(dist);

        AddDetectedReferenceVisitor visitor{ out, ptr, dist * dist, type, mStore };

        for (CellStore* cellStore : mWorldScene->getActiveCells())
        {
            cellStore->forEach(visitor);
        }
    }

    float World::feetToGameUnits(float feet)
    {
        // Original engine rounds size upward
        static const float unitsPerFoot = std::ceil(Constants::UnitsPerFoot);
        return feet * unitsPerFoot;
    }

    MWWorld::Ptr World::getPlayerPtr()
    {
        return mPlayer ? mPlayer->getPlayer() : MWWorld::Ptr{};
    }

    MWWorld::ConstPtr World::getPlayerConstPtr() const
    {
        return mPlayer ? mPlayer->getConstPlayer() : MWWorld::ConstPtr{};
    }

    void World::updateDialogueGlobals()
    {
        MWWorld::Ptr player = getPlayerPtr();
        int bounty = player.getClass().getNpcStats(player).getBounty();
        int playerGold = player.getClass().getContainerStore(player).count(ContainerStore::sGoldId);

        static float fCrimeGoldDiscountMult
            = mStore.get<ESM::GameSetting>().find("fCrimeGoldDiscountMult")->mValue.getFloat();
        static float fCrimeGoldTurnInMult
            = mStore.get<ESM::GameSetting>().find("fCrimeGoldTurnInMult")->mValue.getFloat();

        int discount = static_cast<int>(bounty * fCrimeGoldDiscountMult);
        int turnIn = static_cast<int>(bounty * fCrimeGoldTurnInMult);

        if (bounty > 0)
        {
            discount = std::max(1, discount);
            turnIn = std::max(1, turnIn);
        }

        mGlobalVariables[Globals::sPCHasCrimeGold].setInteger((bounty <= playerGold) ? 1 : 0);

        mGlobalVariables[Globals::sPCHasGoldDiscount].setInteger((discount <= playerGold) ? 1 : 0);
        mGlobalVariables[Globals::sCrimeGoldDiscount].setInteger(discount);

        mGlobalVariables[Globals::sCrimeGoldTurnIn].setInteger(turnIn);
        mGlobalVariables[Globals::sPCHasTurnIn].setInteger((turnIn <= playerGold) ? 1 : 0);
    }

    void World::confiscateStolenItems(const Ptr& ptr)
    {
        MWWorld::ConstPtr prisonMarker = getClosestMarker(ptr, ESM::RefId::stringRefId("prisonmarker"));
        if (prisonMarker.isEmpty())
        {
            Log(Debug::Warning) << "Failed to confiscate items: no closest prison marker found.";
            return;
        }
        ESM::RefId prisonName = prisonMarker.getCellRef().getDestCell();
        if (prisonName.empty())
        {
            Log(Debug::Warning) << "Failed to confiscate items: prison marker not linked to prison interior";
            return;
        }
        MWWorld::CellStore& prison = mWorldModel.getCell(prisonName);

        MWWorld::Ptr closestChest = prison.search(ESM::RefId::stringRefId("stolen_goods"));
        if (!closestChest.isEmpty()) // Found a close chest
        {
            MWBase::Environment::get().getMechanicsManager()->confiscateStolenItems(ptr, closestChest);
        }
        else
            Log(Debug::Warning) << "Failed to confiscate items: no stolen_goods container found";
    }

    void World::goToJail()
    {
        const MWWorld::Ptr player = getPlayerPtr();
        if (!mGoToJail)
        {
            // Reset bounty and forget the crime now, but don't change cell yet (the player should be able to read the
            // dialog text first)
            mGoToJail = true;
            mPlayerInJail = true;

            int bounty = player.getClass().getNpcStats(player).getBounty();
            player.getClass().getNpcStats(player).setBounty(0);
            mPlayer->recordCrimeId();
            confiscateStolenItems(player);

            static int iDaysinPrisonMod = mStore.get<ESM::GameSetting>().find("iDaysinPrisonMod")->mValue.getInteger();
            mDaysInPrison = std::max(1, bounty / iDaysinPrisonMod);

            return;
        }
        else
        {
            if (MWBase::Environment::get().getMechanicsManager()->isAttackPreparing(player))
            {
                player.getClass().getCreatureStats(player).setAttackingOrSpell(false);
            }

            mPlayer->setDrawState(MWMechanics::DrawState::Nothing);
            mGoToJail = false;

            MWBase::Environment::get().getWindowManager()->removeGuiMode(MWGui::GM_Dialogue);

            MWBase::Environment::get().getWindowManager()->goToJail(mDaysInPrison);
        }
    }

    bool World::isPlayerInJail() const
    {
        if (mGameProfile == ESM::GameProfile::Oblivion && mOblivionCombat)
            return std::any_of(mOblivionCombat->crimeContracts().mJails.begin(),
                mOblivionCombat->crimeContracts().mJails.end(), [&](const auto& jail) {
                    return jail.mPhase == ESM4::JailPhase::Serving
                        && (ESM4::runtimeReferenceKey(jail.mActor) == ESM::FormKey::dynamic("player", 1)
                            || (mOblivionRuntimeState && ESM4::runtimeReferenceKey(jail.mActor)
                                == ESM4::runtimeReferenceKey(mOblivionRuntimeState->mPlayer.mReference)));
                });
        return mPlayerInJail;
    }

    void World::setPlayerTraveling(bool traveling)
    {
        mPlayerTraveling = traveling;
    }

    bool World::isPlayerTraveling() const
    {
        return mPlayerTraveling;
    }

    float World::getTerrainHeightAt(const osg::Vec3f& worldPos, ESM::RefId worldspace) const
    {
        return mRendering->getTerrainHeightAt(worldPos, worldspace);
    }

    osg::Vec3f World::getHalfExtents(const ConstPtr& object, bool rendering) const
    {
        if (!object.getClass().isActor())
            return mRendering->getHalfExtents(object);

        // Handle actors separately because of bodyparts
        if (rendering)
            return mPhysics->getRenderingHalfExtents(object);
        else
            return mPhysics->getHalfExtents(object);
    }

    std::filesystem::path World::exportSceneGraph(const Ptr& ptr)
    {
        auto file = mUserDataPath / "openmw.osgt";
        if (!ptr.isEmpty())
        {
            mRendering->pagingBlacklistObject(mStore.find(ptr.getCellRef().getRefId()), ptr);
            mWorldScene->removeFromPagedRefs(ptr);
        }
        mRendering->exportSceneGraph(ptr, file, "Ascii");
        return file;
    }

    void World::spawnRandomCreature(const ESM::RefId& creatureList)
    {
        const ESM::CreatureLevList* list = mStore.get<ESM::CreatureLevList>().find(creatureList);

        static int iNumberCreatures = mStore.get<ESM::GameSetting>().find("iNumberCreatures")->mValue.getInteger();
        int numCreatures = 1 + Misc::Rng::rollDice(iNumberCreatures, mPrng); // [1, iNumberCreatures]

        for (int i = 0; i < numCreatures; ++i)
        {
            const ESM::RefId& selectedCreature = MWMechanics::getLevelledItem(list, true, mPrng);
            if (selectedCreature.empty())
                continue;

            MWWorld::ManualRef ref(mStore, selectedCreature, 1);

            safePlaceObject(ref.getPtr(), getPlayerPtr(), getPlayerPtr().getCell(), 0, 220.f);
        }
    }

    void World::spawnEffect(VFS::Path::NormalizedView model, const std::string& textureOverride,
        const osg::Vec3f& worldPos, float scale, bool isMagicVFX, bool useAmbientLight, std::string_view effectId,
        bool loop)
    {
        mRendering->spawnEffect(model, textureOverride, worldPos, scale, isMagicVFX, useAmbientLight, effectId, loop);
    }

    void World::removeEffect(std::string_view effectId)
    {
        mRendering->removeEffect(effectId);
    }

    struct ResetActorsVisitor
    {
        World& mWorld;

        bool operator()(const Ptr& ptr)
        {
            if (ptr.getClass().isActor() && ptr.getCellRef().hasContentFile())
            {
                if (ptr.getCell()->movedHere(ptr))
                    return true;

                const ESM::Position& origPos = ptr.getCellRef().getPosition();
                mWorld.moveObject(ptr, origPos.asVec3());
                mWorld.rotateObject(ptr, origPos.asRotationVec3());
                ptr.getClass().adjustPosition(ptr, true);
            }
            return true;
        }
    };

    void World::resetActors()
    {
        for (CellStore* cellstore : mWorldScene->getActiveCells())
        {
            ResetActorsVisitor visitor{ *this };
            cellstore->forEach(visitor);
        }
    }

    bool World::isWalkingOnWater(const ConstPtr& actor) const
    {
        const MWPhysics::Actor* physicActor = mPhysics->getActor(actor);
        if (physicActor && physicActor->isWalkingOnWater())
            return true;
        return false;
    }

    osg::Vec3f World::aimToTarget(const ConstPtr& actor, const ConstPtr& target, bool isRangedCombat)
    {
        osg::Vec3f weaponPos = actor.getRefData().getPosition().asVec3();
        float heightRatio = isRangedCombat ? 2.f * Constants::TorsoHeight : 1.f;
        weaponPos.z() += mPhysics->getHalfExtents(actor).z() * heightRatio;
        osg::Vec3f targetPos = mPhysics->getCollisionObjectPosition(target);
        return (targetPos - weaponPos);
    }

    namespace
    {
        void preload(MWWorld::Scene* scene, const ESMStore& store, const ESM::RefId& obj)
        {
            if (obj.empty())
                return;
            try
            {
                MWWorld::ManualRef ref(store, obj);
                std::string model = ref.getPtr().getClass().getCorrectedModel(ref.getPtr());
                if (!model.empty())
                    scene->preload(model, ref.getPtr().getClass().useAnim());
            }
            catch (const std::exception& e)
            {
                Log(Debug::Warning) << "Failed to preload scene object " << obj << ": " << e.what();
            }
        }
    }

    void World::preloadEffects(const ESM::EffectList* effectList)
    {
        for (const ESM::IndexedENAMstruct& effectInfo : effectList->mList)
        {
            const ESM::MagicEffect* effect = mStore.get<ESM::MagicEffect>().find(effectInfo.mData.mEffectID);

            if (MWMechanics::isSummoningEffect(effectInfo.mData.mEffectID))
            {
                preload(mWorldScene.get(), mStore, ESM::RefId::stringRefId("VFX_Summon_Start"));
                preload(mWorldScene.get(), mStore, MWMechanics::getSummonedCreature(effectInfo.mData.mEffectID));
            }

            preload(mWorldScene.get(), mStore, effect->mCasting);
            preload(mWorldScene.get(), mStore, effect->mHit);

            if (effectInfo.mData.mArea > 0)
                preload(mWorldScene.get(), mStore, effect->mArea);
            if (effectInfo.mData.mRange == ESM::RT_Target)
                preload(mWorldScene.get(), mStore, effect->mBolt);
        }
    }

    DetourNavigator::Navigator* World::getNavigator() const
    {
        return mNavigator.get();
    }

    void World::updateActorPath(const MWWorld::ConstPtr& actor, const std::deque<osg::Vec3f>& path,
        const DetourNavigator::AgentBounds& agentBounds, const osg::Vec3f& start, const osg::Vec3f& end) const
    {
        mRendering->updateActorPath(actor, path, agentBounds, start, end);
    }

    void World::removeActorPath(const MWWorld::ConstPtr& actor) const
    {
        mRendering->removeActorPath(actor);
    }

    void World::setNavMeshNumberToRender(const std::size_t value)
    {
        mRendering->setNavMeshNumber(value);
    }

    DetourNavigator::AgentBounds World::getPathfindingAgentBounds(const MWWorld::ConstPtr& actor) const
    {
        const MWPhysics::Actor* physicsActor = mPhysics->getActor(actor);
        if (physicsActor == nullptr || !actor.isInCell() || actor.getCell()->isExterior())
            return DetourNavigator::AgentBounds{ Settings::game().mActorCollisionShapeType,
                Settings::game().mDefaultActorPathfindHalfExtents };
        else
            return DetourNavigator::AgentBounds{ physicsActor->getCollisionShapeType(),
                physicsActor->getHalfExtents() };
    }

    bool World::hasCollisionWithDoor(
        const MWWorld::ConstPtr& door, const osg::Vec3f& position, const osg::Vec3f& destination) const
    {
        const auto object = mPhysics->getObject(door);

        if (!object)
            return false;

        btVector3 aabbMin;
        btVector3 aabbMax;
        object->getShapeInstance()->mCollisionShape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);

        const auto toLocal = object->getTransform().inverse();
        const auto localFrom = toLocal(Misc::Convert::toBullet(position));
        const auto localTo = toLocal(Misc::Convert::toBullet(destination));

        btScalar hitDistance = 1;
        btVector3 hitNormal;
        return btRayAabb(localFrom, localTo, aabbMin, aabbMax, hitDistance, hitNormal);
    }

    bool World::isAreaOccupiedByOtherActor(const MWWorld::ConstPtr& actor, const osg::Vec3f& position) const
    {
        const osg::Vec3f halfExtents = getPathfindingAgentBounds(actor).mHalfExtents;
        const float maxHalfExtent = std::max(halfExtents.x(), std::max(halfExtents.y(), halfExtents.z()));
        return mPhysics->isAreaOccupiedByOtherActor(actor.mRef, position, 2 * maxHalfExtent);
    }

    void World::reportStats(unsigned int frameNumber, osg::Stats& stats) const
    {
        DetourNavigator::reportStats(mNavigator->getStats(), frameNumber, stats);
        mPhysics->reportStats(frameNumber, stats);
        mWorldScene->reportStats(frameNumber, stats);
    }

    std::vector<MWWorld::Ptr> World::getAll(const ESM::RefId& id)
    {
        return mWorldModel.getAll(id);
    }

    Misc::Rng::Generator& World::getPrng()
    {
        return mPrng;
    }

    MWRender::PostProcessor* World::getPostProcessor()
    {
        return mRendering->getPostProcessor();
    }

    void World::setActorActive(const MWWorld::Ptr& ptr, bool value)
    {
        if (MWPhysics::Actor* const actor = mPhysics->getActor(ptr))
            actor->setActive(value);
    }
}
