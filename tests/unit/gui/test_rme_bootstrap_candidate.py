"""Exercise resource discovery for the one-shot RME firmware filename."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class BootstrapCandidateTests(unittest.TestCase):

    def test_candidate_names_and_revision_validation(self):
        source = (ROOT / "src/resources/bootstrap.cpp").read_text()
        predicate = source.split("static bool is_bootstrap_candidate(", 1)[1]
        predicate = "static bool is_bootstrap_candidate(" + predicate.split(
            "static bool is_relevant_bbf_for_bootstrap", 1)[0]
        program = '#include <string.h>\n#include <cassert>\n' + predicate + """
int main() {
    assert(is_bootstrap_candidate("FWUPD.RME"));
    assert(is_bootstrap_candidate("fwupd.rme"));
    assert(is_bootstrap_candidate("coreone_indx_6.9.2-RME.bbf"));
    assert(is_bootstrap_candidate("COREONE.BBF"));
    assert(!is_bootstrap_candidate("other.RME"));
    assert(!is_bootstrap_candidate("FWUPD.RME.rme-part"));
    assert(!is_bootstrap_candidate("FWUPD.UI"));
    assert(!is_bootstrap_candidate("FWUPD.RME.rme-verified"));
    assert(!is_bootstrap_candidate("no_extension"));
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "candidate_test")
            subprocess.run(["c++", "-x", "c++", "-o", executable, "-"],
                           input=program,
                           text=True,
                           check=True)
            subprocess.run([executable], check=True)
        discovery = source.split("static bool find_suitable_bbf_file", 1)[-1]
        self.assertIn("is_bootstrap_candidate(entry->d_name)", discovery)
        self.assertIn("is_relevant_bbf_for_bootstrap", discovery)
        self.assertIn("return bbf_revision == revision;", source)


if __name__ == "__main__":
    unittest.main()
