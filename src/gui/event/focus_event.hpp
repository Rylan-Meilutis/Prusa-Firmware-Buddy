#pragma once

#include "gui_event.hpp"

namespace gui_event {

/// Received when an element receives focus
struct FocusInEvent {
    enum class Reason {
        /// It is not specified how the item gained the focus
        unspecified,

        /// The item gained the focus by the "focus next" logic (positive knob diff)
        /// This would indicate that we want for example focus the first child of the item
        forward_focus_chain,

        /// The item gained the focus by the "focus previous" logic (negative knob diff)
        /// This would indicate that we want for example focus the last child of the item
        reverse_focus_chain,
    };

    Reason reason = Reason::unspecified;

    inline bool operator==(const FocusInEvent &) const = default;
};
static_assert(GuiEventType<FocusInEvent>);

/// Received when an event loses focus
struct FocusOutEvent {
    inline bool operator==(const FocusOutEvent &) const = default;
};
static_assert(GuiEventType<FocusOutEvent>);

}; // namespace gui_event
