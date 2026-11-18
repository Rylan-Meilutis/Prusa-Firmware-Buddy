#include "gcode/gcode.h"
#include "gcode/queue.h"
#include "PrusaGcodeSuite.hpp"
#include <common/sys.hpp>
#include <string.h>
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include "data_exchange.hpp"
#include <firmware_cleanup_receipt.hpp>
#include "serial_printing.hpp"
#include "serial_remote_control.hpp"
#include <transfers/monitor.hpp>
#include "core/serial.h"
#include <option/has_usb_device.h>
#if HAS_USB_DEVICE()
    #include <tusb.h>
#endif

static void update_main_board(bool update_older, const char *sfn) {
    // Hold the storage latch through TX drain and reset, not merely through
    // marker creation. No new staging may replace the armed candidate.
    auto handoff_slot = transfers::Monitor::instance.allocate(
        transfers::Monitor::Type::Link, "/usb/FWUPD.CLN", 0);
    if (!handoff_slot) {
        SERIAL_ERROR_MSG("M997 blocked: transfer in progress");
        return;
    }
    if (*sfn) { // Flash selected BBF
        char selected_sfn[13] {};
        strlcpy(selected_sfn, sfn, sizeof(selected_sfn));
        if ((strcasecmp(selected_sfn, "FWUPD.RME") == 0 || strcasecmp(selected_sfn, "FWUPD.BBF") == 0)
            && !firmware_update_handoff::clear_cleanup_authorization()) {
            SERIAL_ERROR_MSG("M997 could not clear prior cleanup authorization");
            return;
        }
        // Uploads stay neutral until this explicit installation request.
        // The final handoff must be a .BBF: after application flashing the
        // resource/bootloader bootstrap also discovers its payload by suffix.
        const bool managed_candidate = strcasecmp(selected_sfn, "FWUPD.RME") == 0
            || strcasecmp(selected_sfn, "FWUPD.BBF") == 0;
        if (managed_candidate) {
            static constexpr const char *marker_path = "/usb/FWUPD.UI";
            static constexpr const char *marker_temp_path = "/usb/FWUPD.UI.tmp";
            remove(marker_temp_path);
            FILE *marker = fopen(marker_temp_path, "wb");
            if (!marker) {
                SERIAL_ERROR_MSG("M997 could not arm one-shot firmware cleanup");
                return;
            }
            setvbuf(marker, nullptr, _IONBF, 0);
            bool marker_ready = fflush(marker) == 0 && fsync(fileno(marker)) == 0;
            marker_ready = fclose(marker) == 0 && marker_ready;
            if (!marker_ready) {
                remove(marker_temp_path);
                SERIAL_ERROR_MSG("M997 could not persist one-shot firmware cleanup");
                return;
            }

            // Arm RAM first, then atomically publish the marker.  The Marlin
            // server watches the marker from another task; publishing it
            // before the retained request allowed that task to delete the
            // candidate during the pre-reset TX drain.
            data_exchange::set_reflash_bbf_sfn(selected_sfn);
            // Persist a separate non-empty receipt while the pre-reset RAM
            // guard is already armed. A missing primary marker after reboot
            // must not strand the consumed candidate indefinitely.
            if (!firmware_update_handoff::persist_cleanup_receipt()) {
                remove(marker_temp_path);
                data_exchange::clear_reflash_bbf_sfn();
                SERIAL_ERROR_MSG("M997 could not persist cleanup receipt");
                return;
            }
            if (strcasecmp(selected_sfn, "FWUPD.RME") == 0) {
                // Never overwrite another BBF. Keep the neutral request armed
                // while publishing the final name so concurrent cleanup cannot
                // remove either image before reset.
                struct stat destination {};
                errno = 0;
                if (stat("/usb/FWUPD.BBF", &destination) == 0 || errno != ENOENT
                    || rename("/usb/FWUPD.RME", "/usb/FWUPD.BBF") != 0) {
                    // Only drop the RAM guard after durable authorization has
                    // been cleared. An I/O failure must not authorize cleanup
                    // of the still-staged candidate during this boot.
                    if (firmware_update_handoff::clear_cleanup_authorization()) {
                        data_exchange::clear_reflash_bbf_sfn();
                    }
                    SERIAL_ERROR_MSG("M997 could not publish firmware BBF");
                    return;
                }
                strlcpy(selected_sfn, "FWUPD.BBF", sizeof(selected_sfn));
                data_exchange::set_reflash_bbf_sfn(selected_sfn);
            }
            if (rename(marker_temp_path, marker_path) != 0) {
                // The independently committed receipt is sufficient. Do not
                // report an aborted flash after publishing its authorization.
                remove(marker_temp_path);
                SERIAL_ECHOLNPGM("echo:M997 using cleanup receipt fallback");
            }
        } else {
            data_exchange::set_reflash_bbf_sfn(selected_sfn);
        }
        if (managed_candidate && serial_remote_control::session_active()) {
            SERIAL_ECHOLNPGM("RME_FIRMWARE candidate=1 armed=1 state=restarting path=FWUPD.BBF");
        }
    } else {
        if (update_older) {
            data_exchange::fw_update_older_on_restart_enable();
        } else {
            data_exchange::fw_update_on_restart_enable();
        }
    }

    if (serial_remote_control::session_active()) {
        SerialPrinting::notify_workflow("firmware_update", "restarting", "Firmware staged; USB will reconnect after installation", 100);
        SERIAL_ECHOLNPGM("RME_FIRMWARE_RESTART reconnect=1");
    }
    queue.ok_to_send();
    // M997 intentionally removes the USB CDC device.  Drain the final
    // acknowledgement and RME reconnect marker before resetting so a serial
    // host can distinguish the expected firmware-update reboot from a broken
    // connection and wait for USB re-enumeration.
    SERIAL_FLUSHTX();
#if HAS_USB_DEVICE()
    // A warm MCU reset can be too short for the host to observe USB removal,
    // leaving a stale ttyACM device that cannot be opened after installation.
    // Force a clean detach before reset; normal startup reconnects TinyUSB.
    tud_disconnect();
#endif
    HAL_Delay(250);
    sys_reset();
}

static void M997_no_parser(unsigned int module_number, [[maybe_unused]] unsigned int address, bool force_update_older, const char *sfn) {
    switch (module_number) {
    case 0:
        update_main_board(force_update_older, sfn);
        break;
    default:
        break;
    }
}

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M997: Perform in-application firmware update <a href="https://reprap.org/wiki/G-code#M997:_Perform_in-application_firmware_update">M997: Perform in-application firmware update</a>
 *
 *#### Usage
 *
 *    M997 [ O | S | B ]
 *
 *#### Parameters
 *
 * - `O` - Update older or same firmware on restart == force reflash == from menu
 * - `S` - Firmware module number(s), default 0
 *   - `0` - main firmware.
 *   - `1` - Reserved, check reprap wiki
 *   - `2` - Reserved, check reprap wiki
 *   - `3` - Reserved, check reprap wiki
 *   - `4` - Reserved, check reprap wiki
 * - `B` - Expansion board address, default 0
 *       - Currently unused, defined just to be reprap compatible
 *
 * - '/' - Selected BBF SFN (short file name)
 *
 * Default values are used for omitted arguments.
 */
void PrusaGcodeSuite::M997() {

    if (transfers::Monitor::instance.id().has_value()) {
        SERIAL_ERROR_MSG("M997 blocked: transfer in progress");
        return;
    }

    char sfn[13] = { '\0' };
    const char *file_path_ptr = nullptr;
    static constexpr const char *const usb_str = "/usb/";
    size_t prefix_len = strlen(usb_str);

    if (parser.ulongval('S', 0) == 0) {
        if ((file_path_ptr = strstr(parser.string_arg, usb_str)) != nullptr) {
            if (*(file_path_ptr + prefix_len)) {
                strlcpy(sfn, file_path_ptr + prefix_len, sizeof(sfn));
            }
        }
    }

    // NOTICE: Keep in mind, that parser.seen('B') can be triggered by the filename in path of '/' parameter
    M997_no_parser(parser.ulongval('S', 0), parser.ulongval('B', 0), parser.seen('O'), sfn);
}

/** @}*/
