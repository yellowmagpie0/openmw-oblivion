#include "oblivionworldfixture.hpp"

#include <components/esm4/loadrefr.hpp>

#include "apps/openmw/mwworld/oblivionmaptravel.hpp"
#include "apps/openmw/mwgui/worldmapprojection.hpp"

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


TEST(OblivionMapTravel, ProvinceMapProjectsNorthUpAcrossTheAuthoredBounds)
{
    ESM4::World::Map map{};
    EXPECT_FALSE(MWGui::hasWorldMapBounds(map));
    map = { 2055, 1830, -59, 47, 59, -58 };
    ASSERT_TRUE(MWGui::hasWorldMapBounds(map));
    EXPECT_EQ(MWGui::projectWorldMap(map, -59 * 4096.f, 47 * 4096.f), osg::Vec2f(0, 0));
    EXPECT_EQ(MWGui::projectWorldMap(map, 59 * 4096.f, -58 * 4096.f), osg::Vec2f(2055, 1830));
    EXPECT_EQ(MWGui::projectWorldMap(map, 0, -5.5f * 4096), osg::Vec2f(1027.5f, 915));
    EXPECT_LT(MWGui::projectWorldMap(map, -60 * 4096.f, 0).x(), 0);
    EXPECT_NEAR(MWGui::worldMapTextureBottom(map, 2048, 2048), 1830.f / 2055, 0.0001f);
    EXPECT_EQ(MWGui::worldMapTextureBottom(map, 2048, 1024), 1.f);
    map.SEcellY = map.NWcellY;
    EXPECT_FALSE(MWGui::hasWorldMapBounds(map));
}
