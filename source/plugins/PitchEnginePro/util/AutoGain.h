#pragma once
struct AutoGain { void reset(double){} float compute(float in, float target){ (void)target; return in; } };