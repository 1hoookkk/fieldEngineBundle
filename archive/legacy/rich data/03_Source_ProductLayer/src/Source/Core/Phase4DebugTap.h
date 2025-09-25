#pragma once
#include <atomic>

// Enable/disable all debug tap code at compile time
#ifndef PHASE4_DEBUG_TAP
#define PHASE4_DEBUG_TAP 1
#endif

struct Phase4DebugTap
{
    std::atomic<uint64_t> pushes{0}, pushFails{0}, pops{0};
    std::atomic<uint64_t> lastSeqPushed{0}, lastSeqPopped{0};

    // Identify queue/processor instances across threads
    std::atomic<uintptr_t> queueAddrOnPush{0};
    std::atomic<uintptr_t> queueAddrOnAudio{0};
    std::atomic<uint64_t>  procId{0};

    // Optional head/tail snapshots (if queue exposes)
    std::atomic<uint64_t> producerHead{0}, producerTail{0};
    std::atomic<uint64_t> consumerHead{0}, consumerTailSeen{0};
};