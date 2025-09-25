#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <cstring>

/**
 * OPTIMIZED Command structure for real-time audio processing
 * 
 * Key improvements over original Command structure:
 * - Reduced size from 300+ bytes to exactly 64 bytes
 * - No dynamic memory allocation
 * - Cache-line aligned for optimal performance
 * - Union-based storage for efficient memory usage
 * - Fast type checking with bitfields
 * 
 * @author SpectralCanvas Team
 * @version 2.0 - Production Optimized
 */

// Command type identifiers (8-bit for compact storage)
enum class CommandType : uint8_t
{
    // Core commands
    Test = 0,
    
    // Paint commands (most frequent)
    PaintBeginStroke = 1,
    PaintUpdateStroke = 2,
    PaintEndStroke = 3,
    PaintClearCanvas = 4,
    PaintSetRegion = 5,
    
    // Sample commands
    SampleLoad = 10,
    SamplePlay = 11,
    SampleStop = 12,
    SampleSetParam = 13,
    
    // Synthesis commands  
    SynthSetMode = 20,
    SynthSetParam = 21,
    SynthNoteOn = 22,
    SynthNoteOff = 23,
    
    // Control commands
    ControlSetGain = 30,
    ControlSetTempo = 31,
    ControlSetKey = 32,
    
    // System commands
    SystemReset = 40,
    SystemPanic = 41,
    
    MaxCommandType = 255
};

/**
 * Optimized 64-byte command structure
 * 
 * Memory layout (64 bytes total):
 * - 1 byte: command type
 * - 1 byte: flags
 * - 2 bytes: channel/slot
 * - 4 bytes: timestamp
 * - 56 bytes: parameter union
 */
struct alignas(64) OptimizedCommand  // Cache-line aligned
{
    // Header (8 bytes)
    CommandType type;           // 1 byte
    uint8_t flags;              // 1 byte (priority, etc.)
    uint16_t channel;           // 2 bytes (slot/channel/index)
    uint32_t timestamp;         // 4 bytes (sample position or time)
    
    // Parameter data (56 bytes) - union for efficient storage
    union ParamData
    {
        // Paint stroke data (24 bytes)
        struct PaintData
        {
            float x;
            float y;
            float pressure;
            float velocity;
            uint32_t color;     // ARGB packed
            uint32_t brushId;
        } paint;
        
        // Audio parameters (32 bytes)
        struct AudioData
        {
            float frequency;
            float amplitude;
            float pan;
            float filterCutoff;
            float resonance;
            float attack;
            float decay;
            float sustain;
            float release;
        } audio;
        
        // Generic float array (up to 14 floats)
        float floats[14];
        
        // Generic int array (up to 14 ints)
        int32_t ints[14];
        
        // Short string (55 chars + null terminator)
        char shortString[56];
        
        // Raw bytes
        uint8_t bytes[56];
        
    } params;
    
    // Constructors
    OptimizedCommand() noexcept
    {
        clear();
    }
    
    explicit OptimizedCommand(CommandType t) noexcept
        : type(t), flags(0), channel(0), timestamp(0)
    {
        std::memset(&params, 0, sizeof(params));
    }
    
    // Paint command constructors
    static OptimizedCommand makePaintStroke(float x, float y, float pressure, uint32_t color) noexcept
    {
        OptimizedCommand cmd(CommandType::PaintUpdateStroke);
        cmd.params.paint.x = x;
        cmd.params.paint.y = y;
        cmd.params.paint.pressure = pressure;
        cmd.params.paint.color = color;
        cmd.params.paint.velocity = 0.0f;
        return cmd;
    }
    
    static OptimizedCommand makePaintBegin(float x, float y, uint32_t brushId) noexcept
    {
        OptimizedCommand cmd(CommandType::PaintBeginStroke);
        cmd.params.paint.x = x;
        cmd.params.paint.y = y;
        cmd.params.paint.brushId = brushId;
        return cmd;
    }
    
    static OptimizedCommand makePaintEnd() noexcept
    {
        return OptimizedCommand(CommandType::PaintEndStroke);
    }
    
    // Audio command constructors
    static OptimizedCommand makeNoteOn(uint16_t channel, float freq, float amp) noexcept
    {
        OptimizedCommand cmd(CommandType::SynthNoteOn);
        cmd.channel = channel;
        cmd.params.audio.frequency = freq;
        cmd.params.audio.amplitude = amp;
        return cmd;
    }
    
    static OptimizedCommand makeNoteOff(uint16_t channel) noexcept
    {
        OptimizedCommand cmd(CommandType::SynthNoteOff);
        cmd.channel = channel;
        return cmd;
    }
    
    static OptimizedCommand makeSetParam(uint16_t channel, int paramId, float value) noexcept
    {
        OptimizedCommand cmd(CommandType::SynthSetParam);
        cmd.channel = channel;
        cmd.params.ints[0] = paramId;
        cmd.params.floats[1] = value;
        return cmd;
    }
    
    // Sample command constructors
    static OptimizedCommand makeSampleLoad(uint16_t slot, const char* path) noexcept
    {
        OptimizedCommand cmd(CommandType::SampleLoad);
        cmd.channel = slot;
        
        // Copy path safely (max 55 chars + null)
        if (path)
        {
            strncpy_s(cmd.params.shortString, sizeof(cmd.params.shortString), path, _TRUNCATE);
            cmd.params.shortString[55] = '\0';
        }
        return cmd;
    }
    
    static OptimizedCommand makeSamplePlay(uint16_t slot, float speed = 1.0f) noexcept
    {
        OptimizedCommand cmd(CommandType::SamplePlay);
        cmd.channel = slot;
        cmd.params.floats[0] = speed;
        return cmd;
    }
    
    // System commands
    static OptimizedCommand makeSystemPanic() noexcept
    {
        OptimizedCommand cmd(CommandType::SystemPanic);
        cmd.flags = 0xFF;  // Highest priority
        return cmd;
    }
    
    // Utility methods
    void clear() noexcept
    {
        std::memset(this, 0, sizeof(OptimizedCommand));
    }
    
    bool isPaintCommand() const noexcept
    {
        return type >= CommandType::PaintBeginStroke && 
               type <= CommandType::PaintSetRegion;
    }
    
    bool isSampleCommand() const noexcept
    {
        return type >= CommandType::SampleLoad && 
               type <= CommandType::SampleSetParam;
    }
    
    bool isSynthCommand() const noexcept
    {
        return type >= CommandType::SynthSetMode && 
               type <= CommandType::SynthNoteOff;
    }
    
    bool isSystemCommand() const noexcept
    {
        return type >= CommandType::SystemReset;
    }
    
    // Priority helpers
    void setPriority(uint8_t priority) noexcept
    {
        flags = (flags & 0xF0) | (priority & 0x0F);
    }
    
    uint8_t getPriority() const noexcept
    {
        return flags & 0x0F;
    }
    
    // Timestamp helpers
    void setTimestamp(uint32_t ts) noexcept
    {
        timestamp = ts;
    }
    
    uint32_t getTimestamp() const noexcept
    {
        return timestamp;
    }
    
    // Color helpers for paint commands
    juce::Colour getColor() const noexcept
    {
        return juce::Colour(params.paint.color);
    }
    
    void setColor(juce::Colour color) noexcept
    {
        params.paint.color = color.getARGB();
    }
};

// Verify size at compile time
static_assert(sizeof(OptimizedCommand) == 64, "OptimizedCommand must be exactly 64 bytes");

/**
 * Command pool for pre-allocated command objects
 * Avoids allocation in real-time contexts
 */
class CommandPool
{
public:
    static constexpr size_t POOL_SIZE = 1024;
    
    CommandPool()
    {
        // Pre-allocate all commands
        for (size_t i = 0; i < POOL_SIZE; ++i)
        {
            freeList[i] = static_cast<int>(i);
        }
        freeCount = POOL_SIZE;
    }
    
    /**
     * Get a command from the pool (lock-free)
     */
    OptimizedCommand* acquire() noexcept
    {
        int index = freeCount.fetch_sub(1, std::memory_order_acquire) - 1;
        
        if (index >= 0)
        {
            int slot = freeList[index];
            return &pool[slot];
        }
        
        // Pool exhausted
        freeCount.fetch_add(1, std::memory_order_release);
        return nullptr;
    }
    
    /**
     * Return a command to the pool (lock-free)
     */
    void release(OptimizedCommand* cmd) noexcept
    {
        if (cmd >= &pool[0] && cmd < &pool[POOL_SIZE])
        {
            int slot = static_cast<int>(cmd - &pool[0]);
            int index = freeCount.fetch_add(1, std::memory_order_release);
            
            if (index < static_cast<int>(POOL_SIZE))
            {
                freeList[index] = slot;
                cmd->clear();
            }
            else
            {
                // Pool overflow - should not happen
                freeCount.fetch_sub(1, std::memory_order_release);
            }
        }
    }
    
    /**
     * Get pool statistics
     */
    int getAvailable() const noexcept
    {
        return freeCount.load(std::memory_order_relaxed);
    }
    
    int getUsed() const noexcept
    {
        return POOL_SIZE - getAvailable();
    }
    
private:
    std::array<OptimizedCommand, POOL_SIZE> pool;
    std::array<int, POOL_SIZE> freeList;
    std::atomic<int> freeCount{0};
};