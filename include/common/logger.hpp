#pragma once

/// @file logger.hpp
/// @brief Minimal structured logging facade for HamDB.
///
/// All engine code calls through this facade so that the logging backend
/// can be swapped without touching any other file.
///
/// @note  The implementation is a stub — messages go to @c stderr.
///        A proper sink-based system will be added in a future milestone.

#include <string_view>

namespace hamdb
{

    /**
     * @brief Severity level for a log message.
     */
    enum class LogLevel : int
    {
        Debug = 0,   ///< Verbose diagnostic information.
        Info = 1,    ///< Normal operational messages.
        Warning = 2, ///< Recoverable anomalies worth noting.
        Error = 3,   ///< Non-fatal errors that affect one operation.
        Fatal = 4,   ///< Unrecoverable errors — the process will terminate.
    };

    /**
     * @brief Global logging facade.
     *
     * Call the free functions below rather than constructing this class directly.
     */
    class Logger
    {
    public:
        Logger() = delete;
        ~Logger() = delete;

        /// Log a message at the given severity level.
        static void log(LogLevel level, std::string_view message) noexcept;

        /// Return the current minimum log level (messages below are suppressed).
        [[nodiscard]] static LogLevel minLevel() noexcept;

        /// Set the minimum log level.
        static void setMinLevel(LogLevel level) noexcept;
    };

    // ── Convenience free functions ────────────────────────────────────────────────

    /// Log at Debug level.
    void logDebug(std::string_view msg) noexcept;

    /// Log at Info level.
    void logInfo(std::string_view msg) noexcept;

    /// Log at Warning level.
    void logWarning(std::string_view msg) noexcept;

    /// Log at Error level.
    void logError(std::string_view msg) noexcept;

    /// Log at Fatal level.
    void logFatal(std::string_view msg) noexcept;

} // namespace hamdb
