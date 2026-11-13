"""Check production probe labels retain actual point counts, never estimates."""
import unittest
import test_serial_pause_recovery as fixture


class SerialProbePoints(unittest.TestCase):
    run_cpp = fixture.SerialPauseRecovery.run_cpp

    def test_actual_point_labels_and_invalid_counts(self):
        source = (
            fixture.ROOT /
            "src/feature/print_status_message/print_status_message_mgr.cpp"
        ).read_text()
        self.run_cpp(r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
''' + fixture.function(
            source, "static const char *serial_probe_progress_label(") + r'''
int main() {
 assert(!strcmp(serial_probe_progress_label("Probing bed",3,25),"Probing bed 3/25 points"));
 assert(!strcmp(serial_probe_progress_label("Additional probing",1,2),"Additional probing 1/2 points"));
 assert(!strcmp(serial_probe_progress_label("Probing bed",100,100),"Probing bed 100/100 points"));
 assert(!serial_probe_progress_label("Probing bed",0,0));
 assert(!serial_probe_progress_label("Probing bed",26,25));
 assert(!serial_probe_progress_label("Probing bed",-1,25));
 assert(!serial_probe_progress_label("Probing bed",1.5f,25));
 assert(!serial_probe_progress_label("Probing bed",1,70000));
 assert(!serial_probe_progress_label("Probing bed",1,std::numeric_limits<float>::quiet_NaN()));
}
''')
        report = fixture.function(source,
                                  "void report_print_status_to_serial_host(")
        self.assertIn("SerialPrinting::notify_status(points, progress, true)",
                      report)
        self.assertIn("PrintStatusMessage::additional_probing", report)


if __name__ == "__main__":
    unittest.main()
