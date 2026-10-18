#include "physicalregistration.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ESM4
{
    PhysicalBodyRegistrationPlan resolvePhysicalBodyRegistration(PhysicalBodyWorldMembership membership,
        const std::optional<PhysicalBodyBlendRegistration>& blend)
    {
        if (membership != PhysicalBodyWorldMembership::Absent && membership != PhysicalBodyWorldMembership::Desired
            && membership != PhysicalBodyWorldMembership::Other)
            throw std::invalid_argument("Invalid physical body World membership");
        if (blend && (!std::isfinite(blend->mHierarchyGain) || !std::isfinite(blend->mVelocityGain)))
            throw std::invalid_argument("Nonfinite physical body registration gains");
        const bool deferred = blend && blend->mHierarchyGain >= 1.f && blend->mVelocityGain >= 1.f
            && blend->mCounter <= 0 && !blend->mRegistrationOverride;
        if (deferred)
            return {membership != PhysicalBodyWorldMembership::Absent, false, true, true, true};
        return {membership == PhysicalBodyWorldMembership::Other, membership != PhysicalBodyWorldMembership::Desired,
            blend.has_value(), false, membership != PhysicalBodyWorldMembership::Desired};
    }

    PhysicalBlendCounterStep resolvePhysicalBlendCounterStep(PhysicalBlendCounterState state,
        PhysicalBlendCounterOperation operation, const PhysicalBlendCounterContext& context)
    {
        if (operation != PhysicalBlendCounterOperation::Increment && operation != PhysicalBlendCounterOperation::Decrement)
            throw std::invalid_argument("Invalid physical blend counter operation");
        const auto oldCounter = state.mCounter;
        const auto word = std::bit_cast<std::uint32_t>(oldCounter);
        state.mCounter = std::bit_cast<std::int32_t>(
            operation == PhysicalBlendCounterOperation::Increment ? word + 1u : word - 1u);
        PhysicalBlendCounterStep result{state, PhysicalBlendCounterSceneOperation::None, false};
        if (operation == PhysicalBlendCounterOperation::Increment)
        {
            if (oldCounter != 0) return result;
            if (context.mRawMode != 3)
            {
                if (state.mCollisionFlags & 0x40)
                {
                    if (context.mRawMode != 2)
                    {
                        result.mSceneOperation = PhysicalBlendCounterSceneOperation::DirectSync;
                        result.mState.mCollisionFlags &= ~std::uint16_t(0x40);
                    }
                }
                else if (state.mCollisionFlags & 1)
                    result.mSceneOperation = PhysicalBlendCounterSceneOperation::Virtual64Update;
            }
        }
        else
        {
            if (state.mCounter > 0) return result;
            result.mState.mCounter = 0;
        }
        result.mRequestNormalBodyRegistration = context.mHasBodyWrapper && context.mHasNativeBody
            && (context.mPackedFilter & 0x3f) == 8 && context.mHasRememberedWorldOwner;
        return result;
    }

    PhysicalRegistrationGraphPlan preparePhysicalRegistrationGraph(const PhysicalRegistrationGraph& graph,
        std::span<const std::size_t> normalRequests, std::size_t operationLimit)
    {
        if (graph.mBodies.empty() || operationLimit == 0)
            throw std::invalid_argument("Invalid physical registration graph or operation limit");
        for (std::size_t i = 0; i < graph.mBodies.size(); ++i)
        {
            const auto& body = graph.mBodies[i];
            // Validate mapped gains even when the first request would skip
            // this body: a plan admits a complete supported graph value.
            resolvePhysicalBodyRegistration(PhysicalBodyWorldMembership::Absent, body.mBlend);
            if (!body.mBlend && body.mHasRememberedWorldOwner)
                throw std::invalid_argument("Unmapped physical body has a remembered blend owner");
            for (const auto index : body.mAssociatedConstraints)
            {
                if (index >= graph.mConstraints.size())
                    throw std::invalid_argument("Unknown physical constraint association");
                const auto& constraint = graph.mConstraints[index];
                if (constraint.mBodyA != i && constraint.mBodyB != i)
                    throw std::invalid_argument("Unrelated physical constraint association");
            }
        }
        for (const auto& constraint : graph.mConstraints)
            if (constraint.mBodyA >= graph.mBodies.size() || constraint.mBodyB >= graph.mBodies.size()
                || constraint.mBodyA == constraint.mBodyB
                || (constraint.mRegistered && (!graph.mBodies[constraint.mBodyA].mRegistered
                    || !graph.mBodies[constraint.mBodyB].mRegistered)))
                throw std::invalid_argument("Invalid physical registration constraint endpoints or membership");
        for (const auto index : normalRequests)
            if (index >= graph.mBodies.size())
                throw std::invalid_argument("Unknown physical body registration request");

        enum class TaskKind { Normal, Force, Constraint, AfterNormal, AfterForce, Remember };
        struct Task
        {
            TaskKind mKind;
            std::size_t mIndex;
            std::optional<std::size_t> mReturnIndex;
        };
        PhysicalRegistrationGraphPlan result{graph, {}, std::vector<unsigned char>(normalRequests.size(), 0)};
        auto& staged = result.mGraph;
        std::vector<Task> tasks;
        for (std::size_t i = normalRequests.size(); i-- > 0;)
            tasks.push_back({TaskKind::Normal, normalRequests[i], i});
        const auto emit = [&](PhysicalRegistrationEventKind kind, std::size_t index, std::uint32_t filter = 0) {
            result.mEvents.push_back({kind, index, filter});
        };
        const auto addConstraint = [&](std::size_t index) {
            auto& constraint = staged.mConstraints[index];
            if (!constraint.mRegistered && staged.mBodies[constraint.mBodyA].mRegistered
                && staged.mBodies[constraint.mBodyB].mRegistered)
            {
                constraint.mRegistered = true;
                emit(PhysicalRegistrationEventKind::ConstraintAdded, index);
            }
        };
        while (!tasks.empty())
        {
            if (operationLimit-- == 0)
                throw std::length_error("Physical registration operation limit exceeded");
            const auto task = tasks.back();
            tasks.pop_back();
            const auto index = task.mIndex;
            if (task.mKind == TaskKind::Remember)
            {
                staged.mBodies[index].mHasRememberedWorldOwner = true;
                continue;
            }
            if (task.mKind == TaskKind::AfterForce)
            {
                addConstraint(index);
                continue;
            }
            if (task.mKind == TaskKind::AfterNormal)
            {
                const auto b = staged.mConstraints[index].mBodyB;
                if (!staged.mBodies[b].mRegistered)
                {
                    emit(PhysicalRegistrationEventKind::ForcedAdmissionDiagnostic, index);
                    tasks.push_back({TaskKind::AfterForce, index, std::nullopt});
                    tasks.push_back({TaskKind::Force, b, std::nullopt});
                }
                else addConstraint(index);
                continue;
            }
            if (task.mKind == TaskKind::Constraint)
            {
                emit(PhysicalRegistrationEventKind::ConstraintRequest, index);
                const auto& constraint = staged.mConstraints[index];
                if (constraint.mRegistered) continue;
                auto& b = staged.mBodies[constraint.mBodyB];
                if (!b.mRegistered)
                    b.mPackedFilter = (staged.mBodies[constraint.mBodyA].mPackedFilter & 0xffff0000u)
                        | (b.mPackedFilter & 0xffffu);
                tasks.push_back({TaskKind::AfterNormal, index, std::nullopt});
                tasks.push_back({TaskKind::Normal, constraint.mBodyB, std::nullopt});
                continue;
            }
            const bool forced = task.mKind == TaskKind::Force;
            emit(forced ? PhysicalRegistrationEventKind::ForcedBodyRequest
                        : PhysicalRegistrationEventKind::NormalBodyRequest, index);
            auto& body = staged.mBodies[index];
            const auto plan = resolvePhysicalBodyRegistration(body.mRegistered
                    ? PhysicalBodyWorldMembership::Desired : PhysicalBodyWorldMembership::Absent,
                forced ? std::nullopt : body.mBlend);
            if (task.mReturnIndex) result.mReturnsTrue[*task.mReturnIndex] = plan.mReturnsTrue;
            if (plan.mRemoveCurrentWorld)
            {
                emit(PhysicalRegistrationEventKind::BodyRemoved, index);
                for (std::size_t j = 0; j < staged.mConstraints.size(); ++j)
                {
                    auto& constraint = staged.mConstraints[j];
                    if (constraint.mRegistered && (constraint.mBodyA == index || constraint.mBodyB == index))
                    {
                        constraint.mRegistered = false;
                        emit(PhysicalRegistrationEventKind::IncidentConstraintRemoved, j);
                    }
                }
                body.mRegistered = false;
            }
            if (plan.mRememberDesiredWorld)
                tasks.push_back({TaskKind::Remember, index, std::nullopt});
            if (plan.mAddDesiredWorld)
            {
                body.mRegistered = true;
                emit(PhysicalRegistrationEventKind::BodyAdded, index, body.mPackedFilter);
                for (std::size_t i = body.mAssociatedConstraints.size(); i-- > 0;)
                    tasks.push_back({TaskKind::Constraint, body.mAssociatedConstraints[i], std::nullopt});
            }
        }
        return result;
    }
}
