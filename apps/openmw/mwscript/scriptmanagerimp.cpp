#include "scriptmanagerimp.hpp"
#include <stdexcept>
#include <type_traits>

#include <algorithm>
#include <cassert>
#include <exception>
#include <sstream>

#include <components/debug/debuglog.hpp>

#include <components/esm/refid.hpp>
#include <components/esm3/loadscpt.hpp>

#include <components/misc/strings/lower.hpp>

#include <components/compiler/context.hpp>
#include <components/compiler/exception.hpp>
#include <components/compiler/quickfileparser.hpp>
#include <components/compiler/scanner.hpp>

#include "../mwworld/esmstore.hpp"

#include "extensions.hpp"
#include "interpretercontext.hpp"

namespace MWScript
{
    namespace
    {
        struct RuntimeCall
        {
            unsigned& mDepth;
            explicit RuntimeCall(unsigned& depth) : mDepth(depth) { ++mDepth; }
            ~RuntimeCall() { --mDepth; }
        };
    }

    struct ScriptManager::RetainedRuntime::Data
    {
        ScriptManager* mOwner;
        decltype(ScriptManager::mScripts) mScripts{};
        decltype(ScriptManager::mOtherLocals) mOtherLocals{};
        GlobalScripts::RetainedRuntime mGlobals;
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(ScriptManager& owner) : mOwner(&owner), mGlobals(owner.mGlobalScripts.retainRuntime()) {}
        void swap() noexcept
        {
            using std::swap;
            static_assert(std::is_nothrow_swappable_v<decltype(mScripts)>);
            swap(mOwner->mScripts, mScripts);
            static_assert(std::is_nothrow_swappable_v<decltype(mOtherLocals)>);
            swap(mOwner->mOtherLocals, mOtherLocals);
        }
    };
    void ScriptManager::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Outgoing ScriptManager runtime is frozen before detachment");
    }
    ScriptManager::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    ScriptManager::RetainedRuntime::~RetainedRuntime()
    {
        if (mData && mData->mOwner && !restore()) std::terminate();
    }
    ScriptManager::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    ScriptManager::RetainedRuntime& ScriptManager::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    ScriptManager::RetainedRuntime ScriptManager::retainRuntime()
    {
        if (mRetainedRuntime || mActiveCalls) throw std::logic_error("ScriptManager retention requires an idle runtime");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    bool ScriptManager::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached || mData->mOwner->mActiveCalls) return false;
        if (!mData->mGlobals.detach()) return false;
        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool ScriptManager::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached || mData->mOwner->mActiveCalls) return false;
        if (!mData->mGlobals.exchange()) return false;
        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool ScriptManager::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner || mData->mOwner->mActiveCalls) return false;
        if (!mData->mGlobals.restore()) return false;
        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
        return true;
    }
    void ScriptManager::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected && !exchange()) std::terminate();
        mData->mGlobals.discard();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
    }
    ScriptManager::~ScriptManager()
    {
        if (mRetainedRuntime) { mRetainedRuntime->mGlobals.discard(); mRetainedRuntime->mOwner = nullptr; }
    }


    ScriptManager::ScriptManager(const MWWorld::ESMStore& store, Compiler::Context& compilerContext, int warningsMode)
        : mErrorHandler()
        , mStore(store)
        , mCompilerContext(compilerContext)
        , mParser(mErrorHandler, mCompilerContext)
        , mGlobalScripts(store)
    {
        installOpcodes(mInterpreter);

        mErrorHandler.setWarningsMode(warningsMode);
    }

    bool ScriptManager::compile(const ESM::RefId& name)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        mParser.reset();
        mErrorHandler.reset();

        if (const ESM::Script* script = mStore.get<ESM::Script>().find(name))
        {
            mErrorHandler.setContext(script->mId.getRefIdString());

            bool success = true;
            try
            {
                std::istringstream input(script->mScriptText);

                Compiler::Scanner scanner(mErrorHandler, input, mCompilerContext.getExtensions());

                scanner.scan(mParser);

                if (!mErrorHandler.isGood())
                    success = false;
            }
            catch (const Compiler::SourceException&)
            {
                // error has already been reported via error handler
                success = false;
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "Error: An exception has been thrown: " << error.what();
                success = false;
            }

            if (!success)
            {
                Log(Debug::Error) << "Error: script compiling failed: " << name;
            }

            if (success)
            {
                mScripts.emplace(name, CompiledScript(mParser.getProgram(), mParser.getLocals()));

                return true;
            }
        }

        return false;
    }

    bool ScriptManager::run(const ESM::RefId& name, Interpreter::Context& interpreterContext)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        // compile script
        auto iter = mScripts.find(name);

        if (iter == mScripts.end())
        {
            if (!compile(name))
            {
                // failed -> ignore script from now on.
                mScripts.emplace(name, CompiledScript({}, Compiler::Locals()));
                return false;
            }

            iter = mScripts.find(name);
            assert(iter != mScripts.end());
        }

        // execute script
        const auto& target = interpreterContext.getTarget();
        if (!iter->second.mProgram.mInstructions.empty()
            && iter->second.mInactive.find(target) == iter->second.mInactive.end())
        {
            try
            {
                mInterpreter.run(iter->second.mProgram, interpreterContext);
                return true;
            }
            catch (const MissingImplicitRefError& e)
            {
                Log(Debug::Error) << "Execution of script " << name << " failed: " << e.what();
            }
            catch (const std::exception& e)
            {
                Log(Debug::Error) << "Execution of script " << name << " failed: " << e.what();

                iter->second.mInactive.insert(target); // don't execute again.
            }
        }
        return false;
    }

    void ScriptManager::clear()
    {
        requireMutableRuntime();
        for (auto& script : mScripts)
        {
            script.second.mInactive.clear();
        }

        mGlobalScripts.clear();
    }

    std::pair<int, int> ScriptManager::compileAll()
    {
        requireMutableRuntime();
        int count = 0;
        int success = 0;

        for (auto& script : mStore.get<ESM::Script>())
        {
            ++count;

            if (compile(script.mId))
                ++success;
        }

        return std::make_pair(count, success);
    }

    const Compiler::Locals& ScriptManager::getLocals(const ESM::RefId& name)
    {
        requireMutableRuntime();
        {
            auto iter = mScripts.find(name);

            if (iter != mScripts.end())
                return iter->second.mLocals;
        }

        {
            auto iter = mOtherLocals.find(name);

            if (iter != mOtherLocals.end())
                return iter->second;
        }

        if (const ESM::Script* script = mStore.get<ESM::Script>().search(name))
        {
            Compiler::Locals locals;

            const Compiler::ContextOverride override(mErrorHandler, name.getRefIdString() + "[local variables]");

            std::istringstream stream(script->mScriptText);
            Compiler::QuickFileParser parser(mErrorHandler, mCompilerContext, locals);
            Compiler::Scanner scanner(mErrorHandler, stream, mCompilerContext.getExtensions());
            try
            {
                scanner.scan(parser);
            }
            catch (const Compiler::SourceException&)
            {
                // error has already been reported via error handler
                locals.clear();
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "Error: An exception has been thrown: " << error.what();
                locals.clear();
            }

            auto iter = mOtherLocals.emplace(name, locals).first;

            return iter->second;
        }

        throw std::logic_error("script " + name.toDebugString() + " does not exist");
    }

    GlobalScripts& ScriptManager::getGlobalScripts()
    {
        return mGlobalScripts;
    }

    const Compiler::Extensions& ScriptManager::getExtensions() const
    {
        return *mCompilerContext.getExtensions();
    }
}
