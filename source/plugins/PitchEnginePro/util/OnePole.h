#pragma once
struct OnePole { double a=0.9, z=0.0; void setMs(double sr, double ms){ a = std::exp(-1.0/(sr*(ms/1000.0))); } double step(double x){ z = a*z + (1.0-a)*x; return z; } };