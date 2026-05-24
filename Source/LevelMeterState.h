#pragma once

#include <atomic>

namespace voxchord
{
class LevelMeterState final
{
public:
    void reset() noexcept
    {
        inputPeak.store (0.0f, std::memory_order_relaxed);
        outputLeftPeak.store (0.0f, std::memory_order_relaxed);
        outputRightPeak.store (0.0f, std::memory_order_relaxed);
        inputClipped.store (false, std::memory_order_relaxed);
        outputLeftClipped.store (false, std::memory_order_relaxed);
        outputRightClipped.store (false, std::memory_order_relaxed);
    }

    void publish (float newInputPeak, float newOutputLeftPeak, float newOutputRightPeak) noexcept
    {
        inputPeak.store (newInputPeak, std::memory_order_relaxed);
        outputLeftPeak.store (newOutputLeftPeak, std::memory_order_relaxed);
        outputRightPeak.store (newOutputRightPeak, std::memory_order_relaxed);

        if (newInputPeak >= clipThreshold)
            inputClipped.store (true, std::memory_order_relaxed);

        if (newOutputLeftPeak >= clipThreshold)
            outputLeftClipped.store (true, std::memory_order_relaxed);

        if (newOutputRightPeak >= clipThreshold)
            outputRightClipped.store (true, std::memory_order_relaxed);
    }

    float getInputPeak() const noexcept
    {
        return inputPeak.load (std::memory_order_relaxed);
    }

    float getOutputPeak() const noexcept
    {
        const auto left = getOutputLeftPeak();
        const auto right = getOutputRightPeak();
        return left > right ? left : right;
    }

    float getOutputLeftPeak() const noexcept
    {
        return outputLeftPeak.load (std::memory_order_relaxed);
    }

    float getOutputRightPeak() const noexcept
    {
        return outputRightPeak.load (std::memory_order_relaxed);
    }

    bool getInputClipped() const noexcept
    {
        return inputClipped.load (std::memory_order_relaxed);
    }

    bool getOutputClipped() const noexcept
    {
        return getOutputLeftClipped() || getOutputRightClipped();
    }

    bool getOutputLeftClipped() const noexcept
    {
        return outputLeftClipped.load (std::memory_order_relaxed);
    }

    bool getOutputRightClipped() const noexcept
    {
        return outputRightClipped.load (std::memory_order_relaxed);
    }

    void clearClipFlags() noexcept
    {
        inputClipped.store (false, std::memory_order_relaxed);
        outputLeftClipped.store (false, std::memory_order_relaxed);
        outputRightClipped.store (false, std::memory_order_relaxed);
    }

private:
    static constexpr float clipThreshold = 0.999f;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputLeftPeak { 0.0f };
    std::atomic<float> outputRightPeak { 0.0f };
    std::atomic<bool> inputClipped { false };
    std::atomic<bool> outputLeftClipped { false };
    std::atomic<bool> outputRightClipped { false };
};

} // namespace voxchord
