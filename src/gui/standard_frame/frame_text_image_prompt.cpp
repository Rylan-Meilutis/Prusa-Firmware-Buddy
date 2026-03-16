#include "frame_text_image_prompt.hpp"
#include "auto_layout.hpp"

namespace {

/// Width reserved for the image on the right side
static constexpr int image_width = 140;

static constexpr std::array layout_no_footer {
    // Text area on the left
    StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16, .margin_top = 16 },
    standard_stack_layout::for_radio,
};

static constexpr std::array layout_only_footer {
    standard_stack_layout::for_footer,
};

static constexpr std::array layout_with_footer = stdext::array_concat(layout_no_footer, layout_only_footer);
static_assert(layout_no_footer.size() + 1 == layout_with_footer.size());

} // namespace

FrameTextImagePrompt::FrameTextImagePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_info, const img::Resource *img_res)
    : info(parent, {}, is_multiline::yes, is_closed_on_click_t::no, txt_info)
    , icon(parent, Rect16(), img_res)
    , radio(parent, {}, fsm_phase) {

    info.SetAlignment(Align_t::LeftCenter());
#if HAS_MINI_DISPLAY()
    info.set_font(Font::small);
#endif

    parent->CaptureNormalWindow(radio);

    // Layout text and radio vertically
    std::array<window_t *, layout_no_footer.size()> windows_no_footer { &info, &radio };
    layout_vertical_stack(parent->GetRect(), windows_no_footer, layout_no_footer);

    // Shrink text width to make room for the image on the right
    const auto text_rect = info.GetRect();
    info.SetRect(Rect16(text_rect.Left(), text_rect.Top(), text_rect.Width() - image_width, text_rect.Height()));

    // Place image to the right of the text
    icon.SetRect(Rect16(text_rect.Left() + text_rect.Width() - image_width, text_rect.Top(), image_width, text_rect.Height()));
}

void FrameTextImagePrompt::add_footer(FooterLine &footer) {
    std::array<window_t *, layout_with_footer.size()> windows_with_footer { &info, &radio, &footer };
    layout_vertical_stack(info.GetParent()->GetRect(), windows_with_footer, layout_with_footer);
}
