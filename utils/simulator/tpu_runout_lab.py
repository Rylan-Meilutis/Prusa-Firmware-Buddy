"""Reproduce pressure-monitor baseline ambiguity with synthetic TPU traces.

This does not emulate an INDX head, the physical presence sensor, or melt flow.
It compiles the actual selected checkout's extrusion_calibration.cpp, not a
reimplementation of the detector. No production settings or thresholds change.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    source = root / 'src/feature/extrusion_calibration.cpp'
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rme-tpu-runout-') as temporary:
        binary = Path(temporary) / 'tpu-runout-lab'
        command = [
            'c++',
            '-std=c++20',
            '-O1',
            '-g',
            '-pthread',
            '-fsanitize=address,undefined',
            '-I' + str(ROOT / 'tests/unit/common/extrusion_calibration/stubs'),
            '-I' + str(root / 'src/common'),
            '-I' + str(root / 'src'),
            str(source),
            str(Path(__file__).with_suffix('.cpp')),
            '-o',
            str(binary),
        ]
        if 'set_pressure_monitor_purge_context' in source.with_suffix(
                '.hpp').read_text():
            command.insert(1, '-DLAB_PURGE_CONTEXT=1')
        subprocess.run(command, check=True)
        environment = dict(os.environ)
        # LeakSanitizer cannot run under some sandbox ptrace configurations;
        # address/undefined checks remain enabled with immediate failure.
        environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
        execution = subprocess.run([str(binary)],
                                   env=environment,
                                   text=True,
                                   capture_output=True,
                                   check=True)
    rows = [json.loads(line) for line in execution.stdout.splitlines()]
    assert len(rows) in (5, 7)
    report = dict(
        source=str(source),
        source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        scenarios=rows,
        limitations=
        'Synthetic load and executed E; not a hardware/thermal model. '
        'A reproduced detector failure class does not prove the cause of the '
        'reported print without its trigger or raw load trace.',
    )
    (args.output_dir /
     'report.json').write_text(json.dumps(report, indent=2) + '\n')
    (args.output_dir / 'trace.jsonl').write_text(execution.stdout)
    print(execution.stdout, end='')
    print(report['limitations'])


if __name__ == '__main__':
    main()
