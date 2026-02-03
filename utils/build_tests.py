#!/usr/bin/env python3
"""
Build unit tests for Buddy firmware

Simple wrapper that handles:
- CMake configuration
- Parallel ninja builds with auto-detected core count
- Building specific test targets

After building, run tests manually with ctest:
    cd build_tests && ctest
    cd build_tests && ctest -R gcode  # run specific tests
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_project_root():
    """Find the project root directory."""
    script_dir = Path(__file__).parent.resolve()
    return script_dir.parent


def setup_tool_from_bootstrap(tool_name, project_root):
    """Try to setup a tool from bootstrap.py dependencies."""
    bootstrap_py = project_root / 'utils' / 'bootstrap.py'

    if not bootstrap_py.exists():
        return False

    try:
        result = subprocess.run([
            sys.executable,
            str(bootstrap_py), '--print-dependency-directory', tool_name
        ],
                                capture_output=True,
                                text=True,
                                check=False)

        if result.returncode != 0:
            return False

        tool_dir = Path(result.stdout.strip())
        if tool_name == 'cmake':
            tool_dir = tool_dir / 'bin'

        if tool_dir.exists():
            os.environ['PATH'] = f"{tool_dir}:{os.environ['PATH']}"
            return True
    except Exception:
        pass

    return False


def check_tool_in_system(tool_name):
    """
    Check if a tool is available on the system

    Returns True if tool is available, False otherwise.
    """
    # First check if tool is already available
    if subprocess.run(['which', tool_name],
                      capture_output=True).returncode == 0:
        return True


def check_tool(tool_name, project_root):
    """
    Check if a tool is available, trying bootstrap if not found.

    Prioritizes project dependencies (.dependencies) over system tools
    for reproducible builds.

    Returns True if tool is available, False otherwise.
    Prints error message if tool cannot be found.
    """
    # Try bootstrap/.dependencies first (prepends to PATH, takes priority)
    if setup_tool_from_bootstrap(tool_name, project_root):
        if subprocess.run(['which', tool_name],
                          capture_output=True).returncode == 0:
            return True

    # Fall back to whatever is already on the system PATH
    if check_tool_in_system(tool_name):
        return True

    # Tool not found, print error
    print(f"Error: {tool_name} not found", file=sys.stderr)
    print(f"Please install {tool_name} or run: python3 utils/bootstrap.py",
          file=sys.stderr)
    return False


def run_command(cmd, verbose=False):
    """Run a shell command."""
    if verbose:
        print(f"$ {' '.join(str(c) for c in cmd)}")

    result = subprocess.run(cmd, capture_output=not verbose, text=True)

    if result.returncode != 0:
        if not verbose and result.stderr:
            print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)

    return result


def get_core_count():
    """Get number of CPU cores for parallel builds."""
    return os.cpu_count() or 4


def main():
    parser = argparse.ArgumentParser(
        description='Build unit tests for Buddy firmware',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           # Build all tests
  %(prog)s cobs_tests                # Build only cobs tests
  %(prog)s --list                    # List available test targets
  %(prog)s --debug                   # Build with debug symbols
  %(prog)s --release                 # Build with release flags (-DNDEBUG)
  %(prog)s --jobs 8                  # Use 8 parallel jobs
  %(prog)s --rebuild                 # Clean rebuild
  %(prog)s --build-dir my_tests      # Use custom build directory
  %(prog)s --run                     # Build and run all tests
  %(prog)s --run -- -R gcode         # Build and run tests matching 'gcode'
  %(prog)s --run -- -LE slow         # Build and run, excluding slow tests
  %(prog)s --test                    # Run tests only (skip build)
  %(prog)s -t -- -R gcode            # Run only gcode tests (skip build)
  %(prog)s -t -- -LE slow --verbose  # Run fast tests with verbose output
""")

    parser.add_argument(
        'targets',
        nargs='*',
        help=
        'Specific test targets to build (e.g., cobs_tests). If empty, builds all tests.'
    )
    parser.add_argument('--build-dir',
                        default='build_tests',
                        help='Build directory name (default: build_tests)')
    parser.add_argument('--board',
                        default='BUDDY',
                        help='Board to build for (default: BUDDY)')
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Build with debug symbols (-DCMAKE_BUILD_TYPE=Debug)')
    parser.add_argument(
        '--release',
        action='store_true',
        help='Build with optimizations (-DCMAKE_BUILD_TYPE=Release)')
    parser.add_argument('--rebuild',
                        action='store_true',
                        help='Clean and rebuild from scratch')
    parser.add_argument('--clean',
                        action='store_true',
                        help='Remove build directory and exit')
    parser.add_argument(
        '-j',
        '--jobs',
        type=int,
        default=get_core_count(),
        help=
        f'Number of parallel build jobs (default: auto-detect, currently {get_core_count()})'
    )
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Verbose output')
    parser.add_argument('--list',
                        action='store_true',
                        help='List available test targets and exit')
    parser.add_argument(
        '--run',
        '-r',
        action='store_true',
        help='Build and run tests. Use -- to pass args to ctest.')
    parser.add_argument(
        '--test',
        '-t',
        action='store_true',
        help='Run tests only (skip build). Use -- to pass args to ctest.')

    # Manually split argv on '--' to separate script args from ctest args
    if '--' in sys.argv:
        split_idx = sys.argv.index('--')
        script_args = sys.argv[1:split_idx]
        ctest_args = sys.argv[split_idx + 1:]
    else:
        script_args = sys.argv[1:]
        ctest_args = []

    args = parser.parse_args(script_args)

    # Validate mutually exclusive options
    if args.debug and args.release:
        print("Error: --debug and --release are mutually exclusive",
              file=sys.stderr)
        sys.exit(1)

    # Find directories
    project_root = find_project_root()
    build_dir = project_root / args.build_dir

    # Handle --test (run only, skip build)
    if args.test:
        if not build_dir.exists():
            print(f"Error: Build directory '{build_dir}' does not exist.",
                  file=sys.stderr)
            print("Run without --test first to build the tests.",
                  file=sys.stderr)
            sys.exit(1)

        cmake_cache = build_dir / 'CMakeCache.txt'
        if not cmake_cache.exists():
            print(f"Error: Build directory '{build_dir}' is not configured.",
                  file=sys.stderr)
            print("Run without --test first to configure and build.",
                  file=sys.stderr)
            sys.exit(1)

        # Ensure cmake/ctest is available
        if not check_tool('cmake', project_root):
            sys.exit(1)

        os.chdir(build_dir)
        ctest_cmd = ['ctest'] + ctest_args
        print(f"Running tests in {build_dir} with: \"{" ".join(ctest_cmd)}\"") #yapf: disable
        if args.verbose:
            print(f"$ {' '.join(ctest_cmd)}")
        result = subprocess.run(ctest_cmd)
        sys.exit(result.returncode)

    # Handle --clean and --rebuild
    if args.clean or args.rebuild:
        print("Cleaning build directory...")
        shutil.rmtree(build_dir, ignore_errors=True)
        if args.clean:
            return 0

    # Check for required tools (will attempt bootstrap if needed)
    if not check_tool('cmake', project_root):
        sys.exit(1)

    if not check_tool('ninja', project_root):
        sys.exit(1)

    # Create build directory if needed
    build_dir.mkdir(exist_ok=True)

    # Change to build directory
    os.chdir(build_dir)

    # Configure with CMake if needed
    cmake_cache = build_dir / 'CMakeCache.txt'
    if not cmake_cache.exists():
        build_type = 'Debug' if args.debug else (
            'Release' if args.release else 'default')
        print(
            f"Configuring tests (Board: {args.board}, Build type: {build_type})..."
        )

        cmake_args = [
            'cmake',
            str(project_root),
            '-G',
            'Ninja',
            f'-DBOARD={args.board}',
        ]

        if args.debug:
            cmake_args.append('-DCMAKE_BUILD_TYPE=Debug')
        elif args.release:
            cmake_args.append('-DCMAKE_BUILD_TYPE=Release')
        # else: default (no CMAKE_BUILD_TYPE) - assertions active (no -DNDEBUG), -Os optimization

        run_command(cmake_args, verbose=args.verbose)
        print()

    # Handle --list
    if args.list:
        print("Available test targets:")
        result = subprocess.run(['ninja', '-t', 'query', 'tests/unit/tests'],
                                capture_output=True,
                                text=True)
        for line in result.stdout.split('\n'):
            target = Path(line).name
            print(target)
        return 0

    # Determine number of jobs
    jobs = args.jobs

    # Build tests
    ninja_args = ['ninja', f'-j{jobs}']

    if args.targets:
        # Build specific targets
        ninja_args.extend(args.targets)
        print(
            f"Building specific tests: {', '.join(args.targets)} ({jobs} parallel jobs)..."
        )
    else:
        # Build all tests
        ninja_args.append('tests')
        print(f"Building all tests ({jobs} parallel jobs)...")

    # Run ninja with output visible to show progress
    if args.verbose:
        ninja_args.append('-v')
        print(f"$ {' '.join(ninja_args)}")

    result = subprocess.run(ninja_args)
    if result.returncode != 0:
        sys.exit(result.returncode)

    print()
    print("Build complete!")

    if args.run:
        # ctest is part of cmake, so it should be available after check_tool('cmake')
        ctest_cmd = ['ctest'] + ctest_args
        print(f"Running tests: {' '.join(ctest_cmd)}")
        result = subprocess.run(ctest_cmd)
        sys.exit(result.returncode)

    else:
        print()
        print("Run tests with:")
        print(f"  cd {build_dir} && ctest")
        print(f"  cd {build_dir} && ctest -R <pattern>")
        print(f"  cd {build_dir} && ctest --verbose")

    return 0


if __name__ == '__main__':
    sys.exit(main())
