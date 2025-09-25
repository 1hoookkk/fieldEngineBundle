#pragma once

#include <JuceHeader.h>
#include <vector>

namespace FieldEngineFX::UI {

class ModulationMatrix : public juce::Component {
public:
    ModulationMatrix();
    
    // Sources and destinations
    void addModSource(const juce::String& name, float* valuePtr);
    void addModDestination(const juce::String& name, float* valuePtr);
    
    // Routing
    void setModAmount(int sourceIndex, int destIndex, float amount);
    float getModAmount(int sourceIndex, int destIndex) const;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
private:
    struct ModSource {
        juce::String name;
        float* value;
        juce::Point<float> position;
    };
    
    struct ModDestination {
        juce::String name;
        float* value;
        juce::Point<float> position;
    };
    
    struct ModConnection {
        int sourceIndex;
        int destIndex;
        float amount;
        juce::Path connectionPath;
    };
    
    std::vector<ModSource> sources;
    std::vector<ModDestination> destinations;
    std::vector<ModConnection> connections;
    
    ModConnection* selectedConnection = nullptr;
    bool isDragging = false;
    
    void drawSources(juce::Graphics& g);
    void drawDestinations(juce::Graphics& g);
    void drawConnections(juce::Graphics& g);
    void updateConnectionPaths();
    
    ModConnection* getConnectionAt(juce::Point<float> pos);
};

} // namespace FieldEngineFX::UI
