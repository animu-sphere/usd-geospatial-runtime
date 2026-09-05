"""Run a command with a composed runtime's search paths layered onto the host.

`ost runtime exec` replaces the environment with the composition's own search
paths, which is what the acceptance probes need: they run through the host
interpreter, whose executable directory supplies its own libraries, and total
isolation is what makes their result a claim about the composition alone.

A native consumer cannot use that. The composed OpenUSD links the host CPython
(`usd_python` against `python313.dll`), and this composition deliberately does
not bundle an interpreter, so a freshly linked executable started with an
isolated PATH cannot load. This runner therefore *prepends* the composition's
paths to the environment it was given instead of replacing it.

That difference is the point: use `ost runtime exec` for anything whose result
is a claim about the runtime, and this only to build and test native code
against it.

    python tools/sdk_env.py --composition <prefix> -- ctest --test-dir <build> -C Release
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

PLATFORMS = {"win32": "windows", "linux": "linux", "darwin": "macos"}


def platform_name(name=None):
    key = name or sys.platform
    for prefix, value in PLATFORMS.items():
        if key.startswith(prefix):
            return value
    raise SystemExit(f"sdk env: unsupported platform {key!r}")


def environment(prefix: Path, base=None) -> dict:
    """Return `base` with the composition's search paths prepended."""
    lock_path = prefix / "metadata/composition.lock.json"
    if not lock_path.is_file():
        raise SystemExit(f"sdk env: {prefix} is not a composed runtime prefix")
    sdk = json.loads(lock_path.read_text(encoding="utf-8"))["sdk"]

    result = dict(os.environ if base is None else base)
    for key, value in sdk.get("settings", {}).items():
        result[key] = value
    for entry in sdk["environment"][platform_name()]:
        if entry["operation"] != "prepend":
            raise SystemExit(f"sdk env: unsupported operation {entry['operation']!r}")
        added = os.pathsep.join(str(prefix / path) for path in entry["paths"])
        existing = result.get(entry["key"], "")
        result[entry["key"]] = f"{added}{os.pathsep}{existing}" if existing else added
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--composition", type=Path, required=True)
    parser.add_argument("--print", action="store_true",
                        help="print the layered variables instead of running a command")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)

    prefix = args.composition.resolve()
    env = environment(prefix)
    if args.print:
        # Print every variable this runner would set, which is the search paths
        # plus the composition's own settings. Printing only the paths would
        # describe an environment the runner does not actually build.
        sdk = json.loads((prefix / "metadata/composition.lock.json").read_text(encoding="utf-8"))["sdk"]
        keys = {entry["key"] for entry in sdk["environment"][platform_name()]}
        keys.update(sdk.get("settings", {}))
        for key in sorted(keys):
            print(f"{key}={env[key]}")
        return 0

    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("no command given; pass it after '--'")
    return subprocess.run(command, env=env).returncode


if __name__ == "__main__":
    raise SystemExit(main())
