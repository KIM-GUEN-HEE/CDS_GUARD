#pragma once

#include <string>
#include <filesystem>
#include <cstdlib>

namespace CdsGuardOs
{
    enum class OperatingSystem
    {
        Windows,
        Linux,
        Mac,
        Unknown
    };

    
    constexpr OperatingSystem kCurrentOperatingSystem = 
        #if defined(_WIN32)
        OperatingSystem::Windows;
        #elif defined(__linux__)
        OperatingSystem::Linux;
        #elif defined(__APPLE__)
        OperatingSystem::Mac;
        #else
        OperatingSystem::Unknown;
        #endif
    
    constexpr std::string kApplicationName{"CDS Interface"};

    inline const std::filesystem::path kApplicationDataPath = 
    (kCurrentOperatingSystem == OperatingSystem::Windows) ? 
        (std::getenv("APPDATA") ? 
            (std::filesystem::path{std::getenv("APPDATA")} / kApplicationName) : 
            (std::filesystem::path{"C:\\Temp"} / kApplicationName)) :
    (kCurrentOperatingSystem == OperatingSystem::Linux) ?
        (std::getenv("HOME") ? 
            (std::filesystem::path{std::getenv("HOME")} / ".config" / kApplicationName) : 
            (std::filesystem::path{"/tmp"} / kApplicationName)) :
    (kCurrentOperatingSystem == OperatingSystem::Mac) ?
        (std::getenv("HOME") ? 
            (std::filesystem::path{std::getenv("HOME")} / "Library/Application Support" / kApplicationName) : 
            (std::filesystem::path{"/tmp"} / kApplicationName)) :
    std::filesystem::path{"Undefined"};
}