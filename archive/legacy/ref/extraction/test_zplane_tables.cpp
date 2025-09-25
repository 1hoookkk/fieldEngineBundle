#include "fieldEngine/Source/Core/ZPlaneTables_T1.h"
#include "fieldEngine/Source/Core/ZPlaneTables_T2.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "Z-Plane Table Integration Test\n";
    std::cout << "==============================\n\n";

    // Test T1 table (frequency mapping)
    std::cout << "T1 TABLE (Frequency Mapping):\n";
    std::cout << "T1[0.0] = " << ZPlaneTables::T1_TABLE_lookup(0.0f) << " Hz\n";
    std::cout << "T1[0.5] = " << ZPlaneTables::T1_TABLE_lookup(0.5f) << " Hz\n";
    std::cout << "T1[1.0] = " << ZPlaneTables::T1_TABLE_lookup(1.0f) << " Hz\n";

    // Test T2 table (resonance mapping)
    std::cout << "\nT2 TABLE (Resonance Mapping):\n";
    std::cout << "T2[0.0] = " << ZPlaneTables::T2_TABLE_lookup(0.0f) << " Q\n";
    std::cout << "T2[0.5] = " << ZPlaneTables::T2_TABLE_lookup(0.5f) << " Q\n";
    std::cout << "T2[1.0] = " << ZPlaneTables::T2_TABLE_lookup(1.0f) << " Q\n";

    // Test interpolation at various points
    std::cout << "\nInterpolation Test:\n";
    std::cout << std::fixed << std::setprecision(2);
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        float freq = ZPlaneTables::T1_TABLE_lookup(t);
        float q = ZPlaneTables::T2_TABLE_lookup(t);
        std::cout << "t=" << t << " -> " << freq << " Hz, Q=" << q << "\n";
    }

    std::cout << "\n✅ Z-Plane table integration test completed!\n";
    return 0;
}