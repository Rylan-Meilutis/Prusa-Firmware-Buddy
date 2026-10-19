"""Execute the production motherboard digest path with injected I/O outcomes."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class CyphalDigestRetryTests(unittest.TestCase):

    def run_case(self, driver):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        source = (root / "src/puppies/cyphal_flash_host.cpp").read_text()
        start = source.index(
            "CommunicationStatus CyphalBridgeFlashHost::write_digest(")
        end = source.index("\n}\n\n} // namespace", start) + 2
        code = r'''
#include <cassert>
#include <cstdint>
#include <functional>
namespace modbus { enum class ServerAddress { bridge }; }
namespace xbuddy_extension {
enum class FileId { none, sensor };
enum class DigestStatus : uint16_t { ok, unavailable, retry };
namespace modbus {
struct DigestRequest { int file_id = 0; int salt = 0; bool operator==(const DigestRequest &) const = default; };
struct Digest { DigestRequest request; uint16_t status; };
FileId parse_file_id(int id) { return id == 1 ? FileId::sensor : FileId::none; }
DigestStatus parse_digest_status(uint16_t value) { return DigestStatus(value); }
}}
using FileId = xbuddy_extension::FileId;
enum class CommunicationStatus { OK, SKIPPED, ERROR };
struct PuppyModbus {
    bool write_ok = true;
    int writes = 0;
    bool write_holding_registers(modbus::ServerAddress, const xbuddy_extension::modbus::Digest &) { ++writes; return write_ok; }
};
struct CyphalBridgeFlashHost {
    using DigestComputeFn = std::function<void(xbuddy_extension::modbus::DigestRequest, FileId, xbuddy_extension::modbus::Digest &)>;
    bool valid = true;
    xbuddy_extension::modbus::DigestRequest current_digest_request {1, 42}, last_digest_request {};
    CommunicationStatus write_digest(PuppyModbus &, modbus::ServerAddress, DigestComputeFn);
};
''' + source[start:end] + r'''
int main() {
    CyphalBridgeFlashHost host;
    PuppyModbus bus;
    int computes = 0;
    auto status = xbuddy_extension::DigestStatus::ok;
    auto compute = [&](auto request, auto, auto &out) {
        ++computes; out.request = request; out.status = uint16_t(status);
    };
    auto send = [&] { return host.write_digest(bus, modbus::ServerAddress::bridge, compute); };
''' + driver + "\n}\n"
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cpp"
            executable = pathlib.Path(directory) / "test"
            cpp.write_text(code)
            subprocess.run(
                [compiler, "-std=c++20",
                 str(cpp), "-o",
                 str(executable)],
                check=True)
            subprocess.run([str(executable)], check=True, timeout=5)

    def test_retry_response_is_not_cached_as_success(self):
        self.run_case(r'''
    status = xbuddy_extension::DigestStatus::retry;
    assert(send() == CommunicationStatus::OK);
    status = xbuddy_extension::DigestStatus::ok;
    assert(send() == CommunicationStatus::OK);
    assert(computes == 2 && bus.writes == 2);
    assert(send() == CommunicationStatus::SKIPPED);
    assert(computes == 2);
''')

    def test_stale_request_does_not_compute_or_write(self):
        self.run_case(r'''
    host.valid = false;
    assert(send() == CommunicationStatus::SKIPPED);
    assert(computes == 0 && bus.writes == 0);
''')

    def test_no_request_bus_loss_salt_change_and_unavailable(self):
        self.run_case(r'''
    host.current_digest_request = {};
    assert(send() == CommunicationStatus::SKIPPED);
    assert(computes == 0);
    host.current_digest_request = {1, 43};
    bus.write_ok = false;
    assert(send() == CommunicationStatus::SKIPPED);
    bus.write_ok = true;
    assert(send() == CommunicationStatus::OK);
    assert(computes == 2);
    assert(send() == CommunicationStatus::SKIPPED);
    host.current_digest_request.salt = 44;
    status = xbuddy_extension::DigestStatus::unavailable;
    assert(send() == CommunicationStatus::OK);
    assert(send() == CommunicationStatus::SKIPPED);
    assert(computes == 3);
''')
