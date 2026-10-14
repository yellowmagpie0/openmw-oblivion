#include "environment.hpp"

#include <cassert>

#include <components/resource/resourcesystem.hpp>

MWBase::Environment* MWBase::Environment::sThis = nullptr;

bool MWBase::Environment::selectWorldScene(const World& expectedWorld, MWWorld::Scene& value) noexcept
{
    if (!sThis || sThis->mWorld != &expectedWorld)
        return false;
    sThis->mWorldScene = &value;
    return true;
}

MWBase::Environment::Environment()
{
    assert(sThis == nullptr);
    sThis = this;
}

MWBase::Environment::~Environment()
{
    sThis = nullptr;
}
