#include <option/has_indx.h>

#if HAS_INDX()

    #include "PrusaGcodeSuite.hpp"
    #include <feature/indx_nozzle_cleaner_calibration/indx_nozzle_cleaner_calibration.hpp>
    #include <option/has_side_leds.h>
    #if HAS_SIDE_LEDS()
        #include <leds/side_strip_handler.hpp>
    #endif

void PrusaGcodeSuite::M1983() {
    #if HAS_SIDE_LEDS()
    leds::ScopedActiveLightHold active_light_hold;
    #endif
    indx_nozzle_cleaner_calibration::run();
}

#endif
