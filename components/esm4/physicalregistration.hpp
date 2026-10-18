#ifndef OPENMW_ESM4_PHYSICALREGISTRATION_H
#define OPENMW_ESM4_PHYSICALREGISTRATION_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ESM4
{
    enum class PhysicalBodyWorldMembership : std::uint8_t { Absent, Desired, Other };

    struct PhysicalBodyBlendRegistration
    {
        float mHierarchyGain, mVelocityGain;
        std::int32_t mCounter;
        bool mRegistrationOverride;
        friend bool operator==(const PhysicalBodyBlendRegistration&, const PhysicalBodyBlendRegistration&) = default;
    };

    struct PhysicalBodyRegistrationPlan
    {
        bool mRemoveCurrentWorld, mAddDesiredWorld;
        bool mRememberDesiredWorld, mDeferred, mReturnsTrue;
        friend bool operator==(const PhysicalBodyRegistrationPlan&, const PhysicalBodyRegistrationPlan&) = default;
    };

    // Original8A48C0/89F470 decision with the optional low-velocity sleep
    // branch excluded. Blend metadata must be resolved from the actual map;
    // null means no mapped blend. Gains must be finite, without clamping.
    // Additions subsequently traverse associated constraints; same-World
    // no-ops do not. ReturnsTrue is the native result, not operation success:
    // a deferred, already absent body also returns true. No World writes,
    // queues, constraint traversal or remembered-owner publication occur here.
    PhysicalBodyRegistrationPlan resolvePhysicalBodyRegistration(PhysicalBodyWorldMembership membership,
        const std::optional<PhysicalBodyBlendRegistration>& blend);

    enum class PhysicalBlendCounterOperation : std::uint8_t { Increment, Decrement };
    enum class PhysicalBlendCounterSceneOperation : std::uint8_t { None, DirectSync, Virtual64Update };

    struct PhysicalBlendCounterState
    {
        std::int32_t mCounter;
        std::uint16_t mCollisionFlags;
        friend bool operator==(const PhysicalBlendCounterState&, const PhysicalBlendCounterState&) = default;
    };

    struct PhysicalBlendCounterContext
    {
        // Raw originalBA7A8C. Only values2 and3 receive special handling.
        std::int32_t mRawMode;
        bool mHasBodyWrapper, mHasNativeBody;
        std::uint32_t mPackedFilter;
        bool mHasRememberedWorldOwner;
    };

    struct PhysicalBlendCounterStep
    {
        PhysicalBlendCounterState mState;
        PhysicalBlendCounterSceneOperation mSceneOperation;
        bool mRequestNormalBodyRegistration;
        friend bool operator==(const PhysicalBlendCounterStep&, const PhysicalBlendCounterStep&) = default;
    };

    // Full88F0E0/88F160 counter/flag decisions. Wrap32 bits explicitly before
    // signed tests. Requests describe the observed downstream calls only;
    // the caller must execute the scene request before body registration,
    // against resolved scene/World identities.
    // Does not assign counter ownership/lifetime or invoke forced admission.
    PhysicalBlendCounterStep resolvePhysicalBlendCounterStep(PhysicalBlendCounterState state,
        PhysicalBlendCounterOperation operation, const PhysicalBlendCounterContext& context);

    struct PhysicalRegistrationBody
    {
        bool mRegistered = false;
        std::uint32_t mPackedFilter = 0;
        bool mHasRememberedWorldOwner = false;
        std::optional<PhysicalBodyBlendRegistration> mBlend;
        std::vector<std::size_t> mAssociatedConstraints;
        friend bool operator==(const PhysicalRegistrationBody&, const PhysicalRegistrationBody&) = default;
    };

    struct PhysicalRegistrationConstraint
    {
        std::size_t mBodyA, mBodyB;
        bool mRegistered;
        friend bool operator==(const PhysicalRegistrationConstraint&, const PhysicalRegistrationConstraint&) = default;
    };

    struct PhysicalRegistrationGraph
    {
        std::vector<PhysicalRegistrationBody> mBodies;
        std::vector<PhysicalRegistrationConstraint> mConstraints;
        friend bool operator==(const PhysicalRegistrationGraph&, const PhysicalRegistrationGraph&) = default;
    };

    enum class PhysicalRegistrationEventKind : std::uint8_t
    {
        NormalBodyRequest, ForcedBodyRequest, ConstraintRequest, BodyAdded,
        BodyRemoved, IncidentConstraintRemoved, ConstraintAdded, ForcedAdmissionDiagnostic,
    };

    struct PhysicalRegistrationEvent
    {
        PhysicalRegistrationEventKind mKind;
        // Body index for body events, constraint index otherwise.
        std::size_t mIndex;
        // The filter at admission, before any later removal/re-admission.
        std::uint32_t mPackedFilter = 0;
        friend bool operator==(const PhysicalRegistrationEvent&, const PhysicalRegistrationEvent&) = default;
    };

    struct PhysicalRegistrationGraphPlan
    {
        PhysicalRegistrationGraph mGraph;
        std::vector<PhysicalRegistrationEvent> mEvents;
        std::vector<unsigned char> mReturnsTrue;
    };

    // Stage ordered normal body requests against one resolved World, with all
    // endpoint wrappers present and World operation gates/queues zero. Follow
    // actual authored lists; do not add an undirected graph closure. Retain
    // removals/re-admissions even when final membership is unchanged.
    // Validates every endpoint, association and initial membership before work.
    // A bounded iterative traversal throws length_error on exhaustion, leaving
    // input untouched. No partial result is published; a native nonreturning
    // prefix is not treated as a completed plan. No solver, queue, scene,
    // low-velocity sleep or callback list mutation is implemented here.
    PhysicalRegistrationGraphPlan preparePhysicalRegistrationGraph(const PhysicalRegistrationGraph& graph,
        std::span<const std::size_t> normalRequests, std::size_t operationLimit = 4096);
}

#endif
