"""Generate the machine-readable runtime metadata for each composed target.

The metadata document is derived only from committed inputs: the composition
manifest, its lock, and the accepted release evidence for the same target. It is
regenerated rather than edited, and CI fails when the committed document differs
from the generated one.
"""

import argparse
import json
from pathlib import Path
import re
import sys
import tomllib

SCHEMA = "usd-geospatial-runtime.runtime-metadata/v1"
TOOL = "tools/runtime_metadata.py"
COMPOSED_OCI_REPOSITORY = "ghcr.io/animu-sphere/usd-geospatial-runtime"
MANIFEST_GLOB = "runtime-composition.*.toml"
TARGET_PATTERN = re.compile(
    r"^(?P<os>[a-z0-9]+)-(?P<arch>[a-z0-9_]+)-(?P<toolchain>[a-z0-9]+)-(?P<host_abi>py(?P<py>[0-9]{2,3}))$"
)
RELEASE_PATTERN = re.compile(r"^v(?P<major>[0-9]+)\.(?P<minor>[0-9]+)\.(?P<patch>[0-9]+)$")
INTERPRETER_PATTERN = re.compile(r"(^|/)python[0-9.]*(\.exe)?$", re.IGNORECASE)
OS_NAMES = {"windows": "windows", "linux": "linux", "macos": "macos"}


class MetadataError(RuntimeError):
    """A committed input does not describe a usable target."""


def slug_of(manifest: Path) -> str:
    """Return the target slug embedded in a manifest filename."""
    name = manifest.name
    if not name.startswith("runtime-composition.") or not name.endswith(".toml"):
        raise MetadataError(f"{name} is not a composition manifest filename")
    return name[len("runtime-composition."):-len(".toml")]


def target_identity(target: str, usd_version: str, bundles_python: bool) -> dict:
    match = TARGET_PATTERN.match(target)
    if not match:
        raise MetadataError(
            f"target {target!r} is not canonical; expected <os>-<arch>-<toolchain>-py<version>"
        )
    if match["os"] not in OS_NAMES:
        raise MetadataError(f"target {target!r} names an unknown operating system")
    digits = match["py"]
    return {
        "id": target,
        "os": OS_NAMES[match["os"]],
        "arch": match["arch"],
        "toolchain": match["toolchain"],
        "host_abi": match["host_abi"],
        "usd_version": usd_version,
        "host_python": {"version": f"{digits[0]}.{digits[1:]}", "bundled": bundles_python},
    }


def release_key(release: str) -> tuple:
    match = RELEASE_PATTERN.match(release)
    if not match:
        raise MetadataError(f"release {release!r} is not a vMAJOR.MINOR.PATCH identifier")
    return int(match["major"]), int(match["minor"]), int(match["patch"])


def select_evidence(root: Path, target: str) -> tuple:
    """Return the newest committed evidence record for a target."""
    candidates = []
    for path in sorted((root / "evidence").glob("*.json")):
        record = json.loads(path.read_text(encoding="utf-8"))
        if record.get("target") == target:
            candidates.append((release_key(record["release"]), path, record))
    if not candidates:
        raise MetadataError(f"no committed evidence record for target {target!r}")
    _, path, record = max(candidates, key=lambda item: item[0])
    return path, record


def build(root: Path, manifest_path: Path) -> dict:
    """Build the metadata document for one composition manifest."""
    slug = slug_of(manifest_path)
    lock_path = root / f"runtime.{slug}.lock.json"
    manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    resolved = lock["resolved"]
    target = manifest["composition"]["target"]
    if resolved["target"] != target:
        raise MetadataError(f"lock target {resolved['target']!r} does not match manifest {target!r}")

    oci_by_artifact = {item["artifact"]: item["source"].split("@", 1)[1] for item in manifest["artifacts"]}
    components = []
    for component in sorted(resolved["components"], key=lambda item: item["id"]):
        digest = component["digest"]
        if digest not in oci_by_artifact:
            raise MetadataError(f"component {component['id']!r} is not pinned to an OCI source")
        components.append({
            "id": component["id"],
            "kind": component["kind"],
            "version": component["version"],
            "artifact": digest,
            "oci_manifest": oci_by_artifact[digest],
        })

    requirements = {item["capability"]: item.get("version") for item in manifest["requirements"]}
    capabilities = []
    for provider in sorted(resolved["providers"], key=lambda item: item["capability"]):
        capability = provider["capability"]
        entry = {
            "capability": capability,
            "required": capability in requirements,
            "component": provider["component"],
            "version": provider["version"],
            "artifact": provider["digest"],
        }
        if requirements.get(capability):
            entry["requirement"] = requirements[capability]
        capabilities.append(entry)
    missing = sorted(set(requirements) - {item["capability"] for item in capabilities})
    if missing:
        raise MetadataError(f"required capabilities have no resolved provider: {', '.join(missing)}")

    usd = next((item["version"] for item in capabilities if item["capability"] == "usd"), None)
    if usd is None:
        raise MetadataError("the composition does not resolve the 'usd' capability")
    bundles_python = any(
        INTERPRETER_PATTERN.search(item["destination"]) for item in resolved["install"]
    )

    evidence_path, evidence = select_evidence(root, target)
    release = evidence["release"]
    release_record = root / "docs/releases" / f"{release}.md"
    if not release_record.is_file():
        raise MetadataError(f"evidence {evidence_path.name} has no release record at {release_record}")

    return {
        "schema": SCHEMA,
        "generated_by": {
            "tool": TOOL,
            "inputs": {
                "manifest": manifest_path.relative_to(root).as_posix(),
                "lock": lock_path.relative_to(root).as_posix(),
                "evidence": evidence_path.relative_to(root).as_posix(),
            },
        },
        "runtime": {
            "name": manifest["composition"]["name"],
            "version": release.lstrip("v"),
            "release": release,
        },
        "target": target_identity(target, usd, bundles_python),
        "identity": {
            "manifest_digest": resolved["manifest_digest"],
            "composition_digest": resolved["composition_digest"],
            "runtime_digest": lock["runtime_digest"],
            "composed_artifact": evidence["composed_artifact"],
            "composed_oci_manifest": evidence["composed_oci_manifest"],
            "oci_locator": f"oci://{COMPOSED_OCI_REPOSITORY}@{evidence['composed_oci_manifest']}",
        },
        "components": components,
        "capabilities": capabilities,
        "evidence": {
            "release": release,
            "status": evidence["status"],
            "evidence_file": evidence_path.relative_to(root).as_posix(),
            "release_record": release_record.relative_to(root).as_posix(),
        },
    }


def manifests(root: Path) -> list:
    found = sorted(root.glob(MANIFEST_GLOB))
    if not found:
        raise MetadataError(f"no composition manifest matched {MANIFEST_GLOB}")
    return found


def documents(root: Path) -> dict:
    """Return the generated metadata document for every manifest, keyed by path."""
    return {root / f"runtime-metadata.{slug_of(path)}.json": build(root, path) for path in manifests(root)}


def serialize(document: dict) -> str:
    return json.dumps(document, indent=2) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--write", action="store_true", help="rewrite the committed metadata documents")
    args = parser.parse_args(argv)
    root = args.root.resolve()

    try:
        generated = documents(root)
    except (MetadataError, KeyError, OSError, ValueError) as error:
        print(f"runtime metadata: {error}", file=sys.stderr)
        return 1

    stale = []
    for path, document in generated.items():
        text = serialize(document)
        if args.write:
            path.write_text(text, encoding="utf-8")
            print(f"wrote {path.relative_to(root).as_posix()}")
        elif not path.is_file() or path.read_text(encoding="utf-8") != text:
            stale.append(path.relative_to(root).as_posix())
    if stale:
        print(
            "runtime metadata is stale; run 'python tools/runtime_metadata.py --write': "
            + ", ".join(stale),
            file=sys.stderr,
        )
        return 1
    if not args.write:
        print(f"runtime metadata: {len(generated)} document(s) up to date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
