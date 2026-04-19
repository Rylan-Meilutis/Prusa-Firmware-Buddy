#include "frame_title_text_image_prompt.hpp"

#include <gui/auto_layout.hpp>
#include <guiconfig/GuiDefaults.hpp>

namespace {

/// Width reserved for the image on the right side
static constexpr int image_width = 160;

static constexpr std::array layout_no_footer {
    StackLayoutItem { .height = 32, .margin_side = 16, .margin_bottom = 4 }, // Title
    StackLayoutItem { .height = 2, .margin_side = 16 }, // Line below title
    StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16, .margin_top = 16 }, // Text
    standard_stack_layout::for_radio,
};

static constexpr std::array layout_only_footer {
    standard_stack_layout::for_footer,
};

static constexpr std::array layout_with_footer = stdext::array_concat(layout_no_footer, layout_only_footer);
static_assert(layout_no_footer.size() + 1 == layout_with_footer.size());

} // namespace

FrameTitleTextImagePrompt::FrameTitleTextImagePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_title, const string_view_utf8 &txt_info, const img::Resource *img_res)
    : title(parent, {}, is_multiline::yes, is_closed_on_click_t::no, txt_title)
    , title_line(parent, {})
    , info(parent, {}, is_multiline::yes, is_closed_on_click_t::no, txt_info)
    , icon(parent, Rect16(), img_res)
    , radio(parent, {}, fsm_phase) {

    title.SetAlignment(Align_t::LeftBottom());
    title.set_font(GuiDefaults::FontBig);
    title.SetTextColor(COLOR_BRAND);

    info.SetAlignment(Align_t::LeftTop());
#if HAS_MINI_DISPLAY()
    info.set_font(Font::small);
#endif

    title_line.SetBackColor(COLOR_DARK_GRAY);

    parent->CaptureNormalWindow(radio);

    std::array<window_t *, layout_no_footer.size()> windows_no_footer { &title, &title_line, &info, &radio };
    layout_vertical_stack(parent->GetRect(), windows_no_footer, layout_no_footer);

    // Hide after layout is calculated - always take 1px line into account, text may be set later
    title_line.set_visible(!txt_title.isNULLSTR());

    // Shrink text width to make room for the image on the right
    const auto text_rect = info.GetRect();
    info.SetRect(Rect16(text_rect.Left(), text_rect.Top(), text_rect.Width() - image_width, text_rect.Height()));

    // Place image to the right of the text
    icon.SetRect(Rect16(text_rect.Left() + text_rect.Width() - image_width, text_rect.Top(), image_width, text_rect.Height()));
}

void FrameTitleTextImagePrompt::add_footer(FooterLine &footer) {
    std::array<window_t *, layout_with_footer.size()> windows_with_footer { &title, &title_line, &info, &radio, &footer };
    layout_vertical_stack(title.GetParent()->GetRect(), windows_with_footer, layout_with_footer);
}
