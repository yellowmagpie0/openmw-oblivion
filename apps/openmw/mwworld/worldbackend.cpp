#include "worldbackend.hpp"

#include <components/detournavigator/navigator.hpp>
#include <components/detournavigator/settings.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/runtimeviewer.hpp"
#include "../mwrender/vismask.hpp"
#include "projectilemanager.hpp"
#include "weather.hpp"

namespace MWWorld
{
    struct World::BackendConfiguration
    {
        osg::ref_ptr<osgViewer::Viewer> mViewer;
        osg::ref_ptr<osg::Group> mRoot;
        SceneUtil::WorkQueue* mWorkQueue;
        SceneUtil::UnrefQueue* mUnrefQueue;
        std::optional<DetourNavigator::Settings> mNavigatorSettings;
    };

    struct World::PreparedBackend::Data
    {
        World* mOwner;
        osg::ref_ptr<MWRender::RuntimeViewer> mViewer;
        MWRender::RuntimeViewer::Bindings mOriginalBindings;
        std::optional<MWRender::RuntimeViewer::Bindings> mCandidateBindings;
        osg::ref_ptr<osg::Camera> mCamera;
        osg::ref_ptr<osg::Group> mRoot;
        std::unique_ptr<MWPhysics::PhysicsSystem> mPhysics;
        std::unique_ptr<DetourNavigator::Navigator> mNavigator;
        std::unique_ptr<MWRender::RenderingManager> mRendering;
        std::unique_ptr<ProjectileManager> mProjectiles;
        std::unique_ptr<WeatherManager> mWeather;
        std::unique_ptr<Scene> mScene;
        bool mSelected = false;
        bool mReady = false;
        bool mExchanged = false;

        explicit Data(World& owner)
            : mOwner(&owner)
            , mViewer(dynamic_cast<MWRender::RuntimeViewer*>(owner.mBackendConfiguration->mViewer.get()))
            , mOriginalBindings(mViewer->captureBindings())
        {
        }

        void restoreSelection() noexcept
        {
            if (!mSelected) return;
            if (!mViewer->selectBindings(mOriginalBindings)) std::terminate();
            mOwner->mRendering->selectRuntimeResources();
            mSelected = false;
        }

        void prepare()
        {
            auto& owner = *mOwner;
            auto& config = *owner.mBackendConfiguration;
            auto* originalCamera = mViewer->getCamera();
            mCamera = new osg::Camera;
            mCamera->setGraphicsContext(originalCamera->getGraphicsContext());
            mCamera->setViewport(new osg::Viewport(*originalCamera->getViewport()));
            mCamera->setProjectionMatrix(originalCamera->getProjectionMatrix());
            mCamera->setViewMatrix(originalCamera->getViewMatrix());
            mRoot = new osg::Group;
            for (unsigned int i = 0; i < config.mRoot->getNumChildren(); ++i)
                if (config.mRoot->getChild(i)->getNodeMask() == MWRender::Mask_GUI)
                    mRoot->addChild(config.mRoot->getChild(i));

            mPhysics = std::make_unique<MWPhysics::PhysicsSystem>(owner.mResourceSystem, mRoot);
            mNavigator = config.mNavigatorSettings
                ? DetourNavigator::makeNavigator(*config.mNavigatorSettings, owner.mUserDataPath)
                : DetourNavigator::makeNavigatorStub();
            // Even a partially constructed renderer can replace shared callbacks or shadow selection.
            mSelected = true;
            mViewer->setCamera(mCamera);
            mRendering = std::make_unique<MWRender::RenderingManager>(mViewer, mRoot, owner.mResourceSystem,
                config.mWorkQueue, *mNavigator, owner.mGroundcoverStore, *config.mUnrefQueue,
                MWRender::RenderingManager::ResourceConfiguration::Reuse);
            mProjectiles = std::make_unique<ProjectileManager>(mRendering->getLightRoot()->asGroup(),
                owner.mResourceSystem, mRendering.get(), mPhysics.get());
            mWeather = std::make_unique<WeatherManager>(*mRendering, owner.mStore);
            mScene = std::make_unique<Scene>(owner, *mRendering, mPhysics.get(), *mNavigator);
            mCandidateBindings.emplace(mViewer->captureBindings());
            restoreSelection();
            mReady = true;
        }

        void dispose(bool restoreOriginal = true, bool ownerDestroyed = false) noexcept
        {
            if (restoreOriginal && mExchanged && !exchange()) std::terminate();
            restoreSelection();
            mReady = false;
            // Scene destruction only stops preload work. Its live objects must be unloaded
            // through clearSelectedScene while the corresponding service owners are selected.
            if (!ownerDestroyed && mScene && !mScene->getActiveCells().empty()) std::terminate();
            mScene.reset();
            if (mProjectiles) mProjectiles->clear();
            mProjectiles.reset();
            mWeather.reset();
            mCandidateBindings.reset();
            if (mCamera)
            {
                mCamera->setRenderer(nullptr);
                mCamera->setUserData(nullptr);
                mCamera->setCullCallback(nullptr);
                mCamera->removeChildren(0, mCamera->getNumChildren());
            }
            if (mRoot)
            {
                mRoot->setUpdateCallback(nullptr);
                mRoot->setCullCallback(nullptr);
                mRoot->removeChildren(0, mRoot->getNumChildren());
            }
            mRendering.reset();
            mPhysics.reset();
            mNavigator.reset();
            mCamera = nullptr;
            mRoot = nullptr;
            mOriginalBindings = MWRender::RuntimeViewer::Bindings{};
            mViewer = nullptr;
        }

        bool exchange() noexcept
        {
            if (!mOwner || !mReady || mSelected || !mCandidateBindings || mViewer->areThreadsRunning()
                || MWBase::Environment::get().getWorld() != mOwner)
                return false;
            osg::ref_ptr<osg::Camera> outgoingCamera = mViewer->getCamera();
            if (!mViewer->selectBindings(mExchanged ? mOriginalBindings : *mCandidateBindings))
                return false;
            using std::swap;
            swap(mOwner->mPhysics, mPhysics);
            swap(mOwner->mNavigator, mNavigator);
            swap(mOwner->mRendering, mRendering);
            swap(mOwner->mProjectileManager, mProjectiles);
            swap(mOwner->mWeatherManager, mWeather);
            swap(mOwner->mWorldScene, mScene);
            swap(mOwner->mBackendConfiguration->mRoot, mRoot);
            mCamera = std::move(outgoingCamera);
            mOwner->mRendering->selectRuntimeResources();
            if (!MWBase::Environment::selectWorldScene(*mOwner, *mOwner->mWorldScene)) std::terminate();
            mExchanged = !mExchanged;
            return true;
        }
    };

    void World::rememberBackendConfiguration(Debug::Level maxRecastLogLevel, osgViewer::Viewer* viewer,
        osg::ref_ptr<osg::Group> rootNode, SceneUtil::WorkQueue* workQueue, SceneUtil::UnrefQueue& unrefQueue)
    {
        auto config = std::make_shared<BackendConfiguration>();
        config->mViewer = viewer;
        config->mRoot = std::move(rootNode);
        config->mWorkQueue = workQueue;
        config->mUnrefQueue = &unrefQueue;
        if (Settings::navigator().mEnable)
        {
            config->mNavigatorSettings.emplace(DetourNavigator::makeSettingsFromSettingsManager(maxRecastLogLevel));
            config->mNavigatorSettings->mRecast.mSwimHeightScale = mSwimHeightScale;
        }
        mBackendConfiguration = std::move(config);
    }

    World::PreparedBackend World::prepareBackend()
    {
        if (mPreparedBackend || mRetainedSimulation || mGameProfile != ESM::GameProfile::Oblivion
            || !mBackendConfiguration || !mPhysics || !mNavigator || !mRendering || !mProjectileManager
            || !mWeatherManager || !mWorldScene
            || !dynamic_cast<MWRender::RuntimeViewer*>(mBackendConfiguration->mViewer.get())
            || mBackendConfiguration->mViewer->areThreadsRunning())
            throw std::logic_error("World backend preparation requires an initialized idle native backend");
        auto data = std::make_unique<PreparedBackend::Data>(*this);
        try { data->prepare(); }
        catch (...) { data->dispose(); throw; }
        mPreparedBackend = data.get();
        return PreparedBackend(std::move(data));
    }

    void World::cancelPreparedBackend() noexcept
    {
        if (!mPreparedBackend) return;
        mPreparedBackend->dispose();
        mPreparedBackend->mOwner = nullptr;
        mPreparedBackend = nullptr;
    }

    void World::destroyRetainedBackends() noexcept
    {
        // Normal World destruction already requires external controllers/widgets to be gone.
        // Stop both backends' preload/physics/render users before either retained cell model dies.
        if (mPreparedBackend)
        {
            mPreparedBackend->dispose(true, true);
            mPreparedBackend->mOwner = nullptr;
            mPreparedBackend = nullptr;
        }
        mWorldScene.reset();
        if (mProjectileManager) mProjectileManager->clear();
        mProjectileManager.reset();
        mWeatherManager.reset();
        mRendering.reset();
        mPhysics.reset();
        mNavigator.reset();
    }

    World::PreparedBackend::PreparedBackend(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    World::PreparedBackend::~PreparedBackend() { cancel(); }
    World::PreparedBackend::PreparedBackend(PreparedBackend&&) noexcept = default;
    World::PreparedBackend& World::PreparedBackend::operator=(PreparedBackend&& other) noexcept
    {
        if (this != &other) { cancel(); mData = std::move(other.mData); }
        return *this;
    }
    bool World::PreparedBackend::ready() const noexcept { return mData && mData->mOwner && mData->mReady; }
    bool World::PreparedBackend::canCancel() const noexcept
    {
        if (!ready() || mData->mSelected || mData->mViewer->areThreadsRunning()
            || MWBase::Environment::get().getWorld() != mData->mOwner)
            return false;
        const auto* candidate = mData->mExchanged ? mData->mOwner->mWorldScene.get() : mData->mScene.get();
        return candidate && candidate->getActiveCells().empty();
    }
    bool World::PreparedBackend::exchange() noexcept { return mData && mData->exchange(); }
    bool World::PreparedBackend::clearSelectedScene()
    {
        if (!ready() || mData->mSelected || mData->mViewer->areThreadsRunning()
            || MWBase::Environment::get().getWorld() != mData->mOwner)
            return false;
        // The caller selects the matching cells, player, Lua, mechanics, UI and sound owners
        // before entry. Unload callbacks must run before any of those owners are released.
        auto& owner = *mData->mOwner;
        auto& scene = *owner.mWorldScene;
        if (owner.mPlayer)
        {
            const auto player = owner.getPlayerPtr();
            // Normal cell changes preserve the player's controller. Whole-backend retirement
            // must detach it while its renderer-owned animation is still alive. An empty
            // prepared scene does not own the outgoing simulation's player.
            if (player.isInCell() && scene.getActiveCells().contains(player.getCell()))
                MWBase::Environment::get().getMechanicsManager()->remove(player, false);
        }
        scene.clear();
        return true;
    }
    bool World::PreparedBackend::discard() noexcept
    {
        if (!ready() || !mData->mExchanged || mData->mSelected || mData->mViewer->areThreadsRunning()
            || MWBase::Environment::get().getWorld() != mData->mOwner
            || !mData->mScene->getActiveCells().empty())
            return false;
        auto* owner = mData->mOwner;
        mData->dispose(false);
        owner->mPreparedBackend = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
        return true;
    }
    void World::PreparedBackend::cancel() noexcept
    {
        if (!mData) return;
        if (mData->mOwner)
        {
            mData->dispose();
            mData->mOwner->mPreparedBackend = nullptr;
            mData->mOwner = nullptr;
        }
        mData.reset();
    }
}
