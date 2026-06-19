#!/usr/bin/env python3
# =============================================================================
#  PhantomRF — post-build hook
# =============================================================================
#  Runs *after* PlatformIO finishes compiling a firmware. Produces a tidy
#  release artifact set under `dist/`:
#
#     dist/
#     ├── PhantomRF-<env>-v<version>.bin               ← main app image
#     ├── PhantomRF-<env>-v<version>.bootloader.bin    ← second-stage bootloader
#     ├── PhantomRF-<env>-v<version>.partitions.bin    ← partition table image
#     ├── PhantomRF-<env>-v<version>.uf2               ← drag-and-drop (S3 only)
#     ├── PhantomRF-<env>-v<version>.sha256            ← sha256 of the main bin
#     └── manifest.json                                ← combined manifest
#
#  Two execution modes:
#
#  1. PlatformIO extra_script mode
#     `extra_scripts = post:scripts/post_build.py` in platformio.ini
#     -> `Import("env")` works, we register an SCons post-action on
#        `$BUILD_DIR/firmware.bin` that calls `process_environment()`.
#
#  2. Standalone CLI mode
#     `python3 scripts/post_build.py --env esp32s3-n16r8`
#     -> Useful for re-processing an already-built env (CI artifact
#        regeneration, manifest-only rebuild, dry-run checks, etc.).
#
#  No external dependencies — stdlib only. Python 3.10+ syntax.
# =============================================================================

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path


# -- ANSI styling ------------------------------------------------------------
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
MAGENTA = "\033[35m"
CYAN = "\033[36m"


def _emit(text: str, *, err: bool = False) -> None:
    stream = sys.stderr if err else sys.stdout
    try:
        print(text, file=stream, flush=True)
    except BrokenPipeError:
        try:
            stream.close()
        except Exception:
            pass


def echo(message: str, color: str | None = None, bold: bool = False, err: bool = False) -> None:
    if color:
        message = f"{color}{message}{RESET}"
    if bold:
        message = f"{BOLD}{message}{RESET}"
    _emit(message, err=err)


def section(title: str) -> None:
    bar = "═" * 60
    echo("")
    echo(f"{CYAN}{BOLD}{bar}{RESET}")
    echo(f"{CYAN}{BOLD}  {title}{RESET}")
    echo(f"{CYAN}{BOLD}{bar}{RESET}")


# -- UF2 constants ----------------------------------------------------------
# Reference: https://github.com/microsoft/uf2 (UF2 specification)
# Only the ESP32-S2 / S3 / C3 have well-supported UF2 bootloaders in the wild
# (the `esp32s2/esp32s3/esp32c3` families in the Microsoft UF2 reference).
# The classic ESP32 has no native USB OTG and no mainstream UF2 bootloader,
# so we deliberately skip UF2 generation for it. See DESIGN.md §9.3.
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_NOT_MAIN_FLASH = 0x00002000
UF2_BLOCK_SIZE = 512
UF2_PAYLOAD_SIZE = 256  # typical for ESP32 family

# Family IDs (from uf2-family.h in the Microsoft UF2 reference).
UF2_FAMILY_ESP32S3 = 0x1C5F21B0
UF2_FAMILY_ESP32S2 = 0x78C0CF8B
UF2_FAMILY_ESP32C3 = 0xC47E5767

# App-partition base address on each supported chip. All three place the
# factory app at 0x10000, so the value is the same, but we keep the mapping
# explicit in case we ever need to add a chip with a different layout.
UF2_BASE_ADDR_DEFAULT = 0x00010000


# Env-name patterns that support UF2. The matching is intentionally strict
# (no `esp32` substring) so that `esp32-classic` and `native` are not falsely
# classified as UF2-capable.
UF2_PATTERNS: tuple[tuple[tuple[str, ...], int], ...] = (
    (("esp32s3", "esp32-s3"), UF2_FAMILY_ESP32S3),
    (("esp32s2", "esp32-s2"), UF2_FAMILY_ESP32S2),
    (("esp32c3", "esp32-c3"), UF2_FAMILY_ESP32C3),
)


# -- Helpers -----------------------------------------------------------------
def sha256_file(path: Path) -> str:
    """Return the lowercase hex SHA-256 of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def human_size(n: int) -> str:
    """Pretty-print a byte count."""
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TB"


def detect_uf2_family(env_name: str) -> int | None:
    """Map a PlatformIO env name to a UF2 family ID, or None if unsupported.

    The match is anchored on the chip family (S2/S3/C3). `esp32-classic` and
    `native` deliberately return None so no UF2 is generated.
    """
    name = env_name.lower()
    for needles, family in UF2_PATTERNS:
        for n in needles:
            if n in name:
                return family
    return None


def detect_uf2_base_addr(env_name: str) -> int:
    """Base address used for the UF2 destination addr field.

    All UF2-capable ESP32 variants place the factory app at 0x10000, so the
    return value is currently identical for every supported env. Kept as a
    function for forward compatibility.
    """
    if detect_uf2_family(env_name) is None:
        return UF2_BASE_ADDR_DEFAULT
    return UF2_BASE_ADDR_DEFAULT


def make_uf2_block(
    *,
    block_num: int,
    total_blocks: int,
    addr: int,
    payload: bytes,
    family_id: int,
) -> bytes:
    """Build a single 512-byte UF2 block."""
    assert len(payload) <= UF2_PAYLOAD_SIZE, "payload too large for UF2 block"
    block = bytearray(UF2_BLOCK_SIZE)
    struct.pack_into("<I", block, 0, UF2_MAGIC_START0)
    struct.pack_into("<I", block, 4, UF2_MAGIC_START1)
    struct.pack_into("<I", block, 8, UF2_FLAG_NOT_MAIN_FLASH)
    struct.pack_into("<I", block, 12, addr)
    struct.pack_into("<I", block, 16, len(payload))
    struct.pack_into("<I", block, 20, block_num)
    struct.pack_into("<I", block, 24, total_blocks)
    struct.pack_into("<I", block, 28, family_id)
    # Copy payload (already padded to UF2_PAYLOAD_SIZE by caller if shorter)
    block[32 : 32 + len(payload)] = payload
    struct.pack_into("<I", block, 508, UF2_MAGIC_END)
    return bytes(block)


def bin_to_uf2(bin_data: bytes, family_id: int, base_addr: int) -> bytes:
    """Wrap a flat binary blob in UF2 blocks."""
    if not bin_data:
        return b""

    # Pad to a multiple of UF2_PAYLOAD_SIZE
    pad_len = (-len(bin_data)) % UF2_PAYLOAD_SIZE
    padded = bin_data + b"\x00" * pad_len

    total = len(padded) // UF2_PAYLOAD_SIZE
    out = bytearray()
    for i in range(total):
        start = i * UF2_PAYLOAD_SIZE
        payload = padded[start : start + UF2_PAYLOAD_SIZE]
        out += make_uf2_block(
            block_num=i,
            total_blocks=total,
            addr=base_addr + start,
            payload=payload,
            family_id=family_id,
        )
    return bytes(out)


# -- Core processing ---------------------------------------------------------
def process_environment(
    env_name: str,
    project_dir: Path,
    version: str,
    *,
    dry_run: bool = False,
    verbose: bool = True,
) -> bool:
    """Locate .pio/build/<env>/ artifacts, copy/rename them into dist/,
    compute SHA-256 hashes, optionally generate a UF2, and write a
    manifest.json describing the lot.

    Returns True on success, False if required inputs are missing.
    """
    section(f"PhantomRF post-build :: {env_name}")

    project_dir = project_dir.resolve()
    dist_dir = project_dir / "dist"
    build_dir = project_dir / ".pio" / "build" / env_name

    echo(f"  {BOLD}environment{RESET}  : {env_name}")
    echo(f"  {BOLD}version{RESET}      : {version}")
    echo(f"  {BOLD}project dir{RESET}  : {project_dir}")
    echo(f"  {BOLD}build dir{RESET}    : {build_dir.relative_to(project_dir)}")
    echo(f"  {BOLD}dist dir{RESET}     : {dist_dir.relative_to(project_dir)}")

    if not build_dir.exists():
        echo(
            f"  {RED}ERROR:{RESET} build directory not found: {build_dir}",
            err=True,
        )
        echo(
            f"  {YELLOW}hint:{RESET} run `pio run -e {env_name}` first",
            err=True,
        )
        return False

    if not dry_run:
        dist_dir.mkdir(parents=True, exist_ok=True)

    # Files we want to harvest. `firmware` is the only required one — the
    # rest are optional (some envs don't produce a bootloader.bin etc.).
    candidates: dict[str, Path] = {
        "firmware": build_dir / "firmware.bin",
        "bootloader": build_dir / "bootloader.bin",
        "partitions": build_dir / "partitions.bin",
    }

    artifacts: list[dict] = []
    echo("")
    for kind, src in candidates.items():
        if not src.exists():
            echo(f"  {YELLOW}skip   {RESET} {kind}.bin (not present)")
            continue

        size = src.stat().st_size
        digest = sha256_file(src)

        # Main firmware keeps a clean name; bootloader/partitions get a
        # suffix to avoid colliding with the main .bin.
        suffix = "" if kind == "firmware" else f".{kind}"
        dest_name = f"PhantomRF-{env_name}-v{version}{suffix}.bin"
        dest_path = dist_dir / dest_name

        echo(f"  {GREEN}found  {RESET} {kind}.bin ({human_size(size)})")
        echo(f"         {DIM}src  ={RESET}  {src.relative_to(project_dir)}")
        echo(f"         {DIM}dst  ={RESET}  dist/{dest_name}")
        echo(f"         {DIM}sha  ={RESET}  {digest}")

        if not dry_run:
            shutil.copy2(src, dest_path)

        artifacts.append(
            {
                "type": kind,
                "name": dest_name,
                "size": size,
                "sha256": digest,
                "source": str(src.relative_to(project_dir)),
            }
        )

    if not artifacts:
        echo(
            f"  {RED}ERROR:{RESET} no firmware artifacts found in {build_dir}",
            err=True,
        )
        return False

    # UF2 generation — only for chips that the UF2 bootloader ecosystem
    # actually supports (currently S2, S3, C3).
    family = detect_uf2_family(env_name)
    firmware_artifact = next((a for a in artifacts if a["type"] == "firmware"), None)
    if family is not None and firmware_artifact is not None:
        uf2_name = f"PhantomRF-{env_name}-v{version}.uf2"
        uf2_path = dist_dir / uf2_name
        base_addr = detect_uf2_base_addr(env_name)

        echo("")
        echo(f"  {MAGENTA}uf2    {RESET} {uf2_name}")
        echo(f"         {DIM}family  ={RESET}  0x{family:08X}")
        echo(f"         {DIM}base    ={RESET}  0x{base_addr:08X}")

        if not dry_run:
            with open(dist_dir / firmware_artifact["name"], "rb") as f:
                bin_data = f.read()
            uf2_data = bin_to_uf2(bin_data, family_id=family, base_addr=base_addr)
            with open(uf2_path, "wb") as f:
                f.write(uf2_data)
            uf2_size = len(uf2_data)
            uf2_sha = sha256_bytes(uf2_data)
        else:
            uf2_size = 0
            uf2_sha = ""

        if not dry_run:
            artifacts.append(
                {
                    "type": "uf2",
                    "name": uf2_name,
                    "size": uf2_size,
                    "sha256": uf2_sha,
                    "family": f"0x{family:08X}",
                    "base_addr": f"0x{base_addr:08X}",
                }
            )
            echo(f"         {DIM}size    ={RESET}  {human_size(uf2_size)}")
            echo(f"         {DIM}sha     ={RESET}  {uf2_sha}")
    else:
        echo("")
        echo(
            f"  {YELLOW}skip   {RESET} UF2 generation (no UF2 family for '{env_name}')"
        )

    # SHA-256 sidecar (sha256sum-compatible) for the main firmware
    if not dry_run and firmware_artifact is not None:
        sha_sidecar = dist_dir / f"PhantomRF-{env_name}-v{version}.sha256"
        with open(sha_sidecar, "w", encoding="utf-8") as f:
            f.write(
                f"{firmware_artifact['sha256']}  {firmware_artifact['name']}\n"
            )
        echo("")
        echo(f"  {GREEN}wrote  {RESET} {sha_sidecar.name}")

    # Combined manifest
    if not dry_run:
        manifest = {
            "project": "PhantomRF",
            "version": version,
            "environment": env_name,
            "built_at": datetime.now(timezone.utc).isoformat(),
            "artifacts": artifacts,
        }
        manifest_path = dist_dir / "manifest.json"
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, sort_keys=False)
            f.write("\n")
        echo(f"  {GREEN}wrote  {RESET} manifest.json ({len(artifacts)} artifacts)")

    echo("")
    echo(f"  {GREEN}{BOLD}done.{RESET} {len(artifacts)} artifact(s) ready in dist/")
    echo("")
    return True


# -- CLI ---------------------------------------------------------------------
def _parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="post_build.py",
        description=(
            "PhantomRF post-build hook — packages PlatformIO build outputs "
            "into dist/ with SHA-256 hashes, optional UF2 generation, and a "
            "manifest.json."
        ),
    )
    p.add_argument(
        "--env",
        help=(
            "PlatformIO environment name. "
            "Defaults to $PIOENV (set by PlatformIO) or $PHM_ENV."
        ),
    )
    p.add_argument(
        "--project-dir",
        default=os.environ.get("PHM_PROJECT_DIR") or os.environ.get("PROJECT_DIR") or ".",
        help="Project root directory (default: current directory or $PROJECT_DIR)",
    )
    p.add_argument(
        "--version",
        default=os.environ.get("PHM_VERSION", "0.1.0"),
        help="Firmware version string (default: 0.1.0 or $PHM_VERSION)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would happen, but don't write any files",
    )
    p.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress non-error output",
    )
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv if argv is not None else sys.argv[1:])

    env_name = args.env or os.environ.get("PIOENV") or os.environ.get("PHM_ENV")
    if not env_name:
        echo(
            f"{RED}ERROR:{RESET} no environment specified "
            "(use --env or set $PIOENV / $PHM_ENV)",
            err=True,
        )
        return 2

    project_dir = Path(args.project_dir)
    if not project_dir.is_dir():
        echo(
            f"{RED}ERROR:{RESET} project directory not found: {project_dir}",
            err=True,
        )
        return 2

    ok = process_environment(
        env_name=env_name,
        project_dir=project_dir,
        version=args.version,
        dry_run=args.dry_run,
        verbose=not args.quiet,
    )
    return 0 if ok else 1


# -- SCons integration -------------------------------------------------------
# PlatformIO sources this file as a Python module. If the SCons `Import`
# builtin is available, register a post-action on firmware.bin. Otherwise
# the file is being executed directly (e.g. `python3 scripts/post_build.py`).
try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821  (SCons builtin)

    _phm_scons_env = env  # type: ignore[name-defined]

    def _phm_post_build_callback(*_args, **_kwargs) -> None:
        _env = os.environ.get("PIOENV", "unknown")
        _project = os.environ.get("PROJECT_DIR", ".")
        _version = os.environ.get("PHM_VERSION", "0.1.0")
        process_environment(
            env_name=_env,
            project_dir=Path(_project),
            version=_version,
        )

    _phm_scons_env.AddPostAction(
        "$BUILD_DIR/firmware.bin", _phm_post_build_callback
    )

except NameError:
    # Standalone mode — run the CLI.
    if __name__ == "__main__":
        sys.exit(main())
