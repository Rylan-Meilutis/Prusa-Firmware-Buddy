/// @file
#pragma once

#include <feature/openprinttag/detail/requests_base.hpp>

namespace buddy::openprinttag {

class MultiRequestBase {

public:
    // Issues/reissues all requests
    void issue();

    inline bool finished() const {
        return sync_request_.finished();
    }

    /// @returns list of all requests in the multirequest (excluding the sync request)
    inline std::span<Request *> requests() {
        return requests_span_;
    }

    inline SyncRequest &sync_request() {
        return sync_request_;
    }

protected:
    MultiRequestBase() = default;

protected:
    /// Should be issued after everything else
    SyncRequest sync_request_;

    /// To be set by the child
    std::span<Request *> requests_span_;
};

} // namespace buddy::openprinttag
