# Integration Tests

## Requirements

1. Python 3.7
2. Make sure you have the project's dependencies downloaded if you haven't built the project before (`python utils/bootstrap.py`)

## Running the suite

Activate the virtualenv, if needed:
```
source .venv/bin/activate
```

Run the test suite
```
pytest tests/integration --firmware <firmware.bin to test>
```

-  Currently supported firmware builds are:
    - MK4, noboot variant
- The first time you run the tests, an OCR library will have to download its models first. This might take some time and you might not see any progress.

### Tips
- See [pytest's documentation](https://docs.pytest.org/en/latest/contents.html#toc)
- Do you want to see what is happening on the simulator's screen? `--enable-graphic` is your friend
- Beware that the tests cache some content (bootstrapped xflash for example). In case you are debugging some issues, clearing the cache using the `--cache-clear` might help.
- You ran the suite, a test failed, you fixed and now want to continue running the suite? Use `--failed-first` flag [doc](https://docs.pytest.org/en/6.2.x/cache.html)
- If you want to see all the logs while the tests are running, use -- for example -- `--log-cli-level info`, or `-s` to disable output capturing.
- Use `-x` to stop on the first failed test. `--pdb` to enter debugger on failure. Use `--gdb` to make the QEMU's GDB server available on port (1234).
- `--simulator <qemu-system-buddy>` lets you specify the simulator's binary to use.

## Serial disconnect fault lab (native, no hardware)

`utils/simulator/serial_fault_lab.py` runs the production progress, session,
inactivity-timeout and serial-finalization functions in an ASan/UBSan C++ process.
It connects those functions to the source RME plugin and the real OctoPrint
G-code reader. Run it with Python containing OctoPrint and the plugin dependencies:

```sh
python utils/simulator/serial_fault_lab.py \
  --plugin-source ../OctoPrint-RME-Compatibility \
  --output-dir /tmp/rme-serial-fault-lab
```

The report and command/state trace are saved as `report.json`. Scenarios cover
healthy progress delivery, missing progress heartbeats with an otherwise live
session, serial inactivity with and without buffered motion, reconnect after
finalization, and a simulated durable executed-offset checkpoint with file-hash
verification using a fresh OctoPrint reader.

This is not a CORE One INDX hardware emulator: time, queue/planner busy flags and
the slicer fallback value are modeled. The checkpoint's executed boundary is
injected, not measured from real motors. No power-panic motion, planner restore,
physical USB failure or automatic recovery is implemented or validated by this
lab. In particular, seeking the last-sent file position is not safe recovery.
The pinned QEMU supports MK4 integration tests, not INDX; do not disable firmware
BOM/safety checks to make another machine model boot.
