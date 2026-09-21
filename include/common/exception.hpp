#pragma once

/// @file exception.hpp
/// @brief HamDB exception hierarchy.
///
/// HamDB uses exceptions only at API boundaries (e.g., Database::open).
/// Internal functions return @c Status codes.  This keeps the hot path
/// free of exception overhead while still giving callers a standard C++
/// error-handling story.

#include "common/enums.hpp"
#include <stdexcept>
#include <string>

namespace hamdb
{

    /**
     * @brief Base class for all HamDB exceptions.
     *
     * Wraps a @c Status code alongside the usual @c what() message so that
     * callers can branch on the kind of failure without string parsing.
     */
    class HamDBException : public std::runtime_error
    {
    public:
        /**
         * @brief Construct with a status code and a descriptive message.
         *
         * @param status  The @c Status that triggered this exception.
         * @param message Human-readable description of the error.
         */
        explicit HamDBException(Status status, const std::string& message);

        /// Return the @c Status code associated with this exception.
        [[nodiscard]] Status status() const noexcept;

    private:
        Status status_; ///< The underlying status code.
    };

    // ── Specialised exception types ───────────────────────────────────────────────

    /// Thrown when a requested page or record cannot be found.
    class NotFoundException : public HamDBException
    {
    public:
        explicit NotFoundException(const std::string& message);
    };

    /// Thrown when an argument passed to an API is invalid.
    class InvalidArgumentException : public HamDBException
    {
    public:
        explicit InvalidArgumentException(const std::string& message);
    };

    /// Thrown when a file-level or OS I/O error occurs.
    class IoException : public HamDBException
    {
    public:
        explicit IoException(const std::string& message);
    };

    /// Thrown when on-disk data is detected to be corrupt.
    class CorruptionException : public HamDBException
    {
    public:
        explicit CorruptionException(const std::string& message);
    };

} // namespace hamdb
