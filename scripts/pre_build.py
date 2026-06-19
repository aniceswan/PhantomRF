#!/usr/bin/env python3
# =============================================================================
#  PhantomRF — pre-build hook
# =============================================================================
#  Runs *before* PlatformIO starts the actual SCons build. Prints a small
#  banner so the developer can see in the CI logs which env / commit / time
#  this build corresponds to.
#
#  Two execution modes:
#
#  1. PlatformIO extra_script mode
#     `extra_scripts = pre:scripts/pre_build.py` in platformio.ini
#     -> The `Import("env")` SCons builtin is defined, we register a
#        pre-action on the firmware target that calls `banner()`.
#
#  2. Standalone CLI mode
#     `python3 scripts/pre_build.py` (useful for local debugging or CI smoke
#     tests that want to verify the script itself runs cleanly).
#     -> Falls through to `if __name__ == "__main__":` and prints the
#        banner once.
#
#  No external dependencies — only the Python 3.10+ standard library.
# =============================================================================

from __future__ import annotations

import os
import sys
from datetime import datetime, timezone


# -- ANSI styling -----------------------------------------------------------
# `click.echo`-style: colors via raw ANSI escapes (no `colorama` dep needed
# on POSIX; Windows 10+ terminals understand them natively).
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
CYAN = "\033[36m"
YELLOW = "\033[33m"
GREEN = "\033[32m"


def _emit(text: str) -> None:
    """Print to stdout, flush, and never crash on broken pipes (e.g. CI)."""
    try:
        print(text, flush=True)
    except BrokenPipeError:
        # downstream consumer closed the pipe — nothing we can do, exit cleanly
        try:
            sys.stdout.close()
        except Exception:
            pass


def banner() -> None:
    """Print a friendly pre-build banner with context."""
    env = os.environ.get("PIOENV", "<unknown>")
    project = os.environ.get("PROJECT_DIR", ".")
    version = os.environ.get("PHM_VERSION", "0.1.0")
    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

    width = 60
    bar = "─" * width

    _emit("")
    _emit(f"{CYAN}{BOLD}{bar}{RESET}")
    _emit(f"{CYAN}{BOLD}  PhantomRF  ::  pre-build hook{RESET}")
    _emit(f"{CYAN}{BOLD}{bar}{RESET}")
    _emit(f"  {BOLD}environment{RESET}  : {env}")
    _emit(f"  {BOLD}version{RESET}      : {version}")
    _emit(f"  {BOLD}project{RESET}      : {project}")
    _emit(f"  {BOLD}started{RESET}      : {timestamp}")
    _emit(f"  {BOLD}status{RESET}       : {YELLOW}building…{RESET}")
    _emit("")


# -- SCons integration ------------------------------------------------------
# When PlatformIO loads this file as an extra_script, the SCons `Import`
# builtin is available and lets us pull the active Environment into scope.
# We then attach a pre-action to the firmware build target.
try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821  (SCons builtin)

    def _phm_pre_build_callback(*_args, **_kwargs) -> None:
        banner()

    # Run before firmware.bin (or the explicit firmware elf) is compiled.
    _phm_scons_env = env  # type: ignore[name-defined]
    _phm_scons_env.AddPreAction("$BUILD_DIR/firmware.bin", _phm_pre_build_callback)

except NameError:
    # Standalone CLI mode — `Import` doesn't exist outside SCons.
    if __name__ == "__main__":
        banner()
