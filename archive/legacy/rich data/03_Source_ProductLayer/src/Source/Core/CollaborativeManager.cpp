#ifndef SC_EXPERIMENTAL_CLOUD
#define SC_EXPERIMENTAL_CLOUD 0
#endif

#if SC_EXPERIMENTAL_CLOUD
#include "CollaborativeManager.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

//==============================================================================
// CloudAPI Qwen3 Coder Implementation

CloudAPI::Qwen3Response CloudAPI::generateCode(const juce::String& prompt, double temperature)
{
    if (!isQwen3Authenticated())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Qwen3 API key not set" };
    }
    
    if (!checkRateLimit())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Rate limit exceeded" };
    }
    
    Qwen3Request request;
    request.prompt = prompt;
    request.temperature = temperature;
    
    return makeQwen3Request(request);
}

CloudAPI::Qwen3Response CloudAPI::generateCodeWithContext(const juce::String& prompt, const juce::String& context, double temperature)
{
    if (!isQwen3Authenticated())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Qwen3 API key not set" };
    }
    
    if (!checkRateLimit())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Rate limit exceeded" };
    }
    
    Qwen3Request request;
    request.prompt = "Context:\n" + context + "\n\nRequest:\n" + prompt;
    request.temperature = temperature;
    request.maxTokens = 4096; // More tokens for context-aware generation
    
    return makeQwen3Request(request);
}

CloudAPI::Qwen3Response CloudAPI::analyzeCode(const juce::String& code, const juce::String& analysisType)
{
    if (!isQwen3Authenticated())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Qwen3 API key not set" };
    }
    
    if (!checkRateLimit())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Rate limit exceeded" };
    }
    
    Qwen3Request request;
    request.prompt = "Analyze this C++ code for " + analysisType + ":\n\n" + code + 
                    "\n\nProvide a detailed analysis focusing on " + analysisType + ".";
    request.temperature = 0.3; // Lower temperature for analysis
    
    return makeQwen3Request(request);
}

CloudAPI::Qwen3Response CloudAPI::refactorCode(const juce::String& code, const juce::String& refactorType)
{
    if (!isQwen3Authenticated())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Qwen3 API key not set" };
    }
    
    if (!checkRateLimit())
    {
        return Qwen3Response{ "", "", 0, 0, 0, false, "Rate limit exceeded" };
    }
    
    Qwen3Request request;
    request.prompt = "Refactor this C++ code for " + refactorType + ":\n\n" + code + 
                    "\n\nProvide the refactored code with explanations of changes.";
    request.temperature = 0.4; // Moderate temperature for refactoring
    
    return makeQwen3Request(request);
}

bool CloudAPI::setQwen3ApiKey(const juce::String& apiKey)
{
    if (apiKey.isEmpty())
        return false;
    
    qwen3ApiKey = apiKey;
    return true;
}

CloudAPI::Qwen3Response CloudAPI::makeQwen3Request(const Qwen3Request& request)
{
    Qwen3Response response;
    
    try
    {
        // Build the HTTP request
        juce::URL url(qwen3Endpoint);
        juce::StringPairArray headers;
        headers.set("Content-Type", "application/json");
        headers.set("Authorization", "Bearer " + qwen3ApiKey);
        headers.set("HTTP-Referer", "https://spectralcanvas.com");
        headers.set("X-Title", "SpectralCanvas Pro");
        
        // Build the payload
        juce::String payload = buildQwen3Payload(request);
        
        // Create the input stream
        std::unique_ptr<juce::WebInputStream> stream = std::make_unique<juce::WebInputStream>(
            url, true, payload.toRawUTF8(), payload.getNumBytesAsUTF8(), headers, 30000);
        
        if (stream == nullptr || !stream->connect(nullptr))
        {
            response.success = false;
            response.errorMessage = "Failed to connect to Qwen3 API";
            return response;
        }
        
        // Read the response
        juce::String responseText;
        juce::MemoryOutputStream responseStream;
        
        if (stream->readEntireStream(responseStream))
        {
            responseText = responseStream.toString();
        }
        else
        {
            response.success = false;
            response.errorMessage = "Failed to read response from Qwen3 API";
            return response;
        }
        
        // Parse the response
        if (!parseQwen3Response(responseText, response))
        {
            response.success = false;
            response.errorMessage = "Failed to parse Qwen3 API response";
            return response;
        }
        
        // Update usage statistics
        updateUsageStats(response);
        
    }
    catch (const std::exception& e)
    {
        response.success = false;
        response.errorMessage = "Exception: " + juce::String(e.what());
    }
    
    return response;
}

juce::String CloudAPI::buildQwen3Payload(const Qwen3Request& request)
{
    juce::var payload;
    payload["model"] = request.model;
    payload["messages"] = juce::var::Array{
        juce::var::Object{
            {"role", "user"},
            {"content", request.prompt}
        }
    };
    payload["temperature"] = request.temperature;
    payload["max_tokens"] = request.maxTokens;
    payload["stream"] = request.stream;
    
    return payload.toString();
}

bool CloudAPI::parseQwen3Response(const juce::String& response, Qwen3Response& result)
{
    try
    {
        juce::var json = juce::JSON::parse(response);
        
        if (json.hasProperty("error"))
        {
            result.success = false;
            result.errorMessage = json["error"]["message"].toString();
            return false;
        }
        
        if (json.hasProperty("choices") && json["choices"].isArray())
        {
            auto choices = json["choices"];
            if (choices.size() > 0)
            {
                auto choice = choices[0];
                if (choice.hasProperty("message") && choice["message"].hasProperty("content"))
                {
                    result.content = choice["message"]["content"].toString();
                    result.success = true;
                }
                
                if (choice.hasProperty("finish_reason"))
                {
                    result.finishReason = choice["finish_reason"].toString();
                }
            }
        }
        
        // Parse usage statistics
        if (json.hasProperty("usage"))
        {
            auto usage = json["usage"];
            result.promptTokens = usage.getProperty("prompt_tokens", 0);
            result.completionTokens = usage.getProperty("completion_tokens", 0);
            result.totalTokens = usage.getProperty("total_tokens", 0);
        }
        
        return result.success;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.errorMessage = "JSON parsing error: " + juce::String(e.what());
        return false;
    }
}

void CloudAPI::updateUsageStats(const Qwen3Response& response)
{
    if (response.success)
    {
        qwen3Usage.requestsThisHour++;
        qwen3Usage.requestsToday++;
        qwen3Usage.lastRequestTime = juce::Time::getHighResolutionTicks();
        
        // Estimate cost (OpenRouter pricing: $0.20/M input, $0.80/M output)
        double inputCost = (response.promptTokens / 1000000.0) * 0.20;
        double outputCost = (response.completionTokens / 1000000.0) * 0.80;
        qwen3Usage.estimatedCost += inputCost + outputCost;
    }
}

bool CloudAPI::checkRateLimit()
{
    // Simple rate limiting: max 10 requests per hour
    if (qwen3Usage.requestsThisHour >= 10)
    {
        return false;
    }
    
    // Reset hourly counter if more than an hour has passed
    juce::int64 currentTime = juce::Time::getHighResolutionTicks();
    juce::int64 oneHourInTicks = juce::Time::getHighResolutionTicksPerSecond() * 3600;
    
    if (currentTime - qwen3Usage.lastRequestTime > oneHourInTicks)
    {
        qwen3Usage.requestsThisHour = 0;
    }
    
    return true;
}

//==============================================================================
// Example usage methods for SpectralCanvas integration

void CollaborativeManager::generateDSPAlgorithm(const juce::String& description)
{
    if (!cloudAPI.isQwen3Authenticated())
    {
        // Could show a dialog asking for API key
        return;
    }
    
    juce::String prompt = "Generate a JUCE DSP algorithm for: " + description + 
                         "\n\nRequirements:\n" +
                         "- Must be real-time safe\n" +
                         "- Use JUCE DSP module\n" +
                         "- Include proper parameter handling\n" +
                         "- Add comments explaining the algorithm";
    
    auto response = cloudAPI.generateCode(prompt, 0.3);
    
    if (response.success)
    {
        // Could integrate this with your code generation system
        // For now, we'll just log it
        juce::Logger::writeToLog("Generated DSP Algorithm:\n" + response.content);
    }
    else
    {
        juce::Logger::writeToLog("Failed to generate DSP algorithm: " + response.errorMessage);
    }
}

void CollaborativeManager::analyzeCurrentCode(const juce::String& codeSnippet)
{
    if (!cloudAPI.isQwen3Authenticated())
        return;
    
    auto response = cloudAPI.analyzeCode(codeSnippet, "performance and RT-safety");
    
    if (response.success)
    {
        juce::Logger::writeToLog("Code Analysis:\n" + response.content);
    }
    else
    {
        juce::Logger::writeToLog("Analysis failed: " + response.errorMessage);
    }
}

void CollaborativeManager::refactorForPerformance(const juce::String& code)
{
    if (!cloudAPI.isQwen3Authenticated())
        return;
    
    auto response = cloudAPI.refactorCode(code, "performance optimization");
    
    if (response.success)
    {
        juce::Logger::writeToLog("Refactored Code:\n" + response.content);
    }
    else
    {
        juce::Logger::writeToLog("Refactoring failed: " + response.errorMessage);
    }
}
#else
// SC_EXPERIMENTAL_CLOUD disabled: provide no-op TU to avoid link issues
#include "CollaborativeManager.h"
#endif
