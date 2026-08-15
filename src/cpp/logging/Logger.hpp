#pragma once

#include <cstdarg>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace n_body_sim_pro::logging {

/*
 * Structured logging for the application layer.
 *
 * The numerical engine reports failures through explicit status codes and
 * never logs from hot loops; this logger serves the application, simulation
 * controller, and instrumentation layers. Records carry a severity, a
 * category, a thread id, and a formatted message, and are kept in a bounded
 * ring buffer that the developer console renders. A console sink mirrors
 * records at or above a configurable threshold to stderr.
 *
 * All functions are thread-safe. Logging is guarded by a severity check
 * before any work is done, so disabled levels are nearly free.
 */

enum class Level {
    Off = 0,
    Error,
    Warning,
    Info,
    Debug,
    Trace,
    Performance,
    Instrumentation
};

enum class Category {
    Simulation,
    Threading,
    Simd,
    Tree,
    Physics,
    Memory,
    Numerics,
    Render,
    Application,
    System
};

struct Record {
    Level level;
    Category category;
    unsigned int thread_id;
    double timestamp_seconds;
    std::string message;
};

const char* level_name(Level level);
const char* category_name(Category category);

/* Ring buffer size (number of retained records). */
constexpr std::size_t kLogBufferCapacity = 2048;

class Logger final {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_minimum_level(Level level);
    Level minimum_level() const { return minimum_level_; }

    void set_console_enabled(bool enabled) { console_enabled_ = enabled; }

    bool is_enabled(Level level) const {
        return level != Level::Off && level <= minimum_level_;
    }

    void log(Level level, Category category, const char* format, ...);
    void logv(Level level, Category category, const char* format, std::va_list arguments);

    /* Snapshot of the retained records, newest last. */
    void copy_records(std::vector<Record>& destination) const;

    std::size_t record_count() const;

private:
    Logger();

    Level minimum_level_ = Level::Info;
    bool console_enabled_ = true;
    unsigned int next_sequence_ = 0;
    mutable std::mutex mutex_;
    Record buffer_[kLogBufferCapacity];
    std::size_t buffer_count_ = 0;
    std::size_t buffer_next_ = 0;
    double session_start_seconds_;
};

/*
 * Convenience helpers. Each is guarded by Logger::is_enabled so a disabled
 * level compiles to a fast level check.
 */
#define N_BODY_SIM_PRO_LOG(level, category, ...)                                           \
    do {                                                                           \
        if (n_body_sim_pro::logging::Logger::instance().is_enabled(level)) {               \
            n_body_sim_pro::logging::Logger::instance().log(level, category, __VA_ARGS__); \
        }                                                                          \
    } while (0)

}  // namespace n_body_sim_pro::logging
