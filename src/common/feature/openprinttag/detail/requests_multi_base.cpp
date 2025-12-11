#include "requests_multi_base.hpp"

namespace buddy::openprinttag {

void MultiRequestBase::issue() {
    // Call issue() of individual requests
    for (auto *request : requests()) {
        request->issue();
    }
}

} // namespace buddy::openprinttag
