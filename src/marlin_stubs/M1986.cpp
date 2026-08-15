#include <option/has_wastebin_fill_tracking.h>
#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <feature/wastebin_watcher/wastebin_watcher.hpp>
#include <gcode/gcode_parser.hpp>
#include <Marlin/src/core/serial.h>
#include <inc/MarlinConfig.h>

static_assert(HAS_WASTEBIN_FILL_TRACKING());

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1986: Empty the INDX nozzle-cleaner wastebin
 *
 * Internal gcode behind the "Empty Wastebin" menu action: parks the head clear of the cleaner,
 * prompts the user to empty the bin and resets the fill counter on confirm. Lives in WastebinWatcher.
 *
 *#### Usage
 *
 *    M1986                 ; prompt to empty the bucket
 *    M1986 Q               ; report count, threshold and capacity
 *    M1986 S1200           ; signal/set the current pellet count
 *    M1986 P2500           ; set the automatic-pause threshold (0 = capacity)
 *    M1986 R               ; reset the pellet count without a prompt
 */
void PrusaGcodeSuite::M1986() {
    auto &watcher = WastebinWatcher::instance();
    const bool set_count = parser.seen('S');
    const bool set_threshold = parser.seen('P');
    const bool reset_count = parser.seen('R');
    const bool query = parser.seen('Q');

    if (set_count) {
        watcher.set_fill_level(parser.ulongval('S'));
    }
    if (set_threshold) {
        const uint32_t threshold = parser.ulongval('P');
        if (threshold > NOZZLE_CLEANER_WASTEBIN_CAPACITY_EXTENDED) {
            SERIAL_ERROR_MSG("M1986 P exceeds maximum wastebin capacity");
            return;
        }
        watcher.set_pause_threshold(threshold);
    }
    if (reset_count) {
        watcher.mark_emptied();
    }

    if (query || set_count || set_threshold || reset_count) {
        SERIAL_ECHOLNPAIR("PURGE_BUCKET pellets=", watcher.fill_level(),
            " threshold=", watcher.pause_threshold(), " capacity=", watcher.capacity());
        return;
    }

    watcher.pause_to_empty(/*full=*/false);
}

/** @}*/
