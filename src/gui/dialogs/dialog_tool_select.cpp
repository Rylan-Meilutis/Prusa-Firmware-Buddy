/// @file
#include "dialog_tool_select.hpp"

#include <algorithm>

#include <window_menu_adv.hpp>
#include <window_menu_virtual.hpp>
#include <window_menu_callback_item.hpp>
#include <WindowMenuItems.hpp>
#include <utils/variant_utils.hpp>
#include <img_resources.hpp>
#include <ScreenHandler.hpp>
#include <window_header.hpp>
#include <dynamic_index_mapping.hpp>

namespace {

enum class Item {
    return_,
    tool
};

static constexpr auto index_mapping_items = std::to_array<DynamicIndexMappingRecord<Item>>({
    { Item::return_, DynamicIndexMappingType::optional_item },
    { Item::tool, DynamicIndexMappingType::dynamic_section },
});

class SelectToolMenu : public WindowMenuVirtual<MI_RETURN, WindowMenuCallbackItem> {

public:
    SelectToolMenu(window_frame_t *parent, Rect16 rect, const SelectToolDialogArgs &args)
        : WindowMenuVirtual(parent, rect, args.allow_return ? CloseScreenReturnBehavior::yes : CloseScreenReturnBehavior::no)
        , args_(args) {

        index_mapping_.set_item_enabled<Item::return_>(args.allow_return);

        // Set item count to cover the highest active tool
        for (auto tool : VirtualToolIndex::all().skip_all_disabled()) {
            index_mapping_.set_section_size<Item::tool>(tool.to_raw() + 1);
        }

        setup_items();
    }

    int item_count() const final {
        return index_mapping_.total_item_count();
    }

    void setup_item(ItemVariant &variant, int index) final {
        const auto m = index_mapping_.from_index(index);

        switch (m.item) {

        case Item::return_:
            variant.emplace<MI_RETURN>();
            break;

        case Item::tool: {
            const auto tool = VirtualToolIndex::from_raw(m.pos_in_section);

            const auto callback = [this, tool] {
                result = tool;
                Screens::Access()->Close();
            };

            WindowMenuCallbackItem &item = variant.emplace<WindowMenuCallbackItem>(string_view_utf8 {}, callback);
            item.SetLabel(tool.display_name(item.string_view_params));
            item.set_enabled(tool.is_enabled() && args_.tool_filter(tool));

            if (stdext::holds_value(VirtualToolIndex::currently_selected(), tool)) {
                item.SetIconId(&img::arrow_right_10x16);
            }
            break;
        }
        }
    }

public:
    std::optional<VirtualToolIndex> result;

private:
    const SelectToolDialogArgs args_;
    DynamicIndexMapping<index_mapping_items> index_mapping_;
};

class SelectToolDialog final : public IDialog {

public:
    SelectToolDialog(const SelectToolDialogArgs &args)
        : header_(this)
        , menu_(this, GuiDefaults::RectScreenNoHeader, args) {
        CaptureNormalWindow(menu_);
    }

    auto result() const {
        return menu_.menu.result;
    }

private:
    window_header_t header_;
    WindowExtendedMenu<SelectToolMenu> menu_;
};
} // namespace

std::optional<VirtualToolIndex> select_tool_dialog(const SelectToolDialogArgs &args) {
    // No point selecting if there is only one tool enabled
    if (auto t = VirtualToolIndex::single_enabled_tool(); t.has_value() && args.tool_filter(*t)) {
        return *t;
    }

    SelectToolDialog d(args);
    Screens::Access()->gui_loop_until_dialog_closed();
    return d.result();
}
