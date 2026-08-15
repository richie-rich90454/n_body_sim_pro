#include "logging/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace n_body_sim_pro::logging {

namespace {
double monotonic_seconds() {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}
}  // namespace

const char* level_name(Level level) {
    switch (level) {
        case Level::Off:
            return "OFF";
        case Level::Error:
            return "ERROR";
        case Level::Warning:
            return "WARN";
        case Level::Info:
            return "INFO";
        case Level::Debug:
            return "DEBUG";
        case Level::Trace:
            return "TRACE";
        case Level::Performance:
            return "PERF";
        case Level::Instrumentation:
            return "INST";
    }
    return "?";
}

const char* category_name(Category category) {
    switch (category) {
        case Category::Simulation:
            return "SIMULATION";
        case Category::Threading:
            return "THREADING";
        case Category::Simd:
            return "SIMD";
        case Category::Tree:
            return "TREE";
        case Category::Physics:
            return "PHYSICS";
        case Category::Memory:
            return "MEMORY";
        case Category::Numerics:
            return "NUMERICS";
        case Category::Render:
            return "RENDER";
        case Category::Application:
            return "APPLICATION";
        case Category::System:
            return "SYSTEM";
    }
    return "?";
}

Logger::Logger() {
    session_start_seconds_ = monotonic_seconds();
    std::fill(std::begin(buffer_), std::end(buffer_), Record{});
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_minimum_level(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minimum_level_ = level;
}

void Logger::log(Level level, Category category, const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    logv(level, category, format, arguments);
    va_end(arguments);
}

void Logger::logv(Level level, Category category, const char* format,
                  std::va_list arguments) {
    if (!is_enabled(level)) {
        return;
    }

    char formatted[1024];
    std::vsnprintf(formatted, sizeof(formatted), format, arguments);

    const unsigned int thread_id =
        static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id()) &
                                  0xFFFFu);

    std::lock_guard<std::mutex> lock(mutex_);
    const double timestamp = monotonic_seconds() - session_start_seconds_;

    Record record;
    record.level = level;
    record.category = category;
    record.thread_id = thread_id;
    record.timestamp_seconds = timestamp;
    record.message = formatted;

    buffer_[buffer_next_] = std::move(record);
    buffer_next_ = (buffer_next_ + 1) % kLogBufferCapacity;
    if (buffer_count_ < kLogBufferCapacity) {
        ++buffer_count_;
    }

    if (console_enabled_) {
        std::fprintf(stderr, "[%09.3f] [%5s] [%-12s] [tid %04x] %s\n", timestamp,
                     level_name(level), category_name(category), thread_id, formatted);
    }
}

void Logger::copy_records(std::vector<Record>& destination) const {
    std::lock_guard<std::mutex> lock(mutex_);
    destination.clear();
    if (buffer_count_ == 0) {
        return;
    }
    destination.reserve(buffer_count_);
    if (buffer_count_ == kLogBufferCapacity) {
        for (std::size_t i = 0; i < buffer_count_; ++i) {
            const std::size_t index = (buffer_next_ + i) % kLogBufferCapacity;
            destination.push_back(buffer_[index]);
        }
    } else {
        for (std::size_t i = 0; i < buffer_count_; ++i) {
            destination.push_back(buffer_[i]);
        }
    }
}

std::size_t Logger::record_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_count_;
}

}  // namespace n_body_sim_pro::logging
