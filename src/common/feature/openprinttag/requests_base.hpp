/// @file
#pragma once

#include <openprinttag/opt_reader.hpp>
#include <utils/uncopyable.hpp>

#include "defines.hpp"

namespace buddy::openprinttag {

/// Base class for any OpenPrintTag reader request.
/// - The system is asynchronous:
///   - The request is enqueued at the moment of creation (issue() needs to be called from the childmost constructor).
///   - Then, it gets @p finished() at some undetermined point, either successfully or with an @p error()
/// - Requests can be created from any thread, the system is thread-safe.
/// - Requests are enqueued/processed in the same order they were created in.
/// - If a request returns an error, other enqueued requests are still executed.
/// - The request needs to be issued by calling @p issue()
class Request : public Uncopyable {

public:
    using Error = ::openprinttag::OPTReader::Error;
    static constexpr auto no_associated_region = std::nullopt;

public:
    /// Issues the request.
    /// If the request is already issues, the issuement is cancelled and it gets reissued again
    void issue() {
        issued_ = true;
        // TODO implementation, this will possibly turn into virtual
    }

    /// @returns whether the request is still running or not
    bool finished() const {
        assert(issued_);
        return finished_;
    }

    /// @returns whether the request has @p finished with an error
    /// The error can be obtained by @p error()
    bool has_error() const {
        assert(finished());
        return error_.has_value();
    }

    /// @returns error if @p has_error (otherwise UB)
    Error error() const {
        assert(finished());
        return *error_;
    }

    /// @returns Region associated with the request, if there is any
    /// This is to help diagnose/recover from the region_corrupt error.
    inline std::optional<Region> region() const {
        return region_;
    }

protected:
    /// This is an kinda-interface base class, cannot be constructed on its own
    Request(std::optional<Region> region)
        : region_(region) {}

protected:
    std::optional<Error> error_;
    std::optional<Region> region_;
    bool issued_ : 1 = false;
    bool finished_ : 1 = false;
};

/// A dumy noop request, useful for synchronization purposes
/// Once @p finished(), all previously enqueued requests must be finished as well
class SyncRequest final : public Request {

public:
    SyncRequest()
        : Request(no_associated_region) {
    }
};

} // namespace buddy::openprinttag
