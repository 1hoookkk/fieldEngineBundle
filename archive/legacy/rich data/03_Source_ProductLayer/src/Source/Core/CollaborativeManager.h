#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>

/**
 * Collaborative Manager - Real-Time Music Creation Revolution
 * 
 * Enables multiple producers to create music together in real-time,
 * fundamentally changing how collaborative music production works.
 * 
 * Core Innovation:
 * - Real-time paint stroke sharing across the internet
 * - Role-based collaboration (one person controls samples, another effects, etc.)
 * - Asynchronous creative challenges and community features
 * - Cloud-based project storage with version control
 * - Live streaming integration for performance and education
 * 
 * Vision: Transform music creation from solo activity to social experience
 */
class CollaborativeManager
{
public:
    CollaborativeManager();
    ~CollaborativeManager();
    
    //==============================================================================
    // Session Management
    
    enum class SessionType
    {
        Solo,               // Local session only
        RealTimeCollab,     // Real-time collaborative session
        AsynchronousCollab, // Turn-based collaboration
        LivePerformance,    // Live streaming performance
        Challenge,          // Community challenge/contest
        Educational,        // Teaching/learning session
        Jam,               // Informal jam session
        Production          // Formal production session
    };
    
    struct SessionInfo
    {
        juce::String sessionId;
        juce::String sessionName;
        SessionType type;
        juce::String hostUserId;
        std::vector<juce::String> participantIds;
        juce::Time createdTime;
        juce::Time lastActivity;
        bool isPublic = false;
        bool allowSpectators = true;
        int maxParticipants = 4;
        juce::String genre;
        juce::String description;
        float tempo = 120.0f;
        juce::String key = "C";
        
        // Session permissions
        struct Permissions
        {
            bool canPaint = true;
            bool canLoadSamples = true;
            bool canControlEffects = true;
            bool canControlMix = false;
            bool canInviteOthers = false;
            bool canKickParticipants = false;
        };
        
        std::unordered_map<juce::String, Permissions> userPermissions;
    };
    
    // Session lifecycle
    bool createSession(const SessionInfo& info);
    bool joinSession(const juce::String& sessionId, const juce::String& userId);
    bool leaveSession();
    void endSession();
    
    SessionInfo getCurrentSession() const { return currentSession; }
    bool isInSession() const { return inActiveSession.load(); }
    
    //==============================================================================
    // Real-Time Collaboration
    
    enum class CollaborativeAction
    {
        // Paint actions
        BeginPaintStroke,
        UpdatePaintStroke,
        EndPaintStroke,
        
        // Sample actions
        LoadSample,
        TriggerSample,
        AdjustSampleVolume,
        
        // Effect actions
        ChangeEffectParameter,
        EnableEffect,
        DisableEffect,
        
        // Tracker actions
        AddTrackerNote,
        RemoveTrackerNote,
        ChangePattern,
        
        // Communication
        ChatMessage,
        VoiceNote,
        ReactionEmoji,
        
        // Session control
        PlayPause,
        ChangeTimecode,
        ChangeTempo,
        ChangeKey
    };
    
    struct CollaborativeEvent
    {
        juce::String eventId;
        juce::String userId;
        juce::String userName;
        CollaborativeAction action;
        juce::uint32 timestamp;
        juce::var parameters;       // Flexible parameter storage
        bool isConfirmed = false;   // For conflict resolution
        
        // Visual feedback
        juce::Colour userColor;
        juce::Point<float> screenPosition;
        float fadeOutTime = 0.0f;
    };
    
    // Send actions to other participants
    void broadcastAction(CollaborativeAction action, const juce::var& parameters);
    void sendPrivateAction(const juce::String& targetUserId, CollaborativeAction action, const juce::var& parameters);
    
    // Receive and process actions from others
    void processIncomingEvents();
    std::vector<CollaborativeEvent> getRecentEvents(int maxEvents = 50) const;
    
    //==============================================================================
    // Role-Based Collaboration
    
    enum class CollaborativeRole
    {
        Host,           // Full control
        Producer,       // Can paint, load samples, control effects
        Painter,        // Can only paint strokes
        Mixer,          // Can only control volume/effects
        Spectator,      // View only
        Educator,       // Teaching permissions
        Student         // Learning mode with guided restrictions
    };
    
    struct UserPresence
    {
        juce::String userId;
        juce::String userName;
        juce::String avatarUrl;
        CollaborativeRole role;
        juce::Colour userColor;
        bool isOnline = false;
        bool isActive = false;      // Currently interacting
        juce::Point<float> cursorPosition;
        juce::Time lastActivity;
        
        // Real-time status
        bool isPainting = false;
        bool isPlayingAudio = false;
        juce::String currentActivity; // "Loading sample", "Painting on track 3", etc.
    };
    
    void setUserRole(const juce::String& userId, CollaborativeRole role);
    void assignUserColor(const juce::String& userId, juce::Colour color);
    std::vector<UserPresence> getConnectedUsers() const;
    void updateUserPresence(const juce::Point<float>& cursorPos, const juce::String& activity);
    
    //==============================================================================
    // Real-Time Paint Stroke Sharing
    
    struct SharedPaintStroke
    {
        juce::String strokeId;
        juce::String userId;
        juce::String userName;
        juce::Colour userColor;
        juce::Path strokePath;
        float pressure = 1.0f;
        juce::uint32 startTime;
        juce::uint32 endTime;
        bool isComplete = false;
        
        // Paint stroke parameters
        int targetEngine;           // Which engine (masking, tracker, etc.)
        juce::var engineParameters; // Engine-specific parameters
        
        // Conflict resolution
        int version = 1;
        bool hasConflict = false;
        std::vector<juce::String> conflictingUsers;
    };
    
    // Real-time stroke sharing
    void beginSharedStroke(const juce::Point<float>& startPoint, float pressure);
    void updateSharedStroke(const juce::Point<float>& point, float pressure);
    void endSharedStroke();
    
    std::vector<SharedPaintStroke> getActiveSharedStrokes() const;
    void resolveStrokeConflict(const juce::String& strokeId, bool acceptRemoteVersion);
    
    //==============================================================================
    // Communication & Social Features
    
    struct ChatMessage
    {
        juce::String messageId;
        juce::String userId;
        juce::String userName;
        juce::String content;
        juce::Time timestamp;
        enum Type { Text, Voice, Audio, Image, Link } type = Text;
        juce::String attachmentUrl;
        
        // Message reactions
        std::unordered_map<juce::String, juce::String> reactions; // userId -> emoji
    };
    
    // Communication
    void sendChatMessage(const juce::String& message);
    void sendVoiceNote(const juce::AudioBuffer<float>& audioData, float duration);
    void sendReaction(const juce::String& emoji, const juce::Point<float>& position);
    
    std::vector<ChatMessage> getChatHistory(int maxMessages = 100) const;
    void addReactionToMessage(const juce::String& messageId, const juce::String& emoji);
    
    // Audio sharing
    void shareAudioClip(const juce::AudioBuffer<float>& audio, const juce::String& description);
    void requestAudioFromUser(const juce::String& userId, const juce::String& description);
    
    //==============================================================================
    // Community Features & Challenges
    
    struct CreativeChallenge
    {
        juce::String challengeId;
        juce::String title;
        juce::String description;
        juce::String creatorId;
        juce::Time startTime;
        juce::Time endTime;
        
        // Challenge parameters
        juce::String genre;
        float tempo = 120.0f;
        juce::String key = "C";
        int maxDuration = 60;       // Seconds
        bool providedSamples = false;
        std::vector<juce::String> sampleUrls;
        
        // Submissions
        struct Submission
        {
            juce::String submissionId;
            juce::String userId;
            juce::String userName;
            juce::String audioUrl;
            juce::String projectData; // Serialized project
            juce::Time submitTime;
            int votes = 0;
            float averageRating = 0.0f;
            std::vector<juce::String> tags;
        };
        
        std::vector<Submission> submissions;
        int maxSubmissions = 100;
        bool allowVoting = true;
        bool allowComments = true;
    };
    
    // Challenge participation
    std::vector<CreativeChallenge> getActiveChallenges() const;
    void joinChallenge(const juce::String& challengeId);
    void submitToChallenge(const juce::String& challengeId, const juce::AudioBuffer<float>& audio);
    void voteOnSubmission(const juce::String& submissionId, float rating);
    
    // Create challenges
    bool createChallenge(const CreativeChallenge& challenge);
    void endChallenge(const juce::String& challengeId);
    
    //==============================================================================
    // Cloud Storage & Version Control
    
    struct ProjectVersion
    {
        juce::String versionId;
        juce::String projectData;   // Serialized project state
        juce::String userId;
        juce::String description;
        juce::Time timestamp;
        juce::String parentVersionId;
        bool isAutoSave = false;
    };
    
    // Project management
    bool saveProjectToCloud(const juce::String& projectName, const juce::String& projectData);
    juce::String loadProjectFromCloud(const juce::String& projectId);
    std::vector<juce::String> getCloudProjects() const;
    
    // Version control
    void saveProjectVersion(const juce::String& description, const juce::String& projectData);
    std::vector<ProjectVersion> getProjectVersions(const juce::String& projectId) const;
    void revertToVersion(const juce::String& versionId);
    void branchFromVersion(const juce::String& versionId, const juce::String& branchName);
    
    //==============================================================================
    // Live Streaming & Performance
    
    struct StreamingSession
    {
        juce::String streamId;
        juce::String title;
        juce::String description;
        bool isLive = false;
        int viewerCount = 0;
        juce::Time startTime;
        
        // Stream settings
        bool enableChat = true;
        bool enableViewerInteraction = false;  // Viewers can suggest
        bool recordStream = true;
        int maxViewers = 1000;
        
        // Interaction
        struct ViewerSuggestion
        {
            juce::String viewerId;
            juce::String suggestion;
            int upvotes = 0;
            juce::Time timestamp;
        };
        
        std::vector<ViewerSuggestion> viewerSuggestions;
    };
    
    // Streaming
    bool startLiveStream(const StreamingSession& config);
    void endLiveStream();
    void updateStreamMetadata(const juce::String& title, const juce::String& description);
    
    StreamingSession getCurrentStream() const { return currentStream; }
    bool isStreaming() const { return streaming.load(); }
    
    // Viewer interaction
    std::vector<StreamingSession::ViewerSuggestion> getViewerSuggestions() const;
    void respondToViewerSuggestion(const juce::String& suggestionId, bool accepted);
    
    //==============================================================================
    // Network & Performance
    
    struct NetworkStats
    {
        float latencyMs = 0.0f;
        float jitterMs = 0.0f;
        float packetLoss = 0.0f;
        float uploadBandwidthKbps = 0.0f;
        float downloadBandwidthKbps = 0.0f;
        int connectedUsers = 0;
        int droppedEvents = 0;
        juce::String connectionQuality; // "Excellent", "Good", "Fair", "Poor"
    };
    
    NetworkStats getNetworkStats() const;
    void setNetworkQuality(int quality); // 0=Low bandwidth, 1=Medium, 2=High
    
    // Connection management
    bool isConnected() const { return connected.load(); }
    void reconnect();
    void setOfflineMode(bool offline) { offlineMode.store(offline); }
    
    //==============================================================================
    // Qwen3 Coder Integration Methods
    
    void generateDSPAlgorithm(const juce::String& description);
    void analyzeCurrentCode(const juce::String& codeSnippet);
    void refactorForPerformance(const juce::String& code);
    
private:
    //==============================================================================
    // Network Implementation
    
    class NetworkManager
    {
    public:
        bool connect(const juce::String& serverUrl);
        void disconnect();
        bool sendMessage(const juce::String& message);
        std::vector<juce::String> receiveMessages();
        
        // WebSocket or UDP implementation details
        void setupWebSocket();
        void setupUDP();
        void handleConnectionLoss();
        
    private:
        std::unique_ptr<juce::WebInputStream> webSocket;
        bool isConnected = false;
    } networkManager;
    
    //==============================================================================
    // State Management
    
    SessionInfo currentSession;
    StreamingSession currentStream;
    std::atomic<bool> inActiveSession{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> streaming{false};
    std::atomic<bool> offlineMode{false};
    
    // User management
    std::vector<UserPresence> connectedUsers;
    juce::String currentUserId;
    juce::String currentUserName;
    CollaborativeRole currentRole = CollaborativeRole::Producer;
    
    // Event handling
    std::vector<CollaborativeEvent> pendingEvents;
    std::vector<CollaborativeEvent> eventHistory;
    std::vector<SharedPaintStroke> activeStrokes;
    
    // Communication
    std::vector<ChatMessage> chatHistory;
    std::unique_ptr<SharedPaintStroke> currentSharedStroke;
    
    //==============================================================================
    // Threading & Synchronization
    
    juce::CriticalSection sessionLock;
    juce::CriticalSection eventLock;
    juce::CriticalSection strokeLock;
    juce::CriticalSection chatLock;
    
    // Background networking thread
    class NetworkThread : public juce::Thread
    {
    public:
        NetworkThread(CollaborativeManager& owner) 
            : Thread("Collaborative Network"), manager(owner) {}
        void run() override;
        
    private:
        CollaborativeManager& manager;
    } networkThread{*this};
    
    // Conflict resolution
    class ConflictResolver
    {
    public:
        void resolveStrokeConflict(SharedPaintStroke& stroke);
        void resolveParameterConflict(const juce::String& parameter, const juce::var& value);
        void resolveTimingConflict(juce::uint32 timestamp);
        
    private:
        enum class Resolution { HostWins, FirstWins, LastWins, Merge };
        Resolution defaultResolution = Resolution::LastWins;
    } conflictResolver;
    
    //==============================================================================
    // Cloud API Integration
    
    class CloudAPI
    {
    public:
        bool uploadProject(const juce::String& projectData);
        juce::String downloadProject(const juce::String& projectId);
        std::vector<juce::String> listProjects();
        bool deleteProject(const juce::String& projectId);
        
        // Authentication
        bool login(const juce::String& username, const juce::String& password);
        void logout();
        bool isLoggedIn() const { return authenticated; }
        
        //==============================================================================
        // Qwen3 Coder AI Integration
        
        struct Qwen3Request
        {
            juce::String model = "qwen/qwen3-coder";
            juce::String prompt;
            double temperature = 0.7;
            int maxTokens = 2048;
            bool stream = false;
        };
        
        struct Qwen3Response
        {
            juce::String content;
            juce::String finishReason;
            int promptTokens = 0;
            int completionTokens = 0;
            int totalTokens = 0;
            bool success = false;
            juce::String errorMessage;
        };
        
        // Qwen3 Coder API methods
        Qwen3Response generateCode(const juce::String& prompt, double temperature = 0.7);
        Qwen3Response generateCodeWithContext(const juce::String& prompt, const juce::String& context, double temperature = 0.7);
        Qwen3Response analyzeCode(const juce::String& code, const juce::String& analysisType = "review");
        Qwen3Response refactorCode(const juce::String& code, const juce::String& refactorType = "optimize");
        
        // Qwen3 Authentication
        bool setQwen3ApiKey(const juce::String& apiKey);
        bool isQwen3Authenticated() const { return !qwen3ApiKey.isEmpty(); }
        
        // Rate limiting and usage tracking
        struct Qwen3Usage
        {
            int requestsThisHour = 0;
            int requestsToday = 0;
            juce::int64 lastRequestTime = 0;
            double estimatedCost = 0.0;
        };
        
        Qwen3Usage getQwen3Usage() const { return qwen3Usage; }
        void resetQwen3Usage() { qwen3Usage = Qwen3Usage{}; }
        
    private:
        bool authenticated = false;
        juce::String authToken;
        juce::String apiEndpoint = "https://api.retrocanvas.com";
        
        // Qwen3 specific members
        juce::String qwen3ApiKey;
        juce::String qwen3Endpoint = "https://openrouter.ai/api/v1/chat/completions";
        Qwen3Usage qwen3Usage;
        
        // Helper methods for Qwen3 integration
        Qwen3Response makeQwen3Request(const Qwen3Request& request);
        juce::String buildQwen3Payload(const Qwen3Request& request);
        bool parseQwen3Response(const juce::String& response, Qwen3Response& result);
        void updateUsageStats(const Qwen3Response& response);
        bool checkRateLimit();
    } cloudAPI;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CollaborativeManager)
};