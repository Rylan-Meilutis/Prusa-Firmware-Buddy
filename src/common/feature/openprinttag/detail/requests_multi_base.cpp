#include "requests_multi_base.hpp"

namespace buddy::openprinttag {

void MultiRequestBase::issue() {
    // Call issue() of individual requests
    for (auto *request : requests()) {
        request->issue();
    }

    // Issue a sync request so that we can track when all previous requests are finished
    // MUST BE THE LAST ISSUE OF THE MULTIREQUEST
    sync_request_.issue();
}

} // namespace buddy::openprinttag
