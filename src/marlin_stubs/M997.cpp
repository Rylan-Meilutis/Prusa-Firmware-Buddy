#include "gcode/gcode.h"
#include "gcode/queue.h"
#include "PrusaGcodeSuite.hpp"
#include <common/sys.hpp>
#include <string.h>
#include <cstdio>
#include <unistd.h>
#include "data_exchange.hpp"
#include "serial_printing.hpp"
#include "serial_remote_control.hpp"
#include <transfers/monitor.hpp>
#include "core/serial.h"
#include <option/has_usb_device.h>
#if HAS_USB_DEVICE()
    #include <tusb.h>
#endif

static void update_main_board(bool update_older, const char *sfn) {
    if (*sfn) { // Flash selected BBF
        char selected_sfn[13] {};
        strlcpy(selected_sfn, sfn, sizeof(selected_sfn));
        // Preserve direct M998/legacy M997 compatibility while ensuring the
        // bootloader cannot auto-discover the staged file on a later reboot.
        if (strcasecmp(selected_sfn, "FWUPD.BBF") == 0) {
            remove("/usb/FWUPD.RME");
            remove("/usb/FWUPD.RME.rme-verified");
            remove("/usb/FWUPD.RME.rme-verified-tmp");
            if (rename("/usb/FWUPD.BBF", "/usb/FWUPD.RME") != 0) {
                SERIAL_ERROR_MSG("M997 could not secure staged firmware");
                return;
            }
            strlcpy(selected_sfn, "FWUPD.RME", sizeof(selected_sfn));
        }
        // FWUPD.RME is deliberately not a .BBF: the bootloader must only open
        // it through this retained, explicitly selected one-shot request.
        // Create the cleanup marker here, in the same operation that arms the
        // reboot, rather than while a file is merely being staged.
        if (strcasecmp(selected_sfn, "FWUPD.RME") == 0) {
            FILE *marker = fopen("/usb/FWUPD.UI", "wb");
            if (!marker) {
                SERIAL_ERROR_MSG("M997 could not arm one-shot firmware cleanup");
                return;
            }
            setvbuf(marker, nullptr, _IONBF, 0);
            bool marker_ready = fflush(marker) == 0 && fsync(fileno(marker)) == 0;
            marker_ready = fclose(marker) == 0 && marker_ready;
            if (!marker_ready) {
                remove("/usb/FWUPD.UI");
                SERIAL_ERROR_MSG("M997 could not persist one-shot firmware cleanup");
                return;
            }
        }
        data_exchange::set_reflash_bbf_sfn(selected_sfn);
        if (strcasecmp(selected_sfn, "FWUPD.RME") == 0 && serial_remote_control::session_active()) {
            SERIAL_ECHOLNPGM("RME_FIRMWARE candidate=1 armed=1 state=restarting path=FWUPD.RME");
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
