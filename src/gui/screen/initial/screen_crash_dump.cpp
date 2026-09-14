/// @file
#include <gui/screen/initial/screen_crash_dump.hpp>

#include <ScreenHandler.hpp>
#include <crash_dump/crash_dump_handlers.hpp>
#include <img_resources.hpp>
#include <lang/i18n.h>
#include <transfers/monitor.hpp>
#include <window_msgbox.hpp>

bool ScreenCrashDump::should_show() {
    crash_dump::BufferT dump_buffer;
    return !crash_dump::get_present_dumps(dump_buffer).empty();
}

ScreenCrashDump::ScreenCrashDump()
    : PseudoScreenCallback {
        [] {
            crash_dump::BufferT dump_buffer;
            const auto present_dumps = crash_dump::get_present_dumps(dump_buffer);
            if (present_dumps.empty()) {
                return;
            }
            if (MsgBoxWarning(_("Crash detected. Save it to USB?"
                                "\n\nDo not share the file publicly,"
                                " the crash dump may include unencrypted sensitive information."
                                " Send it to: reports@prusa3d.com"),
                    Responses_YesNo)
                == Response::Yes) {
                auto transfer_slot = transfers::Monitor::instance.allocate(
                    transfers::Monitor::Type::Link, crash_dump::buddy_dump_usb_path, 0);
                if (!transfer_slot) {
                    MsgBoxWarning(_("A file transfer is already in progress. The crash dump was kept; save it after the transfer finishes."), Responses_Ok);
                    return;
                }
                MsgBoxIconned box { GuiDefaults::DialogFrameRect, Responses_NONE, 0, _("Saving to USB"), is_multiline::yes, &img::info_58x58 };
                box.Show();
                // Force a redraw so the box is visible during the blocking save.
                Screens::Access()->Draw();
                for (const auto &dump_handler : present_dumps) {
                    dump_handler->usb_save();
                }
                transfer_slot->done(transfers::Monitor::Outcome::Finished);
                box.Hide();
            }

            for (const auto &dump_handler : present_dumps) {
                dump_handler->remove();
            }
        },
    } {}
