// gui.hpp
#pragma once

#include "guitypes.hpp"
#include "display_helper.h"
#include <guiconfig/GuiDefaults.hpp>

extern void gui_run(void);

extern void gui_init(void);

extern void gui_redraw(void);

#include "window.hpp"
#include "window_frame.hpp"
#include "window_text.hpp"
#include "window_roll_text.hpp"
#include "window_numb.hpp"
#include "window_icon.hpp"
#include "window_term.hpp"
#include "window_msgbox.hpp"
#include "window_progress.hpp"

extern uint8_t gui_get_nesting(void);

/// Reset the loop timer, triggering a LOOP immediately before the next draw
void gui_reset_loop_timer();

extern void gui_loop(void);
extern void gui_error_run(void);

extern void gui_bare_loop(void);
