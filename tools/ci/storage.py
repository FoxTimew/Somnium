#!/usr/bin/env python3
"""Bound disposable CI builds without deleting retained evidence or worktrees.
Callers hold the runner root lock; each candidate also requires its run lock.
"""
from contextlib import contextmanager
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import time

GIB = 1024 ** 3
MIN_FREE_BYTES = 12 * GIB
BUILD_QUOTA_BYTES = 24 * GIB
BUILD_MAX_AGE_DAYS = 2
BUILD_NAMES = ("build", "hot-reload-spike")
EVIDENCE_SUFFIXES = {".log", ".json", ".yaml", ".yml", ".xml", ".png", ".rgba", ".ppm", ".tracy"}


class WorktreePresent(RuntimeError):
    pass


@contextmanager
def exclusive(path):
    """An OS lock is released even when concurrency cancellation kills the process."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        stream.write(b"0")
        stream.flush()
        stream.seek(0)
        if os.name == "nt":
            import msvcrt
            try:
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise BlockingIOError(f"Another local/CI operation holds {path}") from error
        else:
            import fcntl
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def linked(path):
    return path.is_symlink() or path.is_junction()


def files(directory):
    # Neither sizing nor evidence copying follows nested links/junctions.
    for parent, directories, names in os.walk(directory, followlinks=False):
        if ".git" in directories or ".git" in names:
            raise WorktreePresent(f"Protected Git worktree inside build: {parent}")
        directories[:] = [name for name in directories if not linked(Path(parent) / name)]
        for name in names:
            path = Path(parent) / name
            if not linked(path) and path.is_file():
                yield path


@dataclass
class Build:
    path: Path
    run: Path
    modified: float
    size: int


def candidates(root, current):
    root, current = root.resolve(), current.resolve()
    runs = root / "runs"
    if linked(runs) or not runs.is_dir():
        return []
    result = []
    for run in runs.iterdir():
        if linked(run) or not run.is_dir() or run.resolve() == current:
            continue
        marker = run / "run.json"
        if linked(marker) or not marker.is_file():
            continue
        try:
            data = json.loads(marker.read_text(encoding="utf-8"))
        except (ValueError, OSError):
            continue
        if data.get("owner") != "somnium-ci":
            continue
        for name in BUILD_NAMES:
            path = run / name
            if linked(path) or not path.is_dir():
                continue
            try:
                size = sum(p.stat().st_size for p in files(path))
            except WorktreePresent:
                continue
            result.append(Build(path.resolve(), run.resolve(), marker.stat().st_mtime, size))
    return sorted(result, key=lambda item: (item.modified, str(item.path)))


def archive_evidence(build):
    # These reports otherwise live inside disposable CMake trees. Keep their paths
    # under the originating run so the normal 14-day evidence retention applies.
    destination = build.run / "logs" / "retired-builds" / f"{build.path.name}-{time.time_ns()}"
    if any(linked(path) for path in [build.run / "logs", destination.parent, destination]):
        raise RuntimeError(f"Unsafe evidence archive path: {destination}")
    for source in files(build.path):
        relative = source.relative_to(build.path)
        if (source.suffix.lower() in EVIDENCE_SUFFIXES or "Testing" in relative.parts
                or source.name == "CMakeCache.txt"):
            target = destination / relative
            if not target.resolve().is_relative_to(destination.resolve()) or linked(target):
                raise RuntimeError(f"Unsafe evidence archive target: {target}")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)


def clean_builds(root, current, *, quota=BUILD_QUOTA_BYTES, days=BUILD_MAX_AGE_DAYS,
                 minimum_free=0, summary=print):
    root = root.resolve()
    builds = candidates(root, current)
    total = sum(item.size for item in builds)
    cutoff = time.time() - days * 86400
    reclaimed = 0
    for build in builds:
        pressure = minimum_free and shutil.disk_usage(root).free < minimum_free
        if build.modified >= cutoff and total <= quota and not pressure:
            continue
        try:
            with exclusive(build.run / "active.lock"):
                # Check the exact absolute target immediately before recursive deletion.
                expected = root / "runs" / build.run.name / build.path.name
                if (build.path != expected or build.path.name not in BUILD_NAMES
                        or build.run == current.resolve() or linked(root / "runs")
                        or linked(build.run) or linked(build.path)
                        or build.path.resolve() != expected):
                    raise RuntimeError(f"Unsafe disposable build path: {build.path}")
                archive_evidence(build)
                shutil.rmtree(build.path)
                total -= build.size
                reclaimed += build.size
                summary(f"Purged disposable build: `{build.path}` ({build.size / GIB:.2f} GiB); reports retained under `{build.run / 'logs/retired-builds'}`")
        except BlockingIOError:
            summary(f"Preserved active build: `{build.path}`")
    return reclaimed


def require_space(directory, cleanup_command, *, minimum=MIN_FREE_BYTES):
    directory = directory.resolve()
    volume = directory
    while not volume.exists():
        volume = volume.parent
    free = shutil.disk_usage(volume).free
    if free < minimum:
        raise RuntimeError(
            f"Insufficient free space before build in {directory}: {free / GIB:.2f} GiB; "
            f"at least {minimum / GIB:.0f} GiB required. Cleanup: {cleanup_command}. "
            "Only CI-owned inactive build directories may be purged automatically; "
            "worktrees, retained evidence and paths outside the CI root are preserved.")
    return free
