#include "oblivionworldfixture.hpp"

#include <components/esm4/loadrefr.hpp>

#include "apps/openmw/mwworld/oblivionmaptravel.hpp"

#include <gtest/gtest.h>

TEST(OblivionMapTravel, HiddenUndiscoveredAndDeletedMarkersRejectTravelBeforeMoving)
{
    TestingOpenMW::NativeWorldFixture fixture;
    ESM4::Reference marker{};
    marker.mFormKey = ESM::FormKey::content("headless.esm", 0xab0);
    marker.mIsMapMarker = true;
    for (const auto flags : { 0, 1, 2 })
    {
        marker.mMapMarkerFlags = flags;
        EXPECT_FALSE(MWWorld::travelToOblivionMapMarker(fixture.mWorld, marker).empty());
    }
    marker.mMapMarkerFlags = 3;
    marker.mFlags = ESM4::Rec_Deleted;
    EXPECT_FALSE(MWWorld::travelToOblivionMapMarker(fixture.mWorld, marker).empty());
    marker.mFlags = 0;
    marker.mIsMapMarker = false;
    EXPECT_FALSE(MWWorld::travelToOblivionMapMarker(fixture.mWorld, marker).empty());
}
