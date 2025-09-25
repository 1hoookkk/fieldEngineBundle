#include "SampleLoaderService.h"
#include "DiagnosticLogger.h"
#include <juce_core/juce_core.h>
#include <chrono>

//==============================================================================
class SampleLoaderService::LoadOperation : public juce::ThreadPoolJob
{
public:
    LoadOperation(SampleLoaderService& service, const juce::File& file, LoadCallback callback)
        : ThreadPoolJob("SampleLoad"), service_(service), file_(file), callback_(std::move(callback))
    {
        LOGV(LOADER, "LoadOperation created for: %s", file.getFullPathName().toRawUTF8());
    }

    JobStatus runJob() override
    {
        LOGI(LOADER, "Starting async load: %s", file_.getFullPathName().toRawUTF8());
        
        auto result = service_.performSyncLoad(file_);
        
        // Always callback on MessageThread
        juce::MessageManager::callAsync([callback = std::move(callback_), result = std::move(result)]() mutable
        {
            LOGV(LOADER, "Load operation callback executing on MessageThread");
            callback(std::move(result));
        });
        
        return JobStatus::jobHasFinished;
    }

private:
    SampleLoaderService& service_;
    juce::File file_;
    LoadCallback callback_;
};

//==============================================================================
SampleLoaderService::SampleLoaderService()
{
    // Initialize audio format manager with all available formats
    formatManager_ = std::make_unique<juce::AudioFormatManager>();
    formatManager_->registerBasicFormats();
    
#ifdef SPECTRAL_ENABLE_MP3
    LOGI(DECODER, "MP3 support enabled in build");
#else
    LOGI(DECODER, "MP3 support disabled in build");
#endif

    // Create thread pool for async loading
    threadPool_ = std::make_unique<juce::ThreadPool>(2); // Max 2 concurrent loads
    
    LOGI(LOADER, "SampleLoaderService initialized with %d supported formats", 
         formatManager_->getNumKnownFormats());
    
    // Log supported formats
    for (int i = 0; i < formatManager_->getNumKnownFormats(); ++i)
    {
        auto* format = formatManager_->getKnownFormat(i);
        if (format)
        {
            LOGV(DECODER, "Registered format: %s (extensions: %s)", 
                 format->getFormatName().toRawUTF8(),
                 format->getFileExtensions().joinIntoString(", ").toRawUTF8());
        }
    }
}

SampleLoaderService::~SampleLoaderService()
{
    if (threadPool_)
    {
        threadPool_->removeAllJobs(true, 5000); // Wait up to 5s for jobs to finish
    }
    LOGI(LOADER, "SampleLoaderService destroyed");
}

void SampleLoaderService::loadViaChooser(juce::Component& parent, LoadCallback callback)
{
    LOGI(UI, "Opening file chooser for sample loading");
    
    auto extensions = getSupportedExtensions().joinIntoString(";");
    
    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose an audio file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        extensions);

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, callback = std::move(callback), chooser](const juce::FileChooser& fc) mutable
        {
            auto result = fc.getResult();
            
            if (!result.exists())
            {
                LOGI(UI, "File chooser cancelled by user");
                
                Result cancelResult;
                cancelResult.code = ErrorCode::FileDialogCancelled;
                cancelResult.errorMessage = "File selection cancelled";
                callback(std::move(cancelResult));
                return;
            }
            
            LOGI(UI, "File selected: %s", result.getFullPathName().toRawUTF8());
            loadFromFile(result, std::move(callback));
        });
}

void SampleLoaderService::loadFromFile(const juce::File& file, LoadCallback callback)
{
    LOGI(LOADER, "Loading file: %s (size: %lld bytes)", 
         file.getFullPathName().toRawUTF8(), file.getSize());
    
    if (!file.existsAsFile())
    {
        LOGE(LOADER, "File does not exist: %s", file.getFullPathName().toRawUTF8());
        
        Result result;
        result.code = ErrorCode::FileOpenFailed;
        result.errorMessage = getErrorMessage(ErrorCode::FileOpenFailed);
        
        juce::MessageManager::callAsync([callback = std::move(callback), result = std::move(result)]() mutable
        {
            callback(std::move(result));
        });
        return;
    }
    
    // Check if extension is supported
    auto extension = file.getFileExtension().toLowerCase();
    if (!isExtensionSupported(extension))
    {
        LOGE(DECODER, "Unsupported file extension: %s", extension.toRawUTF8());
        
        Result result;
        result.code = ErrorCode::UnsupportedFormat;
        result.errorMessage = getErrorMessage(ErrorCode::UnsupportedFormat);
        
        juce::MessageManager::callAsync([callback = std::move(callback), result = std::move(result)]() mutable
        {
            callback(std::move(result));
        });
        return;
    }
    
    // Launch async load operation
    performAsyncLoad(file, std::move(callback));
}

juce::StringArray SampleLoaderService::getSupportedExtensions() const
{
    juce::StringArray extensions;
    
    // Always supported by JUCE
    extensions.add("*.wav");
    extensions.add("*.aiff");
    extensions.add("*.aif");
    extensions.add("*.flac");
    extensions.add("*.ogg");
    
#ifdef SPECTRAL_ENABLE_MP3
    extensions.add("*.mp3");
#endif
    
    return extensions;
}

juce::String SampleLoaderService::getSupportedFormatsString() const
{
    auto extensions = getSupportedExtensions();
    juce::StringArray formats;
    
    for (const auto& ext : extensions)
    {
        // Remove the "*.") prefix for display
        formats.add(ext.substring(2).toUpperCase());
    }
    
    return formats.joinIntoString(", ");
}

juce::String SampleLoaderService::getErrorMessage(ErrorCode code)
{
    switch (code)
    {
        case ErrorCode::Success:
            return "Load successful";
            
        case ErrorCode::FileDialogCancelled:
            return "Load cancelled";
            
        case ErrorCode::FileOpenFailed:
            return "Couldn't open file. Check permissions or path.";
            
        case ErrorCode::UnsupportedFormat:
#ifdef SPECTRAL_ENABLE_MP3
            return "Unsupported format. Try WAV, AIFF, FLAC, OGG, or MP3.";
#else
            return "Unsupported format. Try WAV, AIFF, FLAC, or OGG. MP3 support disabled.";
#endif
            
        case ErrorCode::DecodeFailed:
            return "Decode failed. File may be corrupt or use unsupported encoding.";
            
        case ErrorCode::RendererInitFailed:
            return "Sample loaded but renderer activation failed. Check logs.";
            
        case ErrorCode::InvalidSampleData:
            return "File decoded but contains no valid audio data.";
            
        default:
            return "Unknown error occurred during sample loading.";
    }
}

bool SampleLoaderService::isExtensionSupported(const juce::String& extension) const
{
    auto lowerExt = extension.toLowerCase();
    
    // Remove leading dot if present
    if (lowerExt.startsWith("."))
        lowerExt = lowerExt.substring(1);
    
    // Check against supported list
    if (lowerExt == "wav" || lowerExt == "aiff" || lowerExt == "aif" || 
        lowerExt == "flac" || lowerExt == "ogg")
        return true;
        
#ifdef SPECTRAL_ENABLE_MP3
    if (lowerExt == "mp3")
        return true;
#endif
    
    return false;
}

void SampleLoaderService::performAsyncLoad(const juce::File& file, LoadCallback callback)
{
    auto operation = std::make_unique<LoadOperation>(*this, file, std::move(callback));
    threadPool_->addJob(operation.release(), true); // Pool takes ownership
}

SampleLoaderService::Result SampleLoaderService::performSyncLoad(const juce::File& file)
{
    Result result;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    LOGI(DECODER, "Creating reader for: %s", file.getFullPathName().toRawUTF8());
    
    // Create reader
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_->createReaderFor(file));
    if (!reader)
    {
        LOGE(DECODER, "Failed to create reader for file: %s", file.getFullPathName().toRawUTF8());
        result.code = ErrorCode::UnsupportedFormat;
        result.errorMessage = getErrorMessage(ErrorCode::UnsupportedFormat);
        return result;
    }
    
    // Validate sample metadata
    const auto numSamples = static_cast<int>(reader->lengthInSamples);
    const auto numChannels = static_cast<int>(reader->numChannels);
    const auto sampleRate = reader->sampleRate;
    const auto bitDepth = static_cast<int>(reader->bitsPerSample);
    
    LOGI(DECODER, "Sample metadata: %d samples, %d channels, %.1fkHz, %d-bit", 
         numSamples, numChannels, sampleRate / 1000.0, bitDepth);
    
    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0)
    {
        LOGE(DECODER, "Invalid sample data: samples=%d, channels=%d, rate=%.1f", 
             numSamples, numChannels, sampleRate);
        result.code = ErrorCode::InvalidSampleData;
        result.errorMessage = getErrorMessage(ErrorCode::InvalidSampleData);
        return result;
    }
    
    // Create buffer and read data
    LOGV(DECODER, "Allocating buffer: %d channels x %d samples", numChannels, numSamples);
    
    auto sample = std::make_unique<LoadedSample>();
    sample->audio.setSize(numChannels, numSamples);
    
    // Read the entire file
    bool readSuccess = reader->read(&sample->audio, 0, numSamples, 0, true, true);
    if (!readSuccess)
    {
        LOGE(DECODER, "Failed to read audio data from file");
        result.code = ErrorCode::DecodeFailed;
        result.errorMessage = getErrorMessage(ErrorCode::DecodeFailed);
        return result;
    }
    
    // Fill metadata
    sample->sampleRate = sampleRate;
    sample->numChannels = numChannels;
    sample->numSamples = numSamples;
    sample->bitDepth = bitDepth;
    sample->durationSeconds = numSamples / sampleRate;
    sample->filename = file.getFileNameWithoutExtension();
    sample->extension = file.getFileExtension().toLowerCase();
    sample->originalFile = file;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto decodeTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    LOGI(DECODER, "Decode completed in %lldms: %s (%.2fs, %.1fkHz)", 
         decodeTimeMs, sample->filename.toRawUTF8(), 
         sample->durationSeconds, sample->sampleRate / 1000.0);
    
    // Success
    result.sample = std::move(sample);
    return result;
}