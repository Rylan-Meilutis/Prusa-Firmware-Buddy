/// @file
#pragma once

#include <async_job/async_job.hpp>
#include <basic_screen_menu.hpp>
#include <e2ee/identity_check_levels.hpp>
#include <WindowMenuInfo.hpp>

class MI_KEY final : public WI_INFO_t {
    constexpr static const char *const label = N_("Key status");

public:
    MI_KEY();
    virtual void Loop() override;
};

class MI_KEYGEN final : public IWindowMenuItem {
    constexpr static const char *const label = N_("Generate new key");

    AsyncJobWithResult<bool> key_generation;

public:
    MI_KEYGEN();

protected:
    virtual void click(IWindowMenu &window_menu) override;
};

class MI_EXPORT final : public IWindowMenuItem {
    constexpr static const char *const label = N_("Export public key");

public:
    MI_EXPORT();

protected:
    virtual void click(IWindowMenu &window_menu) override;
};

#if 0
    // #error dead code found by automatic analyses (see BFW-5461)
// Disabled for now, because the identity checking is not complete
// having it disabled and the default being Accept all means, that
// the feature is invisible for users.
class MI_IDENTITY_CHECKING : public WI_SWITCH_t<3> {
    constexpr static const char *const label = N_("Identity checking");

    constexpr static const char *str_Known = N_("Known only");
    constexpr static const char *str_Ask = N_("Ask");
    constexpr static const char *str_All = N_("Accept all");

public:
    MI_IDENTITY_CHECKING();
    virtual void OnChange(size_t old_index) override;
};
#endif

using ScreenMenuE2eeBase = BasicScreenMenu<
    MI_KEY,
    MI_KEYGEN,
    MI_EXPORT>;

class ScreenMenuE2ee final : public ScreenMenuE2eeBase {
public:
    ScreenMenuE2ee();
};
