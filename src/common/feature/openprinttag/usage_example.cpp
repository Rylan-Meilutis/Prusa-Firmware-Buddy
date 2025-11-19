#include "requests_read.hpp"

namespace opt = buddy::openprinttag;

void opt_usage_example() {
    const auto tag_opt = opt::ToolTag::for_tool(VirtualToolIndex::from_raw(0));
    if (!tag_opt) {
        return;
    }

    const auto tag = *tag_opt;

    std::array<char, 64> material_name_buffer;

    opt::ReadStringRequestBase material_name { tag.field(opt::MainField::material_name), material_name_buffer };
    opt::ReadFloatRequest density { tag.field(opt::MainField::density) };

    opt::SyncRequest sync;

    material_name.issue();
    density.issue();
    sync.issue();

    while (!sync.finished()) {
        // Wait for the request group to finish
        // We could possibly give the request group a semaphore or something as well
    }

    printf("%s", material_name.result()->data());
}
