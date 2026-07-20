#!/usr/bin/env python3
"""Release build wrapper.

Defaults to building the RME release machine firmware presets and stages BBFs
in ./bbf for easy copying.
"""

from __future__ import annotations

import argparse
import atexit
import json
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "bbf"
DEFAULT_SIGNING_KEY = PROJECT_ROOT / ".local" / "firmware-signing-key.pem"
DEFAULT_VERSION_WORKTREE_ROOT = PROJECT_ROOT.parent / f".{PROJECT_ROOT.name}-rme-version-builds"
# Version worktrees share .dependencies with the primary checkout, making this
# one lock cover direct, release-matrix, and worktree-local wrapper invocations.
BUILD_LOCK_PATH = PROJECT_ROOT / ".dependencies" / ".rme-build.lock"
BUILD_LOCK_ENV = "RME_BUILD_LOCK_OWNER"
_build_lock_file = None
MIN_REPO_PYTHON = (3, 8)
MAX_REPO_PYTHON_EXCLUSIVE = (3, 13)

MINI_LANGUAGE_PRESETS = [
    "mini-en-cs",
    "mini-en-de",
    "mini-en-es",
    "mini-en-fr",
    "mini-en-it",
    "mini-en-pl",
    "mini-en-ja",
    "mini-en-uk",
]

RELEASE_PRESETS = [
    "coreone",
    "coreone_indx",
    "coreonel",
    "mini",
    *MINI_LANGUAGE_PRESETS,
    "mk4",
    "mk3.5",
    "xl",
]

PRESET_ALIASES = {
    "all": RELEASE_PRESETS,
    "release": RELEASE_PRESETS,
    "machines": RELEASE_PRESETS,
    "physical": RELEASE_PRESETS,
    "mini-languages": MINI_LANGUAGE_PRESETS,
    "mini-lang": MINI_LANGUAGE_PRESETS,
    "mini-all": ["mini", *MINI_LANGUAGE_PRESETS],
}

PROGRESS_RE = re.compile(r"\[(\d+)/(\d+)\]")
MEMORY_REGION_RE = re.compile(
    r"^\s*([A-Z_]+):\s+(\d+(?:\.\d+)?)\s+([KMG]?B)\s+"
    r"(\d+(?:\.\d+)?)\s+([KMG]?B)\s+(\d+(?:\.\d+)?)%"
)
MAP_MEMORY_REGION_RE = re.compile(r"^([A-Z_]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+\S+")
SIZE_SECTION_RE = re.compile(r"^(\S+)\s+(\d+)\s+(\d+)$")
STATUS_LABELS = {
    "queued": "queued",
    "running": "running",
    "success": "done",
    "failed": "failed",
}
STATUS_COLORS = {
    "queued": "\033[90m",
    "running": "\033[36m",
    "success": "\033[32m",
    "failed": "\033[31m",
}
RESET = "\033[0m"
MEMORY_UNIT_BYTES = {
    "B": 1,
    "KB": 1024,
    "MB": 1024 * 1024,
    "GB": 1024 * 1024 * 1024,
}


@dataclass(frozen=True)
class MemoryRegion:
    used_bytes: int
    total_bytes: int
    percent: float


def acquire_build_lock() -> None:
    """Prevent concurrent wrappers from sharing build/output directories."""
    global _build_lock_file
    inherited_owner = os.environ.get(BUILD_LOCK_ENV)
    if inherited_owner:
        return

    BUILD_LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    lock_file = BUILD_LOCK_PATH.open("a+b")
    try:
        if os.name == "posix":
            import fcntl

            fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        else:
            import msvcrt

            lock_file.seek(0)
            if lock_file.read(1) == b"":
                lock_file.write(b"\0")
                lock_file.flush()
            lock_file.seek(0)
            msvcrt.locking(lock_file.fileno(), msvcrt.LK_NBLCK, 1)
    except (BlockingIOError, OSError):
        lock_file.close()
        raise SystemExit(f"Another build.py process is already running (lock: {BUILD_LOCK_PATH}).")

    _build_lock_file = lock_file
    os.environ[BUILD_LOCK_ENV] = str(os.getpid())

    def release() -> None:
        global _build_lock_file
        if _build_lock_file is not None:
            _build_lock_file.close()
            _build_lock_file = None

    atexit.register(release)


def terminate_build_process(process: subprocess.Popen[str], *, force: bool = False) -> None:
    if process.poll() is not None:
        return
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
        except ProcessLookupError:
            pass
    elif force:
        process.kill()
    else:
        process.terminate()


@dataclass
class BuildJob:
    preset: str
    command: list[str]
    products_dir: Path
    firmware_paths: list[Path]
    status: str = "queued"
    progress: str = "-"
    started_at: float | None = None
    finished_at: float | None = None
    returncode: int | None = None
    last_line: str = ""
    log: list[str] = field(default_factory=list)
    memory_regions: dict[str, MemoryRegion] = field(default_factory=dict)
    staged_bbfs: list[Path] = field(default_factory=list)

    @property
    def elapsed_s(self) -> int:
        end = self.finished_at or time.monotonic()
        if self.started_at is None:
            return 0
        return int(end - self.started_at)

    @property
    def progress_percent(self) -> int | None:
        if self.status == "success":
            return 100
        if "/" not in self.progress:
            return None
        done, total = self.progress.split("/", 1)
        try:
            done_i = int(done)
            total_i = int(total)
        except ValueError:
            return None
        if total_i <= 0:
            return None
        return min(100, int(done_i * 100 / total_i))

    def finish(self, returncode: int) -> None:
        self.returncode = returncode
        self.finished_at = time.monotonic()
        self.status = "success" if returncode == 0 else "failed"
        if self.status == "success" and "/" in self.progress:
            _, total = self.progress.split("/", 1)
            self.progress = f"{total}/{total}"

    def capture_output(self, line: str) -> None:
        self.log.append(line)
        self.last_line = line
        progress_match = PROGRESS_RE.search(line)
        if progress_match:
            self.progress = f"{progress_match.group(1)}/{progress_match.group(2)}"
        memory_match = MEMORY_REGION_RE.match(line)
        if memory_match:
            name, used, used_unit, total, total_unit, percent = memory_match.groups()
            self.memory_regions[name] = MemoryRegion(
                used_bytes=int(float(used) * MEMORY_UNIT_BYTES[used_unit]),
                total_bytes=int(float(total) * MEMORY_UNIT_BYTES[total_unit]),
                percent=float(percent),
            )


def load_preset_names() -> set[str]:
    with (PROJECT_ROOT / "utils" / "presets" / "presets.json").open() as f:
        data = json.load(f)
    return {preset["name"] for preset in data["presets"]}


def python_version_text(version: tuple[int, int]) -> str:
    return f"{version[0]}.{version[1]}"


def repo_python_range_text() -> str:
    max_minor = MAX_REPO_PYTHON_EXCLUSIVE[1] - 1
    return f"{MIN_REPO_PYTHON[0]}.{MIN_REPO_PYTHON[1]}-{MAX_REPO_PYTHON_EXCLUSIVE[0]}.{max_minor}"


def repo_python_is_supported(version: tuple[int, int]) -> bool:
    return MIN_REPO_PYTHON <= version < MAX_REPO_PYTHON_EXCLUSIVE


def python_command_version(command: list[str]) -> tuple[int, int] | None:
    try:
        result = subprocess.run(
            [
                *command,
                "-c",
                "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
            check=False,
        )
    except OSError:
        return None
    if result.returncode != 0:
        return None
    match = re.fullmatch(r"\s*(\d+)\.(\d+)\s*", result.stdout)
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


def repo_python_candidates() -> list[list[str]]:
    candidates: list[list[str]] = []
    env_python = os.environ.get("BUDDY_PYTHON")
    if env_python:
        candidates.append([env_python])

    for name in ("python3.12", "python3.11", "python3.10", "python3.9", "python3.8"):
        python = shutil.which(name)
        if python:
            candidates.append([python])

    if os.name == "nt" and shutil.which("py"):
        candidates.extend(
            [["py", "-3.12"], ["py", "-3.11"], ["py", "-3.10"], ["py", "-3.9"], ["py", "-3.8"]]
        )

    for python in pyenv_python_candidates():
        candidate = [str(python)]
        if candidate not in candidates:
            candidates.append(candidate)

    current = [sys.executable]
    if current not in candidates:
        candidates.append(current)
    return candidates


def pyenv_python_candidates() -> list[Path]:
    roots: list[Path] = []
    env_root = os.environ.get("PYENV_ROOT")
    if env_root:
        roots.append(Path(env_root))

    pyenv = shutil.which("pyenv")
    if pyenv:
        result = subprocess.run(
            [pyenv, "root"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
            check=False,
        )
        if result.returncode == 0 and result.stdout.strip():
            roots.append(Path(result.stdout.strip()))

    default_root = Path.home() / ".pyenv"
    if default_root not in roots:
        roots.append(default_root)

    pythons: list[Path] = []
    seen: set[Path] = set()
    for root in roots:
        versions = root / "versions"
        if not versions.exists():
            continue
        for python in versions.glob("*/bin/python"):
            if python not in seen:
                pythons.append(python)
                seen.add(python)
    return pythons


def find_repo_python() -> list[str]:
    current_version = sys.version_info[:2]
    if repo_python_is_supported(current_version):
        return [sys.executable]

    for candidate in repo_python_candidates():
        version = python_command_version(candidate)
        if version and repo_python_is_supported(version):
            return candidate

    current_text = python_version_text(current_version)
    raise SystemExit(
        f"Python {repo_python_range_text()} is required for this repo's pinned dependencies. "
        f"The current interpreter is Python {current_text}, which cannot build Pillow/numpy pins reliably. "
        "Install python3.12 or set BUDDY_PYTHON=/path/to/python3.12 and rerun ./build.py."
    )


def repo_python_env(repo_python: list[str]) -> dict[str, str]:
    env = os.environ.copy()
    if len(repo_python) == 1:
        env["BUDDY_PYTHON"] = repo_python[0]
    return env


def venv_root() -> Path:
    return PROJECT_ROOT / ".venv"


def venv_bin_dir() -> Path:
    if os.name == "nt":
        return venv_root() / "Scripts"
    return venv_root() / "bin"


def venv_python_path(root: Path | None = None) -> Path:
    venv = root or venv_root()
    if os.name == "nt":
        return venv / "Scripts" / "python.exe"
    return venv / "bin" / "python"


def configure_managed_python_env(env: dict[str, str]) -> None:
    bin_dir = venv_bin_dir()
    path = env.get("PATH", "")
    env["PATH"] = str(bin_dir) + (os.pathsep + path if path else "")
    env["BUDDY_PYTHON"] = str(venv_python_path())
    env.setdefault("Python3_ROOT_DIR", str(venv_root()))
    env.setdefault("Python3_EXECUTABLE", str(venv_python_path()))


def has_cmake_definition(definitions: list[str], name: str) -> bool:
    return any(definition.split(":", 1)[0].split("=", 1)[0] == name for definition in definitions)


def managed_python_cmake_defs(definitions: list[str], venv: Path) -> list[str]:
    cmake_defs = list(definitions)
    if not has_cmake_definition(cmake_defs, "Python3_ROOT_DIR"):
        cmake_defs.append(f"Python3_ROOT_DIR:PATH={venv}")
    if not has_cmake_definition(cmake_defs, "Python3_EXECUTABLE"):
        cmake_defs.append(f"Python3_EXECUTABLE:FILEPATH={venv_python_path(venv)}")
    return cmake_defs


def split_csv(values: list[str]) -> list[str]:
    tokens: list[str] = []
    for value in values:
        tokens.extend(token.strip() for token in value.split(",") if token.strip())
    return tokens


def unique_presets(tokens: list[str], known_presets: set[str]) -> list[str]:
    selected: list[str] = []
    seen: set[str] = set()

    for token in tokens:
        expanded = PRESET_ALIASES.get(token, [token])
        for preset in expanded:
            if preset not in known_presets:
                raise SystemExit(f"Unknown preset: {preset}")
            if preset not in seen:
                selected.append(preset)
                seen.add(preset)

    return selected


def default_signing_key() -> Path | None:
    env_key = os.environ.get("FIRMWARE_SIGNING_KEY")
    if env_key:
        return Path(env_key)
    if DEFAULT_SIGNING_KEY.exists():
        return DEFAULT_SIGNING_KEY
    return None


def current_git_branch() -> str | None:
    result = subprocess.run(
        ["git", "branch", "--show-current"],
        cwd=PROJECT_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        encoding="utf-8",
        check=False,
    )
    if result.returncode != 0:
        return None
    branch = result.stdout.strip()
    return branch or None


def git_ref_exists(ref: str) -> bool:
    result = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", ref],
        cwd=PROJECT_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def normalize_version(value: str) -> str:
    version = value.strip()
    if version.startswith("rme-v"):
        version = version.removeprefix("rme-v")
    elif version.startswith("v"):
        version = version.removeprefix("v")
    version = version.removesuffix("-RME")
    if not re.fullmatch(r"\d+(?:\.\d+)+", version):
        raise SystemExit(f"Invalid version: {value}")
    return version


def version_branch(version: str) -> str:
    return f"rme-v{version}"


def version_ref(version: str) -> str:
    branch = version_branch(version)
    if git_ref_exists(branch):
        return branch
    remote_branch = f"origin/{branch}"
    if git_ref_exists(remote_branch):
        return remote_branch
    raise SystemExit(f"Could not find branch {branch} or {remote_branch}")


def version_worktree(version: str) -> Path:
    return DEFAULT_VERSION_WORKTREE_ROOT / version_branch(version)


def prune_stale_worktrees() -> None:
    result = subprocess.run(
        ["git", "worktree", "prune"],
        cwd=PROJECT_ROOT,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def add_version_worktree(worktree: Path, ref: str) -> None:
    result = subprocess.run(
        ["git", "worktree", "add", "--detach", str(worktree), ref],
        cwd=PROJECT_ROOT,
        check=False,
    )
    if result.returncode == 0:
        return

    prune_stale_worktrees()
    result = subprocess.run(
        ["git", "worktree", "add", "--force", "--detach", str(worktree), ref],
        cwd=PROJECT_ROOT,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def link_shared_cache(worktree: Path, name: str) -> None:
    source = PROJECT_ROOT / name
    destination = worktree / name
    if not source.exists() or destination.is_symlink():
        return
    if destination.exists():
        if destination.is_dir() and not any(destination.iterdir()):
            destination.rmdir()
        else:
            return
    try:
        destination.symlink_to(source, target_is_directory=source.is_dir())
    except OSError:
        if source.is_dir():
            shutil.copytree(source, destination, symlinks=True)
        else:
            shutil.copy2(source, destination)


def link_shared_version_caches(worktree: Path) -> None:
    if worktree == PROJECT_ROOT:
        return
    for name in (".dependencies", ".venv", ".local"):
        link_shared_cache(worktree, name)


def ensure_version_worktree(version: str, *, dry_run: bool = False, use_current: bool = True) -> Path:
    branch = version_branch(version)
    if use_current and current_git_branch() == branch:
        return PROJECT_ROOT

    ref = version_ref(version)
    worktree = version_worktree(version)
    if dry_run:
        return worktree

    DEFAULT_VERSION_WORKTREE_ROOT.mkdir(parents=True, exist_ok=True)
    if not worktree.exists():
        prune_stale_worktrees()
        add_version_worktree(worktree, ref)
    else:
        result = subprocess.run(
            ["git", "-C", str(worktree), "switch", "--detach", ref],
            cwd=PROJECT_ROOT,
            check=False,
        )
        if result.returncode != 0:
            raise SystemExit(result.returncode)
    link_shared_version_caches(worktree)
    return worktree


def version_build_dir(args: argparse.Namespace, version: str, worktree: Path) -> Path:
    build_root = args.build_dir.resolve() if args.build_dir else worktree / "build"
    return build_root / version


def make_version_build_command(args: argparse.Namespace, version: str, worktree: Path, output_dir: Path) -> list[str]:
    command = [
        *find_repo_python(),
        str(worktree / "build.py"),
        "--output-dir",
        str(output_dir / version),
        "--jobs",
        str(args.jobs),
        "--build-dir",
        str(version_build_dir(args, version, worktree)),
    ]
    for preset in args.preset or []:
        command.extend(["--preset", preset])
    command.extend(["--build-type", args.build_type])
    command.extend(["--bootloader", args.bootloader])
    signing_key = None if args.no_signing_key else (args.signing_key or default_signing_key())
    if signing_key:
        command.extend(["--signing-key", str(signing_key.resolve())])
    if args.no_signing_key:
        command.append("--no-signing-key")
    if args.skip_bootstrap:
        command.append("--skip-bootstrap")
    if args.store_output:
        command.append("--store-output")
    if args.no_store_output:
        command.append("--no-store-output")
    if args.generate_dfu:
        command.append("--generate-dfu")
    if args.final:
        command.append("--final")
    if args.version_suffix is not None:
        command.extend(["--version-suffix", args.version_suffix])
    if args.version_suffix_short is not None:
        command.extend(["--version-suffix-short", args.version_suffix_short])
    if args.no_clean_output:
        command.append("--no-clean-output")
    for definition in managed_python_cmake_defs(args.cmake_def, worktree / ".venv"):
        command.extend(["-D", definition])
    return command


def build_versions(args: argparse.Namespace) -> int:
    started_at = time.monotonic()
    versions = [normalize_version(version) for version in args.versions]
    output_dir = args.output_dir.resolve()
    if args.jobs is not None and args.jobs < 1:
        raise SystemExit("--jobs must be at least 1")
    if args.store_output and args.no_store_output:
        raise SystemExit("--store-output and --no-store-output are mutually exclusive")

    print("Building versions:", ",".join(versions))
    print("BBF output:", output_dir)
    print("Version worktree cache:", DEFAULT_VERSION_WORKTREE_ROOT)
    print("Parallel jobs per version:", args.jobs)
    print("Total elapsed:", format_elapsed(int(time.monotonic() - started_at)))

    commands: list[tuple[str, Path, list[str]]] = []
    for version in versions:
        worktree = ensure_version_worktree(version, dry_run=args.dry_run, use_current=False)
        version_output = output_dir / version
        if args.dry_run:
            command = make_version_build_command(args, version, worktree, output_dir)
            commands.append((version, worktree, command))
            continue
        if not args.no_clean_output and version_output.exists():
            shutil.rmtree(version_output)
        version_output.mkdir(parents=True, exist_ok=True)
        command = make_version_build_command(args, version, worktree, output_dir)
        commands.append((version, worktree, command))

    if args.dry_run:
        for version, worktree, command in commands:
            print(f"# {version} ({worktree})")
            print(" ".join(str(part) for part in command))
        return 0

    failed: list[str] = []
    for version, worktree, command in commands:
        print()
        print(f"Building RME {version} in {worktree}")
        result = subprocess.run(command, cwd=worktree, check=False)
        if result.returncode != 0:
            failed.append(version)

    print()
    print("Multi-version build summary")
    print("-" * 72)
    print(f"Result: {len(commands) - len(failed)} succeeded, {len(failed)} failed")
    print(f"Total elapsed: {format_elapsed(int(time.monotonic() - started_at))}")
    print()
    print("Staged version folders")
    print("-" * 72)
    for version, _, _ in commands:
        print(f"{version:<12} {(output_dir / version).resolve()}")
    if failed:
        print()
        print("Failed versions:", ", ".join(failed))
        return 1
    return 0


def normalize_bbf_names(output_dir: Path) -> None:
    for bbf in output_dir.glob("*.bbf"):
        destination = bbf.with_name(normalized_bbf_name(bbf.name))
        if destination == bbf:
            continue
        if destination.exists():
            destination.unlink()
        bbf.rename(destination)


def normalized_bbf_name(name: str) -> str:
    for marker in ("_release_boot", "_release_noboot", "_release_emptyboot"):
        name = name.replace(marker, "")
    return name


def keep_only_bbf_files(output_dir: Path) -> None:
    for product in output_dir.iterdir():
        if product.is_file() and product.suffix != ".bbf":
            product.unlink()


def is_selected_bbf(path: Path, selected_presets: list[str]) -> bool:
    stem = path.stem
    return any(stem == preset or stem.startswith(f"{preset}_") for preset in selected_presets)


def prune_unselected_bbfs(output_dir: Path, selected_presets: list[str]) -> None:
    for bbf in output_dir.glob("*.bbf"):
        if not is_selected_bbf(bbf, selected_presets):
            bbf.unlink()


def copy_finished_bbfs(job: BuildJob, output_dir: Path, selected_presets: list[str]) -> None:
    for bbf in job.products_dir.glob("*.bbf"):
        destination = output_dir / normalized_bbf_name(bbf.name)
        if destination.exists():
            destination.unlink()
        shutil.copy2(bbf, destination)
        job.staged_bbfs.append(destination.resolve())
    prune_unselected_bbfs(output_dir, selected_presets)
    keep_only_bbf_files(output_dir)


def collect_finished_bbfs(jobs: list[BuildJob], output_dir: Path, selected_presets: list[str]) -> None:
    for job in jobs:
        if job.returncode == 0:
            copy_finished_bbfs(job, output_dir, selected_presets)


def format_elapsed(seconds: int) -> str:
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    if hours:
        return f"{hours:d}:{minutes:02d}:{seconds:02d}"
    return f"{minutes:d}:{seconds:02d}"


def format_bytes(value: int) -> str:
    if value >= 1024 * 1024:
        return f"{value / (1024 * 1024):.2f} MiB"
    if value >= 1024:
        return f"{value / 1024:.1f} KiB"
    return f"{value} B"


def format_region(region: MemoryRegion | None) -> str:
    if region is None:
        return "-"
    return f"{format_bytes(region.used_bytes)} / {format_bytes(region.total_bytes)} ({region.percent:.2f}%)"


def aggregate_ram(job: BuildJob) -> MemoryRegion | None:
    ram_regions = [region for name, region in job.memory_regions.items() if name != "FLASH"]
    if not ram_regions:
        return None
    used_bytes = sum(region.used_bytes for region in ram_regions)
    total_bytes = sum(region.total_bytes for region in ram_regions)
    percent = used_bytes * 100 / total_bytes if total_bytes else 0
    return MemoryRegion(used_bytes=used_bytes, total_bytes=total_bytes, percent=percent)


def find_arm_size() -> Path | None:
    candidates = sorted((PROJECT_ROOT / ".dependencies").glob("gcc-arm-none-eabi-*/bin/arm-none-eabi-size"))
    return candidates[-1] if candidates else None


def read_map_regions(map_path: Path) -> dict[str, tuple[int, int]]:
    regions: dict[str, tuple[int, int]] = {}
    in_memory_configuration = False
    for line in map_path.read_text(errors="replace").splitlines():
        if line == "Memory Configuration":
            in_memory_configuration = True
            continue
        if not in_memory_configuration:
            continue
        match = MAP_MEMORY_REGION_RE.match(line)
        if match:
            name, origin, length = match.groups()
            regions[name] = (int(origin, 16), int(length, 16))
        elif regions and line.strip():
            break
    return regions


def hydrate_memory_from_artifact(job: BuildJob) -> None:
    if job.memory_regions:
        return
    arm_size = find_arm_size()
    if arm_size is None:
        return
    for firmware in job.firmware_paths:
        map_path = firmware.with_suffix(".map")
        if not firmware.exists() or not map_path.exists():
            continue
        capacities = read_map_regions(map_path)
        if not capacities:
            continue
        result = subprocess.run(
            [str(arm_size), "-A", str(firmware)],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            encoding="utf-8",
            check=False,
        )
        if result.returncode != 0:
            continue
        used = {name: 0 for name in capacities}
        for line in result.stdout.splitlines():
            match = SIZE_SECTION_RE.match(line)
            if not match:
                continue
            section_name, size, address = match.groups()
            section_size = int(size)
            section_address = int(address)
            for name, (origin, length) in capacities.items():
                if origin <= section_address < origin + length:
                    used[name] += section_size
                    break
            if section_name == ".data" and "FLASH" in used:
                used["FLASH"] += section_size
        for name, (_, total_bytes) in capacities.items():
            used_bytes = used[name]
            percent = used_bytes * 100 / total_bytes if total_bytes else 0
            job.memory_regions[name] = MemoryRegion(used_bytes, total_bytes, percent)
        return


def progress_bar(percent: int | None, width: int = 18) -> str:
    if percent is None:
        return "[" + "." * width + "]"
    filled = min(width, max(0, int(width * percent / 100)))
    return "[" + "#" * filled + "." * (width - filled) + "]"


def strip_ansi(value: str) -> str:
    return re.sub(r"\033\[[0-9;]*m", "", value)


class TerminalUI:
    def __init__(self, enabled: bool):
        self.enabled = enabled
        self.started = False
        self.line_count = 0

    def start(self) -> None:
        if not self.enabled or self.started:
            return
        print("\033[?25l", end="", flush=True)
        self.started = True

    def stop(self) -> None:
        if not self.enabled or not self.started:
            return
        print("\033[?25h", end="", flush=True)
        self.started = False

    def render(self, jobs: list[BuildJob], *, total_elapsed_s: int | None = None) -> None:
        if not self.enabled:
            return
        self.start()
        lines = build_progress_lines(jobs, total_elapsed_s=total_elapsed_s, color=True)
        if self.line_count:
            print(f"\033[{self.line_count}F", end="")
        for index, line in enumerate(lines):
            print("\033[2K" + line)
            if index == len(lines) - 1 and self.line_count > len(lines):
                for _ in range(self.line_count - len(lines)):
                    print("\033[2K")
        self.line_count = len(lines)


def build_progress_lines(jobs: list[BuildJob], *, total_elapsed_s: int | None = None, color: bool = False) -> list[str]:
    completed = sum(1 for job in jobs if job.status in {"success", "failed"})
    failed = sum(1 for job in jobs if job.status == "failed")
    elapsed = f"  elapsed {format_elapsed(total_elapsed_s)}" if total_elapsed_s is not None else ""

    lines = [
        f"Firmware build progress  {completed}/{len(jobs)} done  {failed} failed{elapsed}",
        "MODEL              STATUS     PROGRESS              TIME     CURRENT STEP",
        "-" * 92,
    ]
    for job in jobs:
        percent = job.progress_percent
        if percent is None:
            progress = job.progress
        elif "/" in job.progress:
            progress = f"{job.progress} {percent:3d}%"
        else:
            progress = f"{percent:3d}%"
        status_text = STATUS_LABELS.get(job.status, job.status)
        if color:
            status = f"{STATUS_COLORS.get(job.status, '')}{status_text:<10}{RESET}"
        else:
            status = f"{status_text:<10}"
        current = strip_ansi(job.last_line)[-34:] if job.last_line else ""
        lines.append(
            f"{job.preset:<18} {status} {progress_bar(percent)} "
            f"{progress:<12} {format_elapsed(job.elapsed_s):<8} {current}"
        )
    return lines


def print_summary(jobs: list[BuildJob], output_dir: Path, *, total_elapsed_s: int) -> None:
    success = [job for job in jobs if job.returncode == 0]
    failed = [job for job in jobs if job.returncode != 0]
    for job in jobs:
        hydrate_memory_from_artifact(job)

    print()
    print("Build summary")
    print("-" * 72)
    print(f"Result: {len(success)} succeeded, {len(failed)} failed")
    print(f"Total elapsed: {format_elapsed(total_elapsed_s)}")
    print()
    print("MACHINE / PRESET   RESULT     TIME     FLASH                         RAM")
    print("-" * 104)
    for job in jobs:
        result = "SUCCESS" if job.returncode == 0 else "FAILED"
        print(
            f"{job.preset:<18} {result:<10} {format_elapsed(job.elapsed_s):<8} "
            f"{format_region(job.memory_regions.get('FLASH')):<29} {format_region(aggregate_ram(job))}"
        )

    print()
    print("Memory regions")
    print("-" * 104)
    for job in jobs:
        regions = ", ".join(f"{name}: {format_region(region)}" for name, region in job.memory_regions.items())
        print(f"{job.preset:<18} {regions or '-'}")

    staged = sorted(output_dir.glob("*.bbf"))
    if staged:
        print()
        print("Staged BBF artifacts")
        print("-" * 104)
        for bbf in staged:
            print(f"{bbf.name:<32} {bbf.resolve()}")


def read_process_output(job: BuildJob, process: subprocess.Popen[str], output_queue: queue.Queue[tuple[BuildJob, str]]) -> None:
    assert process.stdout is not None
    for line in process.stdout:
        line = line.rstrip()
        job.capture_output(line)
        output_queue.put((job, line))


def generate_signing_key_with_ecdsa(private_key: Path, public_key: Path) -> bool:
    try:
        from ecdsa import NIST256p, SigningKey
    except ImportError:
        return False

    key = SigningKey.generate(curve=NIST256p)
    private_key.write_bytes(key.to_pem())
    public_key.write_bytes(key.verifying_key.to_pem())
    return True


def generate_signing_key_with_openssl(private_key: Path, public_key: Path) -> bool:
    openssl = shutil.which("openssl")
    if not openssl:
        return False

    subprocess.run(
        [openssl, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(private_key)],
        check=True,
    )
    subprocess.run(
        [openssl, "ec", "-in", str(private_key), "-pubout", "-out", str(public_key)],
        check=True,
    )
    return True


def venv_python() -> Path:
    if os.name == "nt":
        return venv_bin_dir() / "python.exe"
    return venv_bin_dir() / "python"


def generate_signing_key_with_bootstrap(private_key: Path, public_key: Path) -> bool:
    python = find_repo_python()
    bootstrap = subprocess.run([*python, str(PROJECT_ROOT / "utils" / "bootstrap.py")], cwd=PROJECT_ROOT, check=False)
    if bootstrap.returncode != 0:
        return False

    venv = venv_python()
    if not venv.exists():
        return False

    script = (
        "from ecdsa import NIST256p, SigningKey\n"
        "from pathlib import Path\n"
        "private_key = Path(r'''%s''')\n"
        "public_key = Path(r'''%s''')\n"
        "key = SigningKey.generate(curve=NIST256p)\n"
        "private_key.write_bytes(key.to_pem())\n"
        "public_key.write_bytes(key.verifying_key.to_pem())\n"
    ) % (str(private_key), str(public_key))
    result = subprocess.run([str(venv), "-c", script], cwd=PROJECT_ROOT, check=False)
    return result.returncode == 0


def setup_signing_key(private_key: Path, *, force: bool = False) -> int:
    public_key = private_key.with_name(f"{private_key.stem}-public.pem")
    if private_key.exists() and not force:
        print(f"Signing key already exists: {private_key}")
        print("Use --force-signing-key to replace it.")
        return 0

    private_key.parent.mkdir(parents=True, exist_ok=True)
    if private_key.exists():
        private_key.unlink()
    if public_key.exists():
        public_key.unlink()

    generated = generate_signing_key_with_ecdsa(private_key, public_key)
    if not generated:
        generated = generate_signing_key_with_openssl(private_key, public_key)
    if not generated:
        generated = generate_signing_key_with_bootstrap(private_key, public_key)
    if not generated:
        print("Could not generate a signing key.")
        print("Install Python package 'ecdsa' or OpenSSL, then rerun --setup-signing.")
        return 1

    if os.name == "posix":
        private_key.chmod(0o600)
        public_key.chmod(0o644)

    print(f"Created signing key: {private_key}")
    print(f"Created public key:  {public_key}")
    print("Set FIRMWARE_SIGNING_KEY to the private key path or leave it at the default location.")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build release firmware and stage BBF files in a top-level folder."
    )
    parser.add_argument(
        "--preset",
        action="append",
        default=None,
        help=(
            "Comma-separated presets or aliases to build. Aliases: all, release, "
            "mini-languages, mini-all. Default: all."
        ),
    )
    parser.add_argument("--list-presets", action="store_true", help="List known release presets and exit.")
    parser.add_argument("--build-type", default="release", help="Build type passed to utils/build.py. Default: release.")
    parser.add_argument("--bootloader", default="yes", help="Bootloader mode passed to utils/build.py. Default: yes.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Where BBFs are staged. Default: ./bbf.")
    parser.add_argument("--build-dir", type=Path, help="Build directory passed to utils/build.py.")
    parser.add_argument(
        "--versions",
        nargs="+",
        default=None,
        help=(
            "Build multiple RME release branches into per-version output folders, "
            "for example: --versions 6.5.7 6.6.1."
        ),
    )
    parser.add_argument("--signing-key", type=Path, help="PEM signing key. Defaults to FIRMWARE_SIGNING_KEY or .local/firmware-signing-key.pem if present.")
    parser.add_argument("--setup-signing", action="store_true", help="Create a local ECDSA signing key and exit.")
    parser.add_argument("--force-signing-key", action="store_true", help="Replace the signing key when used with --setup-signing.")
    parser.add_argument("--no-signing-key", action="store_true", help="Do not pass a signing key, even if the default key exists.")
    parser.add_argument("--skip-bootstrap", action="store_true", help="Pass --skip-bootstrap to utils/build.py.")
    parser.add_argument("--store-output", action="store_true", help="Ask utils/build.py to store build logs in files.")
    parser.add_argument("--no-store-output", action="store_true", help="Print build output to the console.")
    parser.add_argument("--generate-dfu", action="store_true", help="Pass --generate-dfu to utils/build.py.")
    parser.add_argument("--final", action="store_true", help="Pass --final to utils/build.py.")
    parser.add_argument("--version-suffix", help="Pass --version-suffix to utils/build.py.")
    parser.add_argument("--version-suffix-short", help="Pass --version-suffix-short to utils/build.py.")
    parser.add_argument("--no-clean-output", action="store_true", help="Do not clear the output folder before building.")
    parser.add_argument("--jobs", type=int, default=4, help="Number of presets to build at once. Default: 4.")
    parser.add_argument("--verbose", action="store_true", help="Print each child build's output while it runs.")
    parser.add_argument("--dry-run", action="store_true", help="Print the build command without running it.")
    parser.add_argument(
        "-D",
        "--cmake-def",
        action="append",
        default=[],
        help="Custom CMake cache entry, passed through to utils/build.py.",
    )
    return parser.parse_args()


def main() -> int:
    wrapper_started_at = time.monotonic()
    args = parse_args()
    known_presets = load_preset_names()

    if args.setup_signing:
        key_path = args.signing_key or DEFAULT_SIGNING_KEY
        return setup_signing_key(key_path, force=args.force_signing_key)

    if args.list_presets:
        for preset in RELEASE_PRESETS:
            print(preset)
        return 0

    acquire_build_lock()

    if args.versions:
        return build_versions(args)

    selected_presets = unique_presets(split_csv(args.preset or ["all"]), known_presets)
    output_dir = args.output_dir.resolve()
    repo_python = find_repo_python()
    child_env = repo_python_env(repo_python)
    configure_managed_python_env(child_env)
    cmake_defs = managed_python_cmake_defs(args.cmake_def, venv_root())

    if args.store_output and args.no_store_output:
        raise SystemExit("--store-output and --no-store-output are mutually exclusive")
    if args.jobs is not None and args.jobs < 1:
        raise SystemExit("--jobs must be at least 1")

    if not args.dry_run and not args.no_clean_output and output_dir.exists():
        shutil.rmtree(output_dir)
    if not args.dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)

    signing_key = None if args.no_signing_key else (args.signing_key or default_signing_key())
    if signing_key:
        if not signing_key.exists():
            raise SystemExit(f"Signing key not found: {signing_key}")

    product_root = output_dir / ".products"
    jobs: list[BuildJob] = []
    for preset in selected_presets:
        products_dir = product_root / preset
        command = [
            *repo_python,
            str(PROJECT_ROOT / "utils" / "build.py"),
            "--preset",
            preset,
            "--build-type",
            args.build_type,
            "--bootloader",
            args.bootloader,
            "--products-dir",
            str(products_dir),
            "--skip-bootstrap",
            "--no-store-output",
        ]

        if args.build_dir:
            command.extend(["--build-dir", str(args.build_dir)])
        if args.store_output:
            command.remove("--no-store-output")
            command.append("--store-output")
        if args.generate_dfu:
            command.append("--generate-dfu")
        if args.final:
            command.append("--final")
        if args.version_suffix is not None:
            command.extend(["--version-suffix", args.version_suffix])
        if args.version_suffix_short is not None:
            command.extend(["--version-suffix-short", args.version_suffix_short])
        if signing_key:
            command.extend(["--signing-key", str(signing_key)])
        for definition in cmake_defs:
            command.extend(["-D", definition])
        bootloader_components = {
            "yes": "boot",
            "no": "noboot",
            "empty": "emptyboot",
        }
        firmware_paths = [
            (args.build_dir or PROJECT_ROOT / "build")
            / f"{preset}_{args.build_type}_{bootloader_components[bootloader.strip().lower()]}"
            / "firmware"
            for bootloader in args.bootloader.split(",")
            if bootloader.strip().lower() in bootloader_components
        ]
        jobs.append(BuildJob(preset=preset, command=command, products_dir=products_dir, firmware_paths=firmware_paths))

    max_parallel = args.jobs
    print("Building presets:", ",".join(selected_presets))
    print("BBF output:", output_dir)
    print("Parallel jobs:", max_parallel)
    print("Total elapsed:", format_elapsed(int(time.monotonic() - wrapper_started_at)))
    if args.dry_run:
        for job in jobs:
            print(" ".join(str(part) for part in job.command))
        return 0

    if not args.skip_bootstrap:
        print("Bootstrapping once before builds...")
        bootstrap = subprocess.run(
            [*repo_python, str(PROJECT_ROOT / "utils" / "bootstrap.py")],
            cwd=PROJECT_ROOT,
            env=child_env,
            check=False,
        )
        if bootstrap.returncode != 0:
            return bootstrap.returncode

    nnvg = venv_bin_dir() / ("nnvg.exe" if os.name == "nt" else "nnvg")
    if not nnvg.exists():
        print(f"Missing managed Nunavut tool: {nnvg}", file=sys.stderr)
        print("Run utils/bootstrap.py or rerun ./build.py without --skip-bootstrap.", file=sys.stderr)
        return 1

    if product_root.exists():
        shutil.rmtree(product_root)
    product_root.mkdir(parents=True, exist_ok=True)

    running: dict[subprocess.Popen[str], BuildJob] = {}
    pending = list(jobs)
    output_queue: queue.Queue[tuple[BuildJob, str]] = queue.Queue()
    last_render = 0.0
    ui = TerminalUI(enabled=sys.stdout.isatty() and not args.verbose)

    try:
        while pending or running:
            while pending and len(running) < max_parallel:
                job = pending.pop(0)
                job.products_dir.mkdir(parents=True, exist_ok=True)
                job.started_at = time.monotonic()
                job.status = "running"
                process = subprocess.Popen(
                    job.command,
                    cwd=PROJECT_ROOT,
                    env=child_env,
                    start_new_session=os.name == "posix",
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    bufsize=1,
                )
                running[process] = job
                threading.Thread(target=read_process_output, args=(job, process, output_queue), daemon=True).start()

            while True:
                try:
                    job, line = output_queue.get_nowait()
                except queue.Empty:
                    break
                if args.verbose:
                    print(f"[{job.preset}] {line}")

            for process, job in list(running.items()):
                returncode = process.poll()
                if returncode is None:
                    continue
                job.finish(returncode)
                running.pop(process)

            now = time.monotonic()
            if now - last_render >= 0.2:
                ui.render(jobs, total_elapsed_s=int(now - wrapper_started_at))
                last_render = now
            if running:
                time.sleep(0.1)
        ui.render(jobs, total_elapsed_s=int(time.monotonic() - wrapper_started_at))
    except KeyboardInterrupt:
        print("\nStopping active builds...")
        for process in running:
            terminate_build_process(process)
        for process in running:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                terminate_build_process(process, force=True)
        return 130
    finally:
        ui.stop()

    collect_finished_bbfs(jobs, output_dir, selected_presets)
    keep_only_bbf_files(output_dir)
    prune_unselected_bbfs(output_dir, selected_presets)
    if product_root.exists():
        shutil.rmtree(product_root)

    failed = [job for job in jobs if job.returncode != 0]
    print_summary(jobs, output_dir, total_elapsed_s=int(time.monotonic() - wrapper_started_at))
    if failed:
        print()
        print("Failed builds:")
        for job in failed:
            print(f" {job.preset}")
            for line in job.log[-20:]:
                print(f"   {line}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
