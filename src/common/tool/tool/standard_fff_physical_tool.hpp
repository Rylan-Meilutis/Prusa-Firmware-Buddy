/// @file
#pragma once

#include <tool/physical_tool.hpp>

class StandardFFFPhysicalToolBase : public PhysicalTool {

public:
    bool supports_filament(const FilamentTypeParameters &filament) const override;

protected:
    explicit StandardFFFPhysicalToolBase(Hotend &hotend);
};

template <typename Hotend>
class StandardFFFPhysicalTool final : public StandardFFFPhysicalToolBase {

public:
    explicit StandardFFFPhysicalTool(PhysicalToolIndex tool_index, const Hotend::Config *hotend_config)
        : StandardFFFPhysicalToolBase(hotend_)
        , hotend_(tool_index, hotend_config) {}

    Hotend &hotend() { return hotend_; }

private:
    Hotend hotend_;
};
