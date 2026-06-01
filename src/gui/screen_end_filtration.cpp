#include "screen_end_filtration.hpp"

#include "ScreenHandler.hpp"
#include <feature/chamber_filtration/chamber_filtration.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <img_resources.hpp>

namespace {
constexpr PhaseResponses responses_end_filtration = { Response::Yes, Response::No, Response::_none, Response::_none };
constexpr Rect16 description_rect {
    30,
    GuiDefaults::HeaderHeight + 35,
    GuiDefaults::ScreenWidth - 60,
    100,
};
} // namespace

ScreenEndFiltration::ScreenEndFiltration()
    : header(this)
    , description(this, description_rect, is_multiline::yes)
    , radio(this, GuiDefaults::GetButtonRect(GetRect()), responses_end_filtration) {
    ClrMenuTimeoutClose();
    CaptureNormalWindow(radio);
    radio.SetBtn(Response::No);

    header.SetIcon(&img::info_16x16);
    header.SetText(_("FILTERING"));

    description.SetAlignment(Align_t::Center());
    description.SetText(_("End post-print filtration early?"));
}

void ScreenEndFiltration::windowEvent([[maybe_unused]] window_t *sender, GUI_event_t event, void *param) {
    if (buddy::chamber_filtration().post_print_remaining_s() == 0) {
        Screens::Access()->Close();
        return;
    }

    if (event != GUI_event_t::CHILD_CLICK) {
        return;
    }

    if (event_conversion_union { .pvoid = param }.response == Response::Yes) {
        buddy::chamber_filtration().stop_post_print_filtration();
    }
    Screens::Access()->Close();
}
