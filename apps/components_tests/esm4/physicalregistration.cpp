#include <components/esm4/physicalregistration.hpp>

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    using Membership = ESM4::PhysicalBodyWorldMembership;
    using CounterOp = ESM4::PhysicalBlendCounterOperation;
    using SceneOp = ESM4::PhysicalBlendCounterSceneOperation;

    TEST(ESM4PhysicalRegistration, BodyAdmissionMatchesOriginalFiniteThresholdsAndNativeReturn)
    {
        // Full original8A48C0/89F470 corpus native-body-registration-static-01.
        // World and associated-constraint effects were recorded boundaries.
        struct Sample { float mHierarchy, mVelocity; std::int32_t mCounter; bool mOverride, mDeferred; };
        const std::array<Sample, 9> samples{{
            {1, 1, 0, false, true}, {1, 1, -1, false, true}, {1, 1, 1, false, false},
            {1, 1, 0, true, false}, {0, 1, 0, false, false}, {1, 0, 0, false, false},
            {std::nextafter(1.f, 0.f), 1, 0, false, false},
            {1, std::nextafter(1.f, 0.f), 0, false, false},
            {std::nextafter(1.f, 2.f), std::nextafter(1.f, 2.f), 0, false, true},
        }};
        for (const auto& row : samples)
            for (const auto membership : {Membership::Absent, Membership::Desired, Membership::Other})
            {
                const auto result = ESM4::resolvePhysicalBodyRegistration(membership,
                    ESM4::PhysicalBodyBlendRegistration{row.mHierarchy, row.mVelocity, row.mCounter, row.mOverride});
                EXPECT_EQ(result.mDeferred, row.mDeferred);
                EXPECT_TRUE(result.mRememberDesiredWorld);
                if (row.mDeferred)
                {
                    EXPECT_EQ(result.mRemoveCurrentWorld, membership != Membership::Absent);
                    EXPECT_FALSE(result.mAddDesiredWorld);
                    EXPECT_TRUE(result.mReturnsTrue); // Also true for an absent body.
                }
                else
                {
                    EXPECT_EQ(result.mRemoveCurrentWorld, membership == Membership::Other);
                    EXPECT_EQ(result.mAddDesiredWorld, membership != Membership::Desired);
                    EXPECT_EQ(result.mReturnsTrue, membership != Membership::Desired);
                }
            }
    }

    TEST(ESM4PhysicalRegistration, UnmappedBodiesRetainGenericWorldDecisionAndInvalidInputsReject)
    {
        EXPECT_EQ(ESM4::resolvePhysicalBodyRegistration(Membership::Absent, std::nullopt),
            (ESM4::PhysicalBodyRegistrationPlan{false, true, false, false, true}));
        EXPECT_EQ(ESM4::resolvePhysicalBodyRegistration(Membership::Desired, std::nullopt),
            (ESM4::PhysicalBodyRegistrationPlan{false, false, false, false, false}));
        EXPECT_EQ(ESM4::resolvePhysicalBodyRegistration(Membership::Other, std::nullopt),
            (ESM4::PhysicalBodyRegistrationPlan{true, true, false, false, true}));
        EXPECT_THROW(ESM4::resolvePhysicalBodyRegistration(static_cast<Membership>(99), std::nullopt),
            std::invalid_argument);
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity()})
        {
            EXPECT_THROW(ESM4::resolvePhysicalBodyRegistration(Membership::Desired,
                ESM4::PhysicalBodyBlendRegistration{invalid, 1, 1, true}), std::invalid_argument);
            EXPECT_THROW(ESM4::resolvePhysicalBodyRegistration(Membership::Absent,
                ESM4::PhysicalBodyBlendRegistration{1, invalid, 0, false}), std::invalid_argument);
        }
    }

    TEST(ESM4PhysicalRegistration, CounterWrapAndSignedClampMatchOriginalHelpers)
    {
        // Frozen full88F0E0/88F160 native-blend-counter-static-01 rows.
        struct Sample { std::int32_t mOld, mIncremented, mDecremented; bool mIncrementRequest, mDecrementRequest; };
        constexpr auto minimum = std::numeric_limits<std::int32_t>::min();
        constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
        const std::array<Sample, 8> samples{{
            {minimum, minimum + 1, maximum, false, false},
            {-2, -1, 0, false, true}, {-1, 0, 0, false, true}, {0, 1, 0, true, true},
            {1, 2, 0, false, true}, {2, 3, 1, false, false},
            {maximum - 1, maximum, maximum - 2, false, false},
            {maximum, minimum, maximum - 1, false, false},
        }};
        const ESM4::PhysicalBlendCounterContext context{0, true, true, 8, true};
        for (const auto& row : samples)
        {
            const ESM4::PhysicalBlendCounterState initial{row.mOld, 0};
            EXPECT_EQ(ESM4::resolvePhysicalBlendCounterStep(initial, CounterOp::Increment, context),
                (ESM4::PhysicalBlendCounterStep{{row.mIncremented, 0}, SceneOp::None, row.mIncrementRequest}));
            EXPECT_EQ(ESM4::resolvePhysicalBlendCounterStep(initial, CounterOp::Decrement, context),
                (ESM4::PhysicalBlendCounterStep{{row.mDecremented, 0}, SceneOp::None, row.mDecrementRequest}));
        }
    }

    TEST(ESM4PhysicalRegistration, CounterZeroIncrementKeepsOriginalSceneRequestsAndFlags)
    {
        struct Sample { std::uint16_t mFlags, mNormalFlags; SceneOp mNormal, mMode2; };
        const std::array<Sample, 6> samples{{
            {0, 0, SceneOp::None, SceneOp::None}, {1, 1, SceneOp::Virtual64Update, SceneOp::Virtual64Update},
            {0x40, 0, SceneOp::DirectSync, SceneOp::None}, {0x41, 1, SceneOp::DirectSync, SceneOp::None},
            {0x101, 0x101, SceneOp::Virtual64Update, SceneOp::Virtual64Update},
            {0xffff, 0xffbf, SceneOp::DirectSync, SceneOp::None},
        }};
        for (const auto& row : samples)
            for (std::int32_t mode : {-1, 0, 2, 3})
            {
                const ESM4::PhysicalBlendCounterContext context{mode, true, true, 8, true};
                const auto result = ESM4::resolvePhysicalBlendCounterStep({0, row.mFlags}, CounterOp::Increment, context);
                EXPECT_EQ(result.mState.mCounter, 1);
                EXPECT_EQ(result.mState.mCollisionFlags, mode == 2 || mode == 3 ? row.mFlags : row.mNormalFlags);
                EXPECT_EQ(result.mSceneOperation, mode == 3 ? SceneOp::None : mode == 2 ? row.mMode2 : row.mNormal);
                EXPECT_TRUE(result.mRequestNormalBodyRegistration);
                // Increment from a nonzero count and every decrement skip
                // this scene work, even when decrement requests registration.
                EXPECT_EQ(ESM4::resolvePhysicalBlendCounterStep({1, row.mFlags}, CounterOp::Increment, context),
                    (ESM4::PhysicalBlendCounterStep{{2, row.mFlags}, SceneOp::None, false}));
                EXPECT_EQ(ESM4::resolvePhysicalBlendCounterStep({1, row.mFlags}, CounterOp::Decrement, context),
                    (ESM4::PhysicalBlendCounterStep{{0, row.mFlags}, SceneOp::None, true}));
            }
    }

    TEST(ESM4PhysicalRegistration, CounterRegistrationRequiresBodyLayerAndRememberedOwner)
    {
        struct Sample { ESM4::PhysicalBlendCounterContext mContext; bool mRequest; };
        const std::array<Sample, 7> samples{{
            {{0, true, true, 8, true}, true}, {{0, true, true, 0x108, true}, true},
            {{0, false, false, 8, true}, false}, {{0, true, false, 8, true}, false},
            {{0, true, true, 0, true}, false}, {{0, true, true, 0xffffffffu, true}, false},
            {{0, true, true, 8, false}, false},
        }};
        for (const auto& row : samples)
            for (auto operation : {CounterOp::Increment, CounterOp::Decrement})
                EXPECT_EQ(ESM4::resolvePhysicalBlendCounterStep({0, 0}, operation, row.mContext)
                    .mRequestNormalBodyRegistration, row.mRequest);
    }

    TEST(ESM4PhysicalRegistration, InvalidCounterOperationRejectsBeforeProducingRequests)
    {
        const ESM4::PhysicalBlendCounterState initial{0, 0x41};
        EXPECT_THROW(ESM4::resolvePhysicalBlendCounterStep(initial, static_cast<CounterOp>(99),
            ESM4::PhysicalBlendCounterContext{0, true, true, 8, true}), std::invalid_argument);
        EXPECT_EQ(initial, (ESM4::PhysicalBlendCounterState{0, 0x41}));
    }

    ESM4::PhysicalRegistrationGraph registrationChain()
    {
        ESM4::PhysicalRegistrationGraph graph;
        graph.mBodies.resize(3);
        for (unsigned i = 0; i < 3; ++i)
        {
            graph.mBodies[i].mPackedFilter = ((0x1010u + i) << 16) | (0x30u + i);
            graph.mBodies[i].mBlend = ESM4::PhysicalBodyBlendRegistration{0, 0, 0, false};
        }
        graph.mBodies[1].mAssociatedConstraints = {0};
        graph.mBodies[2].mAssociatedConstraints = {1};
        graph.mConstraints = {{1, 0, false}, {2, 1, false}};
        return graph;
    }

    TEST(ESM4PhysicalRegistration, DirectedListsAtEndpointBDoNotInventAdmissionOfEndpointA)
    {
        auto graph = registrationChain();
        graph.mBodies[0].mAssociatedConstraints = {0};
        graph.mBodies[1].mAssociatedConstraints = {1};
        graph.mBodies[2].mAssociatedConstraints.clear();
        const auto before = graph;
        const std::array<std::size_t, 1> requests{0};
        const auto result = ESM4::preparePhysicalRegistrationGraph(graph, requests);
        EXPECT_EQ(graph, before);
        EXPECT_TRUE(result.mGraph.mBodies[0].mRegistered);
        EXPECT_FALSE(result.mGraph.mBodies[1].mRegistered);
        EXPECT_FALSE(result.mGraph.mBodies[2].mRegistered);
        EXPECT_FALSE(result.mGraph.mConstraints[0].mRegistered);
        EXPECT_FALSE(result.mGraph.mConstraints[1].mRegistered);
        EXPECT_EQ(result.mReturnsTrue, (std::vector<unsigned char>{1}));
        using Kind = ESM4::PhysicalRegistrationEventKind;
        EXPECT_EQ(result.mEvents, (std::vector<ESM4::PhysicalRegistrationEvent>{
            {Kind::NormalBodyRequest, 0}, {Kind::BodyAdded, 0, 0x10100030},
            {Kind::ConstraintRequest, 0}, {Kind::NormalBodyRequest, 0}}));
    }

    TEST(ESM4PhysicalRegistration, ExistingWorldMembershipSkipsAssociatedListsForRepeatedRequests)
    {
        auto graph = registrationChain();
        for (auto& body : graph.mBodies) body.mRegistered = true;
        const std::array<std::size_t, 2> requests{2, 2};
        const auto result = ESM4::preparePhysicalRegistrationGraph(graph, requests);
        EXPECT_EQ(result.mReturnsTrue, (std::vector<unsigned char>{0, 0}));
        ASSERT_EQ(result.mEvents.size(), 2u);
        for (const auto& event : result.mEvents)
            EXPECT_EQ(event.mKind, ESM4::PhysicalRegistrationEventKind::NormalBodyRequest);
        EXPECT_FALSE(result.mGraph.mConstraints[0].mRegistered);
        EXPECT_FALSE(result.mGraph.mConstraints[1].mRegistered);
        EXPECT_TRUE(result.mGraph.mBodies[2].mHasRememberedWorldOwner);
    }

    TEST(ESM4PhysicalRegistration, NativeRemovalAndReadmissionRetainPreviouslyRegisteredEndpointFilter)
    {
        auto graph = registrationChain();
        graph.mBodies[1].mRegistered = true;
        graph.mBodies[1].mBlend = ESM4::PhysicalBodyBlendRegistration{1, 1, 0, false};
        const auto before = graph;
        const std::array<std::size_t, 1> requests{2};
        const auto result = ESM4::preparePhysicalRegistrationGraph(graph, requests);
        EXPECT_EQ(graph, before);
        // Frozen chain/gains010/start2/initialmask2 row of the expanded
        // original-instruction corpus03. Body1 retains1011, so its ancestor
        // inherits1011 rather than the initiating body's1012 group.
        using Kind = ESM4::PhysicalRegistrationEventKind;
        EXPECT_EQ(result.mEvents, (std::vector<ESM4::PhysicalRegistrationEvent>{
            {Kind::NormalBodyRequest, 2}, {Kind::BodyAdded, 2, 0x10120032},
            {Kind::ConstraintRequest, 1}, {Kind::NormalBodyRequest, 1},
            {Kind::BodyRemoved, 1}, {Kind::ForcedAdmissionDiagnostic, 1},
            {Kind::ForcedBodyRequest, 1}, {Kind::BodyAdded, 1, 0x10110031},
            {Kind::ConstraintRequest, 0}, {Kind::NormalBodyRequest, 0},
            {Kind::BodyAdded, 0, 0x10110030}, {Kind::ConstraintAdded, 0}, {Kind::ConstraintAdded, 1}}));
        for (const auto& body : result.mGraph.mBodies)
        {
            EXPECT_TRUE(body.mRegistered);
            EXPECT_TRUE(body.mHasRememberedWorldOwner);
        }
        EXPECT_TRUE(result.mGraph.mConstraints[0].mRegistered);
        EXPECT_TRUE(result.mGraph.mConstraints[1].mRegistered);
        EXPECT_EQ(result.mGraph.mBodies[0].mPackedFilter, 0x10110030u);
    }

    TEST(ESM4PhysicalRegistration, RepeatedAssociationsKeepRequestOrderButDoNotDuplicateOwnedConstraints)
    {
        auto graph = registrationChain();
        graph.mBodies[1].mAssociatedConstraints = {0, 0};
        graph.mBodies[2].mAssociatedConstraints = {1, 1};
        const std::array<std::size_t, 1> requests{2};
        const auto result = ESM4::preparePhysicalRegistrationGraph(graph, requests);
        using Kind = ESM4::PhysicalRegistrationEventKind;
        EXPECT_EQ(std::count_if(result.mEvents.begin(), result.mEvents.end(),
            [](const auto& event) { return event.mKind == Kind::ConstraintRequest; }), 4);
        EXPECT_EQ(std::count_if(result.mEvents.begin(), result.mEvents.end(),
            [](const auto& event) { return event.mKind == Kind::ConstraintAdded; }), 2);
        EXPECT_EQ(result.mEvents.back(), (ESM4::PhysicalRegistrationEvent{Kind::ConstraintRequest, 1}));
        EXPECT_EQ(result.mGraph.mBodies[0].mPackedFilter, 0x10120030u);
    }

    TEST(ESM4PhysicalRegistration, InvalidGraphAndRequestRejectBeforeReturningAPlan)
    {
        const auto graph = registrationChain();
        const std::array<std::size_t, 2> requests{2, 999};
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(graph, requests), std::invalid_argument);
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph({}, {}), std::invalid_argument);
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(graph, {}, 0), std::invalid_argument);
        auto malformed = graph;
        malformed.mConstraints[0].mRegistered = true;
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        malformed = graph; malformed.mConstraints[0].mBodyA = 999;
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        malformed = graph; malformed.mBodies[0].mAssociatedConstraints = {1};
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        malformed = graph; malformed.mBodies[0].mAssociatedConstraints = {999};
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        malformed = graph; malformed.mBodies[0].mBlend.reset();
        malformed.mBodies[0].mHasRememberedWorldOwner = true;
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        malformed = graph; malformed.mBodies[0].mBlend->mHierarchyGain = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(malformed, {}), std::invalid_argument);
        const auto empty = ESM4::preparePhysicalRegistrationGraph(graph, {});
        EXPECT_EQ(empty.mGraph, graph);
        EXPECT_TRUE(empty.mEvents.empty());
        EXPECT_TRUE(empty.mReturnsTrue.empty());
    }

    TEST(ESM4PhysicalRegistration, BoundedTraversalNeverPublishesANativeNonterminalPrefix)
    {
        auto graph = registrationChain();
        graph.mBodies[0].mAssociatedConstraints = {0};
        graph.mBodies[1].mAssociatedConstraints = {0, 1};
        graph.mBodies[1].mBlend = ESM4::PhysicalBodyBlendRegistration{1, 1, 0, false};
        const auto before = graph;
        const std::array<std::size_t, 1> requests{2};
        // Native corpus03 retains this shared-list/gains010 prefix at the
        // declared256-event bound; it is never a golden final state.
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(graph, requests, 4096), std::length_error);
        EXPECT_EQ(graph, before);
        const auto valid = registrationChain();
        EXPECT_THROW(ESM4::preparePhysicalRegistrationGraph(valid, requests, 1), std::length_error);
        EXPECT_EQ(valid, registrationChain());
    }
}
