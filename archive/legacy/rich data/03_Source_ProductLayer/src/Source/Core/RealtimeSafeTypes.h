#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// RT-safe assertions for debug builds
#ifndef RT_SAFE_ASSERT
    #ifdef JUCE_DEBUG
        #include <thread>
        #define RT_SAFE_ASSERT(condition) juce_assert(condition)
        // No RT logging - use atomic counters instead
        #define RT_SAFE_LOG(message) ((void)0)
        
        // Thread ID validation for RT paths
        extern std::thread::id g_audioThreadId;
        #define RT_ASSERT_AUDIO_THREAD() RT_SAFE_ASSERT(std::this_thread::get_id() == g_audioThreadId)
    #else
        #define RT_SAFE_ASSERT(condition) ((void)0)
        #define RT_SAFE_LOG(message) ((void)0)
        #define RT_ASSERT_AUDIO_THREAD() ((void)0)
    #endif
#endif

// Static assertions for RT-safe types
template<typename T>
constexpr bool is_rt_safe_v = std::is_trivially_copyable_v<T> && 
                              std::is_standard_layout_v<T> && 
                              !std::is_pointer_v<T>;

#define RT_SAFE_TYPE_ASSERT(T) static_assert(is_rt_safe_v<T>, #T " must be RT-safe (trivially copyable, standard layout, non-pointer)")

// Lock-free atomic validation
template<typename T>
constexpr bool is_lock_free_atomic_v = std::atomic<T>::is_always_lock_free;

#define LOCK_FREE_ATOMIC_ASSERT(T) static_assert(is_lock_free_atomic_v<T>, #T " atomic must be lock-free for RT safety")

// Cache-aligned storage for optimal performance
#if _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4324)
#endif

template<typename T, size_t Alignment = 64>
struct alignas(Alignment) AlignedStorage
{
    T data{};

    AlignedStorage() noexcept = default;

    template <typename... Args>
    explicit AlignedStorage(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : data(std::forward<Args>(args)...) {}

    AlignedStorage(const AlignedStorage&) = delete;
    AlignedStorage& operator=(const AlignedStorage&) = delete;
    AlignedStorage(AlignedStorage&&) = delete;
    AlignedStorage& operator=(AlignedStorage&&) = delete;

    T&       get()       noexcept { return data; }
    const T& get() const noexcept { return data; }

    operator T&() noexcept { return data; }
    operator const T&() const noexcept { return data; }
};

#if _MSC_VER
  #pragma warning(pop)
#endif

// RT-safe circular buffer with atomic indices
#if _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4324)
#endif

template<typename T, size_t Size>
class RTCircularBuffer
{
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2 for RT-safe operation");
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

public:
    RTCircularBuffer() noexcept : writeIndex_(0), readIndex_(0) 
    {
        buffer_.get().fill(T{});
    }
    
    // Write single sample (producer thread only)
    void write(const T& sample) noexcept
    {
        const size_t currentWrite = writeIndex_.get().load(std::memory_order_relaxed);
        buffer_.get()[currentWrite & (Size - 1)] = sample;
        writeIndex_.get().store(currentWrite + 1, std::memory_order_release);
    }
    
    // Write block of samples (producer thread only)
    void writeBlock(const T* samples, size_t numSamples) noexcept
    {
        const size_t currentWrite = writeIndex_.get().load(std::memory_order_relaxed);
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            buffer_.get()[(currentWrite + i) & (Size - 1)] = samples[i];
        }
        
        writeIndex_.get().store(currentWrite + numSamples, std::memory_order_release);
    }
    
    // Read single sample (consumer thread only)
    T read() noexcept
    {
        const size_t currentRead = readIndex_.get().load(std::memory_order_relaxed);
        const T sample = buffer_.get()[currentRead & (Size - 1)];
        readIndex_.get().store(currentRead + 1, std::memory_order_release);
        return sample;
    }
    
    // Read block of samples (consumer thread only)
    void readBlock(T* samples, size_t numSamples) noexcept
    {
        const size_t currentRead = readIndex_.get().load(std::memory_order_relaxed);
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            samples[i] = buffer_.get()[(currentRead + i) & (Size - 1)];
        }
        
        readIndex_.get().store(currentRead + numSamples, std::memory_order_release);
    }
    
    // Get available samples for reading
    size_t getAvailableForReading() const noexcept
    {
        const size_t write = writeIndex_.get().load(std::memory_order_acquire);
        const size_t read = readIndex_.get().load(std::memory_order_relaxed);
        return write - read;
    }
    
    // Get available space for writing  
    size_t getAvailableForWriting() const noexcept
    {
        const size_t write = writeIndex_.get().load(std::memory_order_relaxed);
        const size_t read = readIndex_.get().load(std::memory_order_acquire);
        return Size - (write - read);
    }

private:
    AlignedStorage<std::array<T, Size>> buffer_;
    AlignedStorage<std::atomic<size_t>> writeIndex_;
    AlignedStorage<std::atomic<size_t>> readIndex_;
};

#if _MSC_VER
  #pragma warning(pop)
#endif

// RT-safe windowing functions (pre-computed at initialization)
template<size_t WindowSize>
class RTWindow
{
public:
    enum Type { Hann, Hamming, Blackman, Rectangular };
    
    explicit RTWindow(Type type = Hann) noexcept
    {
        generateWindow(type);
    }
    
    // Apply window to buffer (RT-safe)
    void apply(float* buffer) const noexcept
    {
        for (size_t i = 0; i < WindowSize; ++i)
        {
            buffer[i] *= window_[i];
        }
    }
    
    // Get window coefficient (RT-safe)
    float operator[](size_t index) const noexcept
    {
        RT_SAFE_ASSERT(index < WindowSize);
        return window_[index];
    }
    
    static constexpr size_t size() noexcept { return WindowSize; }

private:
    void generateWindow(Type type) noexcept
    {
        const float N = static_cast<float>(WindowSize - 1);
        
        switch (type)
        {
            case Hann:
                for (size_t n = 0; n < WindowSize; ++n)
                {
                    const float factor = static_cast<float>(n) / N;
                    window_[n] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * factor));
                }
                break;
                
            case Hamming:
                for (size_t n = 0; n < WindowSize; ++n)
                {
                    const float factor = static_cast<float>(n) / N;
                    window_[n] = 0.54f - 0.46f * std::cos(2.0f * static_cast<float>(M_PI) * factor);
                }
                break;
                
            case Blackman:
                for (size_t n = 0; n < WindowSize; ++n)
                {
                    const float factor = static_cast<float>(n) / N;
                    window_[n] = 0.42f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * factor) 
                               + 0.08f * std::cos(4.0f * static_cast<float>(M_PI) * factor);
                }
                break;
                
            case Rectangular:
            default:
                window_.fill(1.0f);
                break;
        }
    }
    
    std::array<float, WindowSize> window_;
};

// RT-safe timing utilities
class RTTimer
{
public:
    RTTimer() noexcept : startTime_(0), sampleRate_(44100.0) {}
    
    void setSampleRate(double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
    }
    
    void reset(double currentTimeInSamples) noexcept
    {
        startTime_.store(currentTimeInSamples, std::memory_order_relaxed);
    }
    
    double getElapsedSamples(double currentTimeInSamples) const noexcept
    {
        return currentTimeInSamples - startTime_.load(std::memory_order_relaxed);
    }
    
    double getElapsedSeconds(double currentTimeInSamples) const noexcept
    {
        return getElapsedSamples(currentTimeInSamples) / sampleRate_;
    }

private:
    std::atomic<double> startTime_;
    double sampleRate_;
};

// RT-safe sequence number generator
class RTSequenceGenerator
{
public:
    RTSequenceGenerator() noexcept : sequence_(0) {}
    
    uint32_t getNext() noexcept
    {
        return sequence_.fetch_add(1, std::memory_order_acq_rel);
    }
    
    uint32_t getCurrent() const noexcept
    {
        return sequence_.load(std::memory_order_acquire);
    }
    
    void reset() noexcept
    {
        sequence_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> sequence_;
};