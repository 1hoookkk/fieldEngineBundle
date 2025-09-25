#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>

/**
 * Hardware Controller Manager - Revolutionary Tactile Interface System
 * 
 * Enables revolutionary hardware integration including:
 * - Custom paint surface controllers with pressure sensitivity
 * - Gesture recognition for air-painting without touching
 * - Haptic feedback that lets you FEEL the audio
 * - Multi-device coordination for complex setups
 * - Support for existing controllers (Push, Maschine, etc.)
 * 
 * This is what makes SPECTRAL CANVAS PRO feel like the future!
 */
class HardwareControllerManager
{
public:
    HardwareControllerManager();
    ~HardwareControllerManager();
    
    //==============================================================================
    // Core Hardware System
    
    void initialize();
    void shutdown();
    void scanForDevices();
    void updateHardware();
    
    //==============================================================================
    // Revolutionary Paint Surface Controllers
    
    enum class PaintSurfaceType
    {
        SpectralCanvas_Pro,      // Our custom hardware surface
        SpectralCanvas_Studio,   // Larger studio version
        iPad_Pro,                // iPad with Apple Pencil
        Wacom_Tablet,           // Wacom drawing tablets
        Surface_Studio,          // Microsoft Surface Studio
        Custom_Tablet,          // Generic tablet/touchscreen
        Multi_Touch_Display     // Multi-touch displays
    };
    
    struct PaintSurfaceInfo
    {
        PaintSurfaceType type;
        juce::String deviceName;
        juce::String serialNumber;
        bool isConnected = false;
        bool supportsPressure = false;
        bool supportsTilt = false;
        bool supportsHover = false;
        bool supportsMultiTouch = false;
        
        // Surface capabilities
        juce::Rectangle<float> activeArea;     // Physical dimensions in mm
        float maxPressure = 1.0f;
        float pressureResolution = 0.001f;
        int maxTouchPoints = 1;
        float updateRate = 120.0f;            // Hz
        
        // Calibration data
        juce::AffineTransform calibrationTransform;
        bool needsCalibration = true;
    };
    
    // Paint surface management
    std::vector<PaintSurfaceInfo> getConnectedPaintSurfaces() const;
    bool connectPaintSurface(const juce::String& deviceId);
    void calibratePaintSurface(const juce::String& deviceId);
    void setPrimaryPaintSurface(const juce::String& deviceId);
    
    //==============================================================================
    // Touch & Pressure Data
    
    struct TouchPoint
    {
        int touchId = 0;
        juce::Point<float> position;       // Normalized 0.0-1.0
        juce::Point<float> rawPosition;    // Raw device coordinates
        float pressure = 1.0f;             // 0.0-1.0
        float tiltX = 0.0f;               // Pen tilt X (-1.0 to 1.0)
        float tiltY = 0.0f;               // Pen tilt Y (-1.0 to 1.0)
        float twist = 0.0f;               // Pen rotation (0.0-1.0)
        float velocity = 0.0f;            // Movement speed
        juce::uint32 timestamp = 0;
        
        // Touch state
        enum class State { Began, Moved, Ended, Cancelled } state = State::Began;
        bool isHovering = false;          // Pen hovering above surface
        bool isPen = false;               // Is this a pen/stylus?
        bool isEraser = false;            // Is eraser end of pen?
        
        TouchPoint() : timestamp(juce::Time::getMillisecondCounter()) {}
    };
    
    // Touch event callbacks
    std::function<void(const TouchPoint&)> onTouchBegan;
    std::function<void(const TouchPoint&)> onTouchMoved;
    std::function<void(const TouchPoint&)> onTouchEnded;
    std::function<void(const TouchPoint&)> onTouchHover;
    
    //==============================================================================
    // Gesture Recognition Engine
    
    enum class GestureType
    {
        Paint,              // Basic paint stroke
        Erase,              // Erasing gesture
        Pinch,              // Zoom in/out
        Pan,                // Pan/scroll
        Rotate,             // Rotation gesture
        Tap,                // Single tap
        DoubleTap,          // Double tap
        LongPress,          // Long press
        Swipe,              // Swipe in direction
        Circle,             // Circular gesture
        AirPaint,           // Air painting (no touch)
        WaveHand,           // Hand wave gesture
        PointAndDraw,       // Point and draw in air
        GrabAndManipulate   // Grab gesture
    };
    
    struct Gesture
    {
        GestureType type;
        juce::Point<float> startPosition;
        juce::Point<float> currentPosition;
        juce::Point<float> endPosition;
        float scale = 1.0f;               // For pinch gestures
        float rotation = 0.0f;            // For rotation gestures
        float velocity = 0.0f;            // Gesture velocity
        float pressure = 1.0f;            // Average pressure
        juce::uint32 duration = 0;        // Gesture duration in ms
        std::vector<TouchPoint> points;   // All touch points in gesture
        
        bool isActive = false;
        bool isComplete = false;
    };
    
    // Gesture recognition
    void enableGestureRecognition(bool enable) { gestureRecognitionEnabled.store(enable); }
    void setGestureSensitivity(float sensitivity) { gestureSensitivity.store(sensitivity); }
    std::vector<Gesture> getCurrentGestures() const;
    
    // Gesture callbacks
    std::function<void(const Gesture&)> onGestureRecognized;
    std::function<void(const Gesture&)> onGestureUpdated;
    std::function<void(const Gesture&)> onGestureCompleted;
    
    //==============================================================================
    // Air Gesture Recognition (Camera-based)
    
    struct HandPose
    {
        juce::Point<float> palmPosition;
        std::array<juce::Point<float>, 5> fingerTips;  // Thumb to pinky
        std::array<juce::Point<float>, 4> fingerJoints; // Knuckles
        float palmSize = 0.0f;
        float confidence = 0.0f;          // 0.0-1.0
        bool isVisible = false;
        bool isLeftHand = false;
        
        // Gesture state
        bool isPinching = false;
        bool isPointing = false;
        bool isFist = false;
        bool isOpen = false;
        float pinchStrength = 0.0f;       // 0.0-1.0
    };
    
    // Air gesture system
    void enableAirGestures(bool enable) { airGesturesEnabled.store(enable); }
    void calibrateAirGestureSpace();
    std::vector<HandPose> getCurrentHandPoses() const;
    
    // Air gesture callbacks
    std::function<void(const HandPose&)> onHandDetected;
    std::function<void(const HandPose&)> onHandLost;
    std::function<void(const HandPose&, const HandPose&)> onAirPaintGesture;
    
    //==============================================================================
    // Haptic Feedback Engine
    
    enum class HapticPattern
    {
        Click,              // Sharp click
        Thud,               // Deep thud
        Buzz,               // Continuous buzz
        Pulse,              // Rhythmic pulse
        Sweep,              // Frequency sweep
        Impact,             // Sharp impact
        Rumble,             // Low rumble
        Custom              // Custom pattern
    };
    
    struct HapticEvent
    {
        HapticPattern pattern;
        float intensity = 1.0f;           // 0.0-1.0
        float duration = 0.1f;            // Seconds
        float frequency = 200.0f;         // Hz for buzz/pulse
        juce::Point<float> location;      // Where on surface (if applicable)
        
        // Audio-reactive haptics
        bool followAudio = false;
        float audioFrequency = 440.0f;    // Frequency to follow
        float audioThreshold = 0.1f;      // Trigger threshold
    };
    
    // Haptic feedback
    void enableHapticFeedback(bool enable) { hapticFeedbackEnabled.store(enable); }
    void setHapticIntensity(float intensity) { hapticIntensity.store(intensity); }
    void triggerHapticEvent(const HapticEvent& event);
    void setAudioReactiveHaptics(bool enable, float sensitivity = 0.5f);
    
    //==============================================================================
    // Traditional MIDI Controllers
    
    enum class ControllerType
    {
        Unknown,
        AbletonPush,
        AbletonPush2,
        NativeMaschine,
        NovationLaunchpad,
        AkaiMPD,
        KorgNanoPad,
        Custom
    };
    
    struct MIDIControllerInfo
    {
        ControllerType type;
        juce::String name;
        juce::String identifier;
        bool isInput = false;
        bool isOutput = false;
        bool isConnected = false;
        
        // Controller-specific features
        bool hasDisplay = false;
        bool hasRGB = false;
        bool hasPressureSensitive = false;
        int numPads = 0;
        int numKnobs = 0;
        int numSliders = 0;
    };
    
    // MIDI controller management
    std::vector<MIDIControllerInfo> getConnectedMIDIControllers() const;
    void mapMIDIController(const juce::String& controllerId);
    void createCustomMIDIMapping(const juce::String& controllerId);
    
    //==============================================================================
    // Multi-Device Coordination
    
    struct DeviceRole
    {
        juce::String deviceId;
        enum class Role { 
            Primary,          // Main painting surface
            Secondary,        // Additional painting surface
            Transport,        // Transport controls
            Mixer,            // Volume/effects control
            Visualizer,       // Display only
            AirController     // Air gesture controller
        } role = Role::Primary;
        
        bool isActive = false;
        juce::Rectangle<float> assignedArea; // Portion of canvas
    };
    
    // Multi-device setup
    void createMultiDeviceSetup(const std::vector<DeviceRole>& setup);
    void synchronizeDevices();
    void setDeviceRole(const juce::String& deviceId, DeviceRole::Role role);
    
    //==============================================================================
    // Controller Mapping & Customization
    
    struct ControlMapping
    {
        juce::String controllerId;
        juce::String controlName;         // "Knob 1", "Pad A1", etc.
        
        enum class Target {
            PaintBrushSize,
            PaintBrushIntensity,
            MaskingMode,
            TrackerTempo,
            EffectParameter,
            VolumeLevel,
            PanPosition,
            CustomParameter
        } target = Target::PaintBrushSize;
        
        juce::String targetParameter;     // Specific parameter name
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float curve = 1.0f;              // Response curve (1.0 = linear)
        bool isToggle = false;
    };
    
    // Mapping management
    void createControlMapping(const ControlMapping& mapping);
    void removeControlMapping(const juce::String& controllerId, const juce::String& controlName);
    void saveControllerPreset(const juce::String& presetName);
    void loadControllerPreset(const juce::String& presetName);
    
private:
    //==============================================================================
    // Hardware Detection & Communication
    
    class DeviceScanner
    {
    public:
        void scanForPaintSurfaces();
        void scanForMIDIControllers();
        void scanForCameras();
        void scanForHapticDevices();
        
        std::vector<PaintSurfaceInfo> foundPaintSurfaces;
        std::vector<MIDIControllerInfo> foundMIDIControllers;
        
    private:
        void detectWacomTablets();
        void detectIPads();
        void detectSurfaceStudio();
        void detectCustomControllers();
    } deviceScanner;
    
    //==============================================================================
    // Gesture Recognition Implementation
    
    class GestureRecognizer
    {
    public:
        void processTouch(const TouchPoint& touch);
        void processHandPose(const HandPose& pose);
        std::vector<Gesture> recognizeGestures(const std::vector<TouchPoint>& touches);
        
    private:
        std::vector<TouchPoint> touchHistory;
        std::vector<Gesture> activeGestures;
        
        bool isPaintGesture(const std::vector<TouchPoint>& touches);
        bool isPinchGesture(const std::vector<TouchPoint>& touches);
        bool isSwipeGesture(const std::vector<TouchPoint>& touches);
        
        float calculateVelocity(const std::vector<TouchPoint>& points);
        float calculatePressure(const std::vector<TouchPoint>& points);
    } gestureRecognizer;
    
    //==============================================================================
    // Air Gesture System (Camera-based)
    
    class AirGestureEngine
    {
    public:
        void initialize();
        void processFrame(const juce::Image& cameraFrame);
        std::vector<HandPose> detectHands(const juce::Image& frame);
        void calibrateDepthSpace();
        
    private:
        bool isInitialized = false;
        juce::Rectangle<float> trackingArea;
        std::vector<HandPose> currentPoses;
        
        // Hand tracking implementation would use ML models
        // For now, placeholder for MediaPipe or similar
        void runHandDetectionModel(const juce::Image& frame);
        HandPose extractHandPose(const std::vector<juce::Point<float>>& landmarks);
    } airGestureEngine;
    
    //==============================================================================
    // Haptic Feedback Implementation
    
    class HapticEngine
    {
    public:
        void initialize();
        void processHapticEvent(const HapticEvent& event);
        void updateAudioReactiveHaptics(const juce::AudioBuffer<float>& audioBuffer);
        
    private:
        std::vector<HapticEvent> activeEvents;
        bool audioReactiveEnabled = false;
        float audioReactiveSensitivity = 0.5f;
        
        void generateHapticSignal(const HapticEvent& event);
        void sendToHapticDevice(const std::vector<float>& signal);
    } hapticEngine;
    
    //==============================================================================
    // State Management
    
    std::vector<PaintSurfaceInfo> connectedPaintSurfaces;
    std::vector<MIDIControllerInfo> connectedMIDIControllers;
    std::vector<DeviceRole> deviceRoles;
    std::vector<ControlMapping> controlMappings;
    
    std::atomic<bool> gestureRecognitionEnabled{true};
    std::atomic<bool> airGesturesEnabled{false};
    std::atomic<bool> hapticFeedbackEnabled{true};
    std::atomic<float> gestureSensitivity{0.8f};
    std::atomic<float> hapticIntensity{0.7f};
    
    // Current input state
    std::vector<TouchPoint> activeTouches;
    std::vector<HandPose> currentHandPoses;
    std::vector<Gesture> currentGestures;
    
    //==============================================================================
    // Device Communication
    
    // Paint surface communication
    void processPaintSurfaceData(const juce::String& deviceId, const juce::MemoryBlock& data);
    void sendPaintSurfaceCommand(const juce::String& deviceId, const juce::String& command);
    
    // MIDI communication
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message);
    
    // MIDI Controller mapping helpers
    void createPushMapping(const MIDIControllerInfo& controller);
    void createMaschineMapping(const MIDIControllerInfo& controller);
    void createLaunchpadMapping(const MIDIControllerInfo& controller);
    void createGenericMapping(const MIDIControllerInfo& controller);
    void routeControlValue(int target, const juce::String& targetParameter, float mappedValue);
    void handleNoteEvent(int note, int velocity, bool isNoteOn);
    void sendDeviceSyncCommand(const juce::String& deviceId);
    
    //==============================================================================
    // Threading & Performance
    
    juce::CriticalSection deviceLock;
    juce::CriticalSection touchLock;
    juce::CriticalSection gestureLock;
    
    // Background processing thread
    class HardwareThread : public juce::Thread
    {
    public:
        HardwareThread(HardwareControllerManager& owner) 
            : Thread("Hardware Manager"), manager(owner) {}
        void run() override;
        
    private:
        HardwareControllerManager& manager;
    } hardwareThread{*this};
    
    // Performance monitoring
    std::atomic<float> inputLatency{0.0f};
    std::atomic<int> touchEventsPerSecond{0};
    juce::Time lastPerformanceUpdate;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareControllerManager)
};