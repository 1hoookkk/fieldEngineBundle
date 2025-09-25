#pragma once

// Runtime build verification - this will prove which code is actually running
#define SC_BUILD_DATETIME __DATE__ " " __TIME__
#define SC_BUILD_CONFIG "MVP_UI_ENABLED"
#define SC_BUILD_HASH "YOLO_SPRINT_" __TIME__

// Compile-time assertions to verify our build flags
#ifndef SC_MVP_UI
    #warning "SC_MVP_UI not defined - this might explain the missing changes!"
#endif