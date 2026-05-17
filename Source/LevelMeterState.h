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
        outputPeak.store (0.0f, std::memory_order_relaxed);
        inputClipped.store (false, std::memory_order_relaxed);
        outputClipped.store (false, std::memory_order_relaxed);
    }

    void publish (float newInputPeak, float newOutputPeak) noexcept
    {
        inputPeak.store (newInputPeak, std::memory_order_relaxed);
        outputPeak.store (newOutputPeak, std::memory_order_relaxed);

        if (newInputPeak >= clipThreshold)
            inputClipped.store (true, std::memory_order_relaxed);

        if (newOutputPeak >= clipThreshold)
            outputClipped.store (true, std::memory_order_relaxed);
    }

    float getInputPeak() const noexcept
    {
        return inputPeak.load (std::memory_order_relaxed);
    }

    float getOutputPeak() const noexcept
    {
        return outputPeak.load (std::memory_order_relaxed);
    }

    bool getInputClipped() const noexcept
    {
        return inputClipped.load (std::memory_order_relaxed);
    }

    bool getOutputClipped() const noexcept
    {
        return outputClipped.load (std::memory_order_relaxed);
    }

    void clearClipFlags() noexcept
    {
        inputClipped.store (false, std::memory_order_relaxed);
        outputClipped.store (false, std::memory_order_relaxed);
    }

private:
    static constexpr float clipThreshold = 0.999f;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool> inputClipped { false };
    std::atomic<bool> outputClipped { false };
};

} // namespace voxchord

