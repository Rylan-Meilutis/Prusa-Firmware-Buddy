/// @file
#pragma once

#include <tool/physical_tool.hpp>
#include <tool/hotend/hotend/dummy_hotend.hpp>

class DummyPhysicalTool final : public PhysicalTool {

public:
    explicit DummyPhysicalTool();

public:
    bool supports_filament(const FilamentTypeParameters &) const override {
        return false;
    }

private:
    DummyHotend hotend_;
};
