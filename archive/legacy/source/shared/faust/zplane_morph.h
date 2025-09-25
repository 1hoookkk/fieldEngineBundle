#pragma once

// Minimal FAUST stub for zplane_morph DSP
// This is a placeholder to allow compilation while we transition to the new modular EMU engine

class mydsp {
public:
    mydsp() {}
    virtual ~mydsp() {}

    virtual void instanceInit(int sample_rate) {}
    virtual void init(int sample_rate) { instanceInit(sample_rate); }
    virtual void buildUserInterface(void* ui_interface) {}
    virtual int getNumInputs() { return 2; }
    virtual int getNumOutputs() { return 2; }
    virtual void compute(int count, float** inputs, float** outputs) {
        // Passthrough implementation
        for (int c = 0; c < getNumOutputs() && c < getNumInputs(); ++c) {
            for (int i = 0; i < count; ++i) {
                outputs[c][i] = inputs[c][i];
            }
        }
    }

    // Parameter interface
    virtual void setParamValue(int param, float value) {}
    virtual float getParamValue(int param) { return 0.0f; }
};