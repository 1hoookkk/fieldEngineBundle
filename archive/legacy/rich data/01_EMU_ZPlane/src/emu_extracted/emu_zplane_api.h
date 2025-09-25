// emu_zplane_api.h  -- single-file entry point (LLM-friendly)
#pragma once
#ifdef _WIN32
  #ifdef EMU_EXPORTS
    #define EMU_API __declspec(dllexport)
  #else
    #define EMU_API __declspec(dllimport)
  #endif
#else
  #define EMU_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// opaque handle
typedef void* EMU_HANDLE;

// create/destroy
EMU_API EMU_HANDLE emu_create(double sampleRate, int maxBlockSize);
EMU_API void       emu_destroy(EMU_HANDLE h);

// shapes: 12 floats = [r0,theta0, r1,theta1, ... r5,theta5]; r in (0..1), theta in radians
EMU_API void emu_set_shapeA(EMU_HANDLE h, const float rtheta12[12]);
EMU_API void emu_set_shapeB(EMU_HANDLE h, const float rtheta12[12]);

// params (units documented)
EMU_API void emu_set_morph(EMU_HANDLE h, float morph01);        // 0..1
EMU_API void emu_set_intensity(EMU_HANDLE h, float intensity01);// 0..1 (maps to radius/Q)
EMU_API void emu_set_drive_db(EMU_HANDLE h, float db);          // dB input drive
EMU_API void emu_set_saturation(EMU_HANDLE h, float sat01);     // 0..1
EMU_API void emu_set_auto_makeup(EMU_HANDLE h, int enabled);    // 0 or 1

// process: separate buffers (no allocations) -- caller owns buffers
// left[], right[] length = numFrames
EMU_API void emu_process_separate(EMU_HANDLE h, float* left, float* right, int numFrames);

#ifdef __cplusplus
}
#endif