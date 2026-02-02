/**
 * @file gui_invalidate.hpp
 * @author Radek Vana
 * @brief header including gui_invalidate function, meant for internal use only (inside windows)
 * @date 2022-01-10
 */

#pragma once

void gui_invalidate();

/// Markt the GUI as valid (falsely)
/// Can be used to prevent unnecessary redraws
void gui_validate();
