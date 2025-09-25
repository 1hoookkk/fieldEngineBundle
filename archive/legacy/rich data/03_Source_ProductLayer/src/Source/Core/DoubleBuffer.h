#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <cstring>

// RT-safe double buffer for GUI→audio mask transfer
struct FloatMaskDB {
  std::vector<float> buf[2];
  std::atomic<int> active{0};
  int width=0, height=0, stride=0;
  
  void allocate(int w, int h) { 
    width = w; 
    height = h; 
    stride = w;
    for (int i = 0; i < 2; ++i) 
      buf[i].assign((size_t)w*h, 1.0f);
    active.store(0, std::memory_order_release);
  }
  
  float* writePtr() { 
    return buf[1 - active.load(std::memory_order_acquire)].data(); 
  }
  
  void flip() { 
    active.store(1 - active.load(std::memory_order_acquire), std::memory_order_release); 
  }
  
  const float* readPtr() const { 
    return buf[active.load(std::memory_order_acquire)].data(); 
  }
  
  size_t size() const { 
    return (size_t)width * height; 
  }
};