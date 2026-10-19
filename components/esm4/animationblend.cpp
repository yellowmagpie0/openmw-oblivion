#include "animationblend.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        float stored(double value)
        {
            const float result = static_cast<float>(value);
            if (!std::isfinite(result))
                throw std::invalid_argument("nonfinite native sequence blend result");
            return result;
        }

        void nonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native sequence blend weight or threshold");
        }
    }

    std::vector<float> normalizeSequenceBlendWeights(
        std::span<const SequenceBlendWeight> slots, float threshold, bool onlyHighest)
    {
        nonnegative(threshold);
        if (slots.size() > 255)
            throw std::invalid_argument("too many native sequence blend slots");
        for (const auto& slot : slots)
        {
            nonnegative(slot.mWeight);
            nonnegative(slot.mEase);
            if (slot.mEase > 1)
                throw std::invalid_argument("invalid native sequence blend ease");
        }
        std::vector<float> result(slots.size(), 0);
        if (slots.empty()) return result;
        if (slots.size() == 1) return {1};
        const auto priority = [](const auto& slot) { return std::bit_cast<std::int8_t>(slot.mPriority); };
        if (slots.size() == 2)
        {
            // 6CC900 retains these products and the reciprocal as float, but
            // the priority-adjusted products remain on the x87 stack.
            const auto& a = slots[0];
            const auto& b = slots[1];
            double first = stored(double(a.mWeight) * a.mEase);
            double second = stored(double(b.mWeight) * b.mEase);
            if (first == 0 && second == 0) return result;
            if (priority(a) > priority(b))
            {
                if (a.mEase == 1) return {1, 0};
                first *= a.mEase;
                second *= stored(1. - a.mEase);
            }
            else if (priority(a) < priority(b))
            {
                if (b.mEase == 1) return {0, 1};
                first *= stored(1. - b.mEase);
                second *= b.mEase;
            }
            const float reciprocal = stored(1. / (first + second));
            result = {stored(first * reciprocal), stored(second * reciprocal)};
            if (onlyHighest)
                return result[1] > result[0] ? std::vector<float>{0, 1} : std::vector<float>{1, 0};
            if (threshold > 0)
            {
                const bool removedFirst = result[0] < threshold;
                if (removedFirst) result[0] = 0;
                // Original 6CCACA/6CCACD writes the second slot twice. Keep
                // this observed two-slot behavior; the many-slot path differs.
                if (result[1] < threshold) result[1] = 1;
                else if (removedFirst) result[1] = 1;
            }
            return result;
        }

        std::int8_t high = -128, next = -128;
        for (const auto& slot : slots)
        {
            const auto value = priority(slot);
            if (value > high) { next = high; high = value; }
            else if (value < high && value > next) next = value;
        }
        float highSum = 0, nextSum = 0, highEase = 0;
        unsigned highCount = 0;
        for (const auto& slot : slots)
        {
            const float product = stored(double(slot.mWeight) * slot.mEase);
            if (priority(slot) == high)
            {
                highSum = stored(double(highSum) + product);
                highEase = std::max(highEase, slot.mEase);
                ++highCount;
            }
            else if (priority(slot) == next)
                nextSum = stored(double(nextSum) + product);
        }
        if (highCount > 1 && highSum != 0) nextSum = 0;
        const float remaining = stored(1. - highEase);
        const float total = stored(double(highSum) * highEase + double(nextSum) * remaining);
        const float reciprocal = total > 0 ? stored(1. / total) : 0;
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const auto& slot = slots[i];
            if (priority(slot) == high)
                result[i] = stored(double(slot.mWeight) * highEase * slot.mEase * reciprocal);
            else if (priority(slot) == next && nextSum != 0)
                result[i] = stored(double(slot.mWeight) * remaining * slot.mEase * reciprocal);
        }
        if (threshold > 0)
        {
            float sum = 0;
            for (auto& value : result)
            {
                if (value < threshold) value = 0;
                sum = stored(double(sum) + value);
            }
            if (sum != 1 && sum > 0)
            {
                const float scale = stored(1. / sum);
                for (auto& value : result) if (value != 0) value = stored(double(value) * scale);
            }
        }
        if (onlyHighest)
        {
            const auto chosen = std::max_element(result.begin(), result.end()) - result.begin();
            std::fill(result.begin(), result.end(), 0);
            result[chosen] = 1;
        }
        return result;
    }

    SequenceBlendPose blendSequenceTransforms(
        std::span<const SequenceBlendPose> poses, std::span<const float> weights)
    {
        if (poses.size() != weights.size() || poses.size() > 255)
            throw std::invalid_argument("mismatched native sequence transform slots");
        for (std::size_t i = 0; i < poses.size(); ++i)
        {
            nonnegative(weights[i]);
            const auto& pose = poses[i];
            if (pose.mTranslation) for (float value : *pose.mTranslation) (void)stored(value);
            if (pose.mRotation)
            {
                double length = 0;
                for (float value : *pose.mRotation) { (void)stored(value); length += double(value) * value; }
                if (length == 0) throw std::invalid_argument("zero native sequence quaternion");
            }
            if (pose.mScale) (void)stored(*pose.mScale);
        }
        if (poses.empty()) return {};
        if (poses.size() == 1) return poses.front();
        std::array<float, 3> translation{};
        std::array<float, 4> rotation{};
        float translationWeight = 1, scaleWeight = 1, scale = 0;
        bool hasTranslation = false, hasRotation = false, hasScale = false;
        for (std::size_t i = 0; i < poses.size(); ++i)
        {
            const float weight = weights[i];
            if (weight == 0) continue;
            const auto& pose = poses[i];
            if (!pose.mTranslation && !pose.mRotation && !pose.mScale)
            {
                translationWeight = stored(double(translationWeight) - weight);
                scaleWeight = stored(double(scaleWeight) - weight);
                continue;
            }
            if (pose.mTranslation)
            {
                hasTranslation = true;
                for (unsigned j = 0; j < 3; ++j)
                    translation[j] = stored(double(translation[j]) + stored(double((*pose.mTranslation)[j]) * weight));
            }
            else translationWeight = stored(double(translationWeight) - weight);
            if (pose.mRotation)
            {
                auto value = *pose.mRotation;
                const float dot = stored(double(value[1]) * rotation[1] + double(value[0]) * rotation[0]
                    + double(value[2]) * rotation[2] + double(value[3]) * rotation[3]);
                if (hasRotation && dot < 0) for (auto& component : value) component = -component;
                hasRotation = true;
                for (unsigned j = 0; j < 4; ++j)
                    rotation[j] = stored(double(rotation[j]) + stored(double(value[j]) * weight));
            }
            if (pose.mScale)
            {
                hasScale = true;
                scale = stored(double(*pose.mScale) * weight + scale);
            }
            else scaleWeight = stored(double(scaleWeight) - weight);
        }
        SequenceBlendPose result;
        if (hasTranslation && translationWeight != 0)
        {
            const float factor = stored(1. / translationWeight);
            for (auto& value : translation) value = stored(double(value) * factor);
            result.mTranslation = translation;
        }
        if (hasRotation)
        {
            const float square = stored(double(rotation[1]) * rotation[1] + double(rotation[0]) * rotation[0]
                + double(rotation[2]) * rotation[2] + double(rotation[3]) * rotation[3]);
            const float length = stored(std::sqrt(double(square)));
            const float reciprocal = stored(1. / length);
            for (auto& value : rotation) value = stored(double(value) * reciprocal);
            result.mRotation = rotation;
        }
        if (hasScale && scaleWeight >= .0001)
            result.mScale = stored(double(scale) / scaleWeight);
        return result;
    }
}
