#pragma once

#include <chrono>
#include <string>

namespace landrop {

// Three types of directory changes FileWatcher handles
enum class FileEventType {
    Created,
    Modified,
    Deleted,
};

// FileEvent data
struct FileEvent {
    std::string path;
    FileEventType type;
    std::chrono::system_clock::time_point timestamp;
};

}
