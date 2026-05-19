/**
 * @file window_wizard_icon.hpp
 */

#pragma once

#include <window.hpp>
#include <selftest_sub_state.hpp>
#include <tool_index.hpp>
#include <option/has_bed_fan.h>
#include <option/has_xbuddy_extension.h>
#include <algorithm>
#include <bitset>
#include <array>

class WindowIconOkNgArray : public window_t {

public:
    // The widget is used both for per-tool icon arrays (PhysicalToolIndex::count) and for
    // fixed-2-icon arrays (xBE cooling fans, bed fans). On a single-toolhead printer with
    // either of those features the per-tool count alone would undersize the storage.
    constexpr static uint8_t max_icon_cnt = std::max<uint8_t>(
        PhysicalToolIndex::count, (HAS_XBUDDY_EXTENSION() || HAS_BED_FAN()) ? 2 : 1);
    constexpr static uint8_t icon_space_width = 20;

    WindowIconOkNgArray(window_t *parent, const point_i16_t pt, uint8_t icon_cnt = 1, const SelftestSubtestState_t state = SelftestSubtestState_t::undef);
    SelftestSubtestState_t GetState(const size_t idx = 0) const { return states[idx]; }
    void SetState(const SelftestSubtestState_t s, const size_t idx = 0);
    void SetIconHidden(const size_t idx, const bool hidden);
    void SetIconCount(const size_t new_icon_cnt);

protected:
    virtual void unconditionalDraw() override;
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    // Encodes the capacity invariant: any printer with a 2-icon callsite must size storage to ≥ 2.
    static_assert(!(HAS_XBUDDY_EXTENSION() || HAS_BED_FAN()) || max_icon_cnt >= 2);
    std::array<SelftestSubtestState_t, max_icon_cnt> states;
    std::bitset<max_icon_cnt> hidden;
    uint8_t icon_cnt;
    uint8_t animation_stage;
};

using WindowIcon_OkNg = WindowIconOkNgArray;
