#include "radio_button.hpp"

std::optional<size_t> RadioButton::IndexFromResponse(Response btn) const {
    for (size_t i = 0; i < maxSize(); ++i) {
        if (btn == (responses)[i]) {
            return i;
        }
    }
    return std::nullopt;
}

Response RadioButton::responseFromIndex(size_t index) const {
    if (index >= maxSize()) {
        return Response::_none;
    }
    return (responses)[index];
}

void RadioButton::Change(Responses_t resp) {
    if (responses == resp) {
        return;
    }
    responses = resp;
    SetBtnCount(fixed_width_buttons_count > 0 ? fixed_width_buttons_count : cnt_responses(responses));

    // in iconned layout index will stay
    if (fixed_width_buttons_count == 0) {
        SetBtnIndex(0);
    }

    validateBtnIndex();

    invalidateWhatIsNeeded();
}

// TODO: REMOVEME completely BFW-6028
#if MAX_RESPONSES != 4
void RadioButton::Change(const PhaseResponses &resp) {
    Change(generateResponses(resp));
}
#endif

RadioButton::RadioButton(window_t *parent, Rect16 rect)
    : RadioButton(parent, rect, Responses_t({ { Response::_none, Response::_none, Response::_none, Response::_none } })) {
}

// TODO: REMOVEME completely BFW-6028
#if MAX_RESPONSES != 4
RadioButton::RadioButton(window_t *parent, Rect16 rect, const PhaseResponses &resp)
    : RadioButton(parent, rect, generateResponses(resp)) {
}
#endif

RadioButton::RadioButton(window_t *parent, Rect16 rect, Responses_t resp)
    : IRadioButton(parent, rect, cnt_responses(resp))
    , responses(resp) {
}
