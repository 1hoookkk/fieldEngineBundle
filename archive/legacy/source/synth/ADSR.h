#pragma once

#include <juce_core/juce_core.h>

class ADSR
{
public:
    ADSR() = default;

    void setSampleRate(double rate) { sampleRate = rate; }

    void setParameters(float attack, float decay, float sustain, float release)
    {
        attackRate = 1.0f / (attack * (float)sampleRate);
        decayRate = 1.0f / (decay * (float)sampleRate);
        sustainLevel = sustain;
        releaseRate = 1.0f / (release * (float)sampleRate);
    }

    void noteOn()
    {
        stage = Stage::Attack;
        level = 0.0f;
    }

    void noteOff()
    {
        stage = Stage::Release;
    }

    float getNextSample()
    {
        switch (stage)
        {
            case Stage::Idle:
                break;
            case Stage::Attack:
                level += attackRate;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                level -= decayRate;
                if (level <= sustainLevel)
                {
                    level = sustainLevel;
                    stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                break;
            case Stage::Release:
                level -= releaseRate;
                if (level <= 0.0f)
                {
                    level = 0.0f;
                    stage = Stage::Idle;
                }
                break;
        }
        return level;
    }

    bool isActive() const { return stage != Stage::Idle; }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;

    double sampleRate = 44100.0;
    float level = 0.0f;

    float attackRate = 0.0f;
    float decayRate = 0.0f;
    float sustainLevel = 1.0f;
    float releaseRate = 0.0f;
};
