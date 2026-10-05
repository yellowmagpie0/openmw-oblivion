#ifndef MWLUA_ENGINEEVENTS_H
#define MWLUA_ENGINEEVENTS_H

#include <variant>
#include <algorithm>
#include <stdexcept>

#include <osg/Vec3f>

#include <components/esm3/cellref.hpp> // defines RefNum that is used as a unique id
#include <components/lua/utilpackage.hpp>

#include "../mwworld/cellstore.hpp"

namespace MWLua
{
    class GlobalScripts;

    class EngineEvents
    {
    public:
        explicit EngineEvents(GlobalScripts& globalScripts)
            : mGlobalScripts(globalScripts)
        {
        }

        struct OnActive
        {
            ESM::RefNum mObject;
        };
        struct OnInactive
        {
            ESM::RefNum mObject;
        };
        struct OnTeleported
        {
            ESM::RefNum mObject;
        };
        struct OnActivate
        {
            ESM::RefNum mActor;
            ESM::RefNum mObject;
        };
        struct OnUseItem
        {
            ESM::RefNum mActor;
            ESM::RefNum mObject;
            bool mForce;
        };
        struct OnConsume
        {
            ESM::RefNum mActor;
            ESM::RefNum mConsumable;
        };
        struct OnNewExterior
        {
            MWWorld::CellStore& mCell;
        };
        struct OnAnimationTextKey
        {
            ESM::RefNum mActor;
            std::string mGroupname;
            std::string mKey;
        };
        struct OnAnimationEnded
        {
            ESM::RefNum mActor;
            std::string mGroupname;
            std::string mStartKey;
            std::string mStopKey;
            float mTime;
            float mCompletion;
        };
        struct OnSkillUse
        {
            ESM::RefNum mActor;
            std::string mSkill;
            int useType;
            float scale;
        };
        struct OnSkillLevelUp
        {
            ESM::RefNum mActor;
            std::string mSkill;
            std::string mSource;
        };
        struct OnJailTimeServed
        {
            ESM::RefNum mActor;
            int mDays;
        };
        struct OnDropped
        {
            ESM::RefNum mObject;
            ESM::RefNum mActor;
            osg::Vec3f mPosition;
            LuaUtil::TransformQ mRotation;
        };
        struct OnPlaced
        {
            ESM::RefNum mObject;
            ESM::RefNum mActor;
            osg::Vec3f mPosition;
            LuaUtil::TransformQ mRotation;
        };
        using Event = std::variant<OnActive, OnInactive, OnConsume, OnActivate, OnUseItem, OnNewExterior, OnTeleported,
            OnAnimationTextKey, OnAnimationEnded, OnSkillUse, OnSkillLevelUp, OnJailTimeServed, OnDropped, OnPlaced>;

        void clear() { mQueue.clear(); }
        void addToQueue(Event e) { mQueue.push_back(std::move(e)); }
        std::size_t queueSize() const { return mQueue.size(); }
        void reserveNextEvent()
        {
            if (mQueue.size() == mQueue.max_size())
                throw std::length_error("engine event queue reservation overflow");
            if (mQueue.size() == mQueue.capacity())
            {
                const auto grown = mQueue.capacity() > mQueue.max_size() / 2
                    ? mQueue.max_size() : mQueue.capacity() * 2;
                mQueue.reserve(std::max(mQueue.size() + 1, grown));
            }
        }
        bool canPushPrepared(std::size_t size) const
        {
            return mQueue.size() == size && mQueue.capacity() > size;
        }
        void callEngineHandlers();

    private:
        class Visitor;

        GlobalScripts& mGlobalScripts;
        std::vector<Event> mQueue;
    };

}

#endif // MWLUA_ENGINEEVENTS_H
