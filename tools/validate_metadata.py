"""Validate the committed composition, metadata, and release-evidence documents.

This is the single entry point used by CI and by the release workflow. It needs
nothing beyond CPython 3.13:

    python tools/validate_metadata.py
    python tools/validate_metadata.py --release v0.1.0
"""

import argparse
import json
from pathlib import Path
import re
import sys
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parent))

import accept
import jsonschema_lite
import runtime_metadata

DIGEST = re.compile(r"^sha256:[0-9a-f]{64}$")
SCHEMAS = Path("schemas")


class Report:
    """Collects check outcomes so one run reports every problem it found."""

    def __init__(self):
        self.failures = []

    def check(self, label, condition, detail=""):
        if condition:
            print(f"ok   {label}")
        else:
            print(f"FAIL {label}{': ' + detail if detail else ''}")
            self.failures.append(label)
        return bool(condition)

    def failed(self, label, detail):
        return self.check(label, False, detail)


def load_schema(root, name):
    return json.loads((root / SCHEMAS / name).read_text(encoding="utf-8"))


def validate_composition(root, report):
    """Check that each manifest is fully pinned and matches its lock."""
    for manifest_path in runtime_metadata.manifests(root):
        slug = runtime_metadata.slug_of(manifest_path)
        lock_path = root / f"runtime.{slug}.lock.json"
        if not report.check(f"{slug}: lock present", lock_path.is_file(), str(lock_path)):
            continue
        manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        embedded = lock["manifest"]

        report.check(f"{slug}: lock embeds the manifest composition",
                     embedded["composition"] == manifest["composition"])
        report.check(f"{slug}: lock embeds the manifest providers",
                     embedded["providers"] == manifest["providers"])
        report.check(f"{slug}: lock embeds the manifest artifacts",
                     {item["artifact"]: item["source"] for item in embedded["artifacts"]}
                     == {item["artifact"]: item["source"] for item in manifest["artifacts"]})
        report.check(f"{slug}: lock embeds the manifest requirements",
                     {tuple(sorted(item.items())) for item in embedded["requirements"]}
                     == {tuple(sorted(item.items())) for item in manifest["requirements"]})

        for item in manifest["artifacts"]:
            locator = item["source"]
            repository = locator.split("@", 1)[0]
            report.check(f"{slug}: artifact {item['artifact'][:19]} is content addressed",
                         bool(DIGEST.match(item["artifact"])))
            report.check(f"{slug}: source {repository} is pinned by OCI digest",
                         locator.startswith("oci://") and "@sha256:" in locator
                         and ":" not in repository.rsplit("/", 1)[-1],
                         locator)

        pinned = {item["artifact"] for item in manifest["artifacts"]}
        resolved = {component["digest"] for component in lock["resolved"]["components"]}
        report.check(f"{slug}: every resolved component is pinned", resolved <= pinned,
                     ", ".join(sorted(resolved - pinned)))
        report.check(f"{slug}: no candidate artifact is unused", pinned <= resolved,
                     ", ".join(sorted(pinned - resolved)))

        provided = {provider["capability"] for provider in lock["resolved"]["providers"]}
        required = {item["capability"] for item in manifest["requirements"]}
        report.check(f"{slug}: every required capability has a provider", required <= provided,
                     ", ".join(sorted(required - provided)))
        report.check(f"{slug}: the lock records no conflicts", not lock["resolved"]["conflicts"])


def check_status(entry):
    """Return the status of an evidence check in either recorded form."""
    return entry if isinstance(entry, str) else entry.get("status")


def required_checks_of(record, baseline):
    """Return the checks a record's own release required.

    A record written before the field existed required exactly the v1 baseline,
    so a historical record stays valid when a later release adds a capability.
    """
    return set(record.get("required_checks", baseline))


def validate_evidence(root, report):
    """Check every committed release-evidence record against its schema."""
    schema = load_schema(root, "release-evidence.v1.json")
    baseline = schema["properties"]["checks"]["required"]
    locks = {}
    for manifest_path in runtime_metadata.manifests(root):
        slug = runtime_metadata.slug_of(manifest_path)
        lock_path = root / f"runtime.{slug}.lock.json"
        try:
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            target = lock["resolved"]["target"]
        except (OSError, ValueError, KeyError) as error:
            report.failed(f"{slug}: lock is readable and names a target", f"{type(error).__name__}: {error}")
            continue
        locks[target] = lock

    records = sorted((root / "evidence").glob("*.json"))
    report.check("evidence: at least one record is committed", bool(records))
    for path in records:
        name = path.relative_to(root).as_posix()
        record = json.loads(path.read_text(encoding="utf-8"))
        errors = jsonschema_lite.errors_for(record, schema)
        if not report.check(f"{name}: matches release-evidence.v1", not errors, "; ".join(errors)):
            continue
        report.check(f"{name}: records a passing acceptance", record["status"] == "passed")
        failed = sorted(check for check, entry in record["checks"].items()
                        if check_status(entry) != "passed")
        report.check(f"{name}: every recorded check passed", not failed, ", ".join(failed))
        report.check(f"{name}: has a release record",
                     (root / "docs/releases" / f"{record['release']}.md").is_file())
        target = record["target"]
        report.check(f"{name}: target {target} has a committed composition", target in locks)
        required = required_checks_of(record, baseline)
        report.check(f"{name}: records every check its release required",
                     required <= set(record["checks"]),
                     ", ".join(sorted(required - set(record["checks"]))))
        report.check(f"{name}: never drops a check the contract already required",
                     set(baseline) <= required,
                     ", ".join(sorted(set(baseline) - required)))
        report.check(f"{name}: composition was reproduced from public inputs",
                     record["clean_public_input_compose"])
        report.check(f"{name}: composed artifact was reconstructed",
                     record["clean_composed_artifact_reconstruction"])


def validate_runtime_metadata(root, report):
    """Check that the committed metadata is schema valid and not stale."""
    schema = load_schema(root, "runtime-metadata.v1.json")
    try:
        generated, pending = runtime_metadata.generate(root)
    except (runtime_metadata.MetadataError, KeyError, OSError, ValueError) as error:
        report.failed("runtime metadata: generation", f"{type(error).__name__}: {error}")
        return
    for path, error in pending.items():
        name = path.relative_to(root).as_posix()
        print(f"note {name}: composition awaits release acceptance; {error}")
        validate_committed_metadata(root, path, schema, name, report)
    for path, document in generated.items():
        name = path.relative_to(root).as_posix()
        errors = jsonschema_lite.errors_for(document, schema)
        report.check(f"{name}: matches runtime-metadata.v1", not errors, "; ".join(errors))
        text = runtime_metadata.serialize(document)
        report.check(f"{name}: is committed and current",
                     path.is_file() and path.read_text(encoding="utf-8") == text,
                     "run 'python tools/runtime_metadata.py --write'")
        validate_published_identities(root, document, name, report)


def validate_committed_metadata(root, path, schema, name, report):
    """Check the document a pending target keeps, which no generation reproduces.

    While the composition sits ahead of its evidence the committed document still
    describes the last accepted release, so it is checked against that record
    rather than against the lock the composition now resolves.
    """
    if not report.check(f"{name}: is committed", path.is_file()):
        return
    document = json.loads(path.read_text(encoding="utf-8"))
    errors = jsonschema_lite.errors_for(document, schema)
    if not report.check(f"{name}: matches runtime-metadata.v1", not errors, "; ".join(errors)):
        return
    record_path = root / document["evidence"]["evidence_file"]
    if not report.check(f"{name}: names a committed evidence record", record_path.is_file(),
                        str(record_path)):
        return
    record = json.loads(record_path.read_text(encoding="utf-8"))
    report.check(f"{name}: describes the release it names",
                 document["runtime"]["release"] == record["release"])
    for field in ("runtime_digest", "composed_artifact", "composed_oci_manifest"):
        report.check(f"{name}: {field} agrees with the evidence it names",
                     document["identity"][field] == record[field])
    validate_published_identities(root, document, name, report)


def validate_published_identities(root, document, name, report):
    """Check that prose repeats the generated identities without drift."""
    identity = document["identity"]
    readme = (root / "README.md").read_text(encoding="utf-8")
    record_path = root / document["evidence"]["release_record"]
    record = record_path.read_text(encoding="utf-8")
    report.check(f"{name}: README publishes the current OCI locator",
                 identity["oci_locator"] in readme)
    report.check(f"{name}: README publishes the composed artifact digest",
                 identity["composed_artifact"] in readme)
    report.check(f"{name}: README publishes the runtime digest",
                 identity["runtime_digest"] in readme)
    for field in ("runtime_digest", "composed_artifact", "composed_oci_manifest"):
        report.check(f"{name}: {document['evidence']['release_record']} publishes {field}",
                     identity[field] in record)


def validate_acceptance_contract(root, report):
    """Check that the acceptance runner emits a schema-valid report."""
    schema = load_schema(root, "acceptance-report.v1.json")
    digest = "sha256:" + "0" * 64
    empty = accept.finalize(accept.new_report(digest, "windows-x86_64-msvc143-py313"))
    report.check("acceptance: an empty report is schema valid",
                 not jsonschema_lite.errors_for(empty, schema))
    report.check("acceptance: an empty report is not complete",
                 empty["status"] == "failed" and not empty["complete"])

    passing = accept.new_report(digest, "windows-x86_64-msvc143-py313")
    for name in accept.REQUIRED_CHECKS:
        accept.record_check(passing, name, 0, f"{name}.json")
    accept.finalize(passing)
    errors = jsonschema_lite.errors_for(passing, schema)
    report.check("acceptance: a passing report is schema valid", not errors, "; ".join(errors))
    report.check("acceptance: a passing report is complete",
                 passing["status"] == "passed" and passing["complete"])

    verified = accept.new_report(digest, "windows-x86_64-msvc143-py313")
    for name in accept.REQUIRED_CHECKS:
        accept.record_check(verified, name, 0, f"{name}.json")
    accept.record_verification(verified, "tier2", False)
    accept.finalize(verified, ValueError("no remote range reads observed"))
    errors = jsonschema_lite.errors_for(verified, schema)
    report.check("acceptance: a failed verification is schema valid", not errors, "; ".join(errors))
    report.check("acceptance: a failed verification fails its own check",
                 verified["checks"]["tier2"]["status"] == "failed" and verified["status"] == "failed")

    partial = accept.new_report(digest, "windows-x86_64-msvc143-py313")
    accept.record_check(partial, accept.REQUIRED_CHECKS[0], 0, "sdk.json")
    accept.finalize(partial, RuntimeError("probe failed"))
    report.check("acceptance: a partial report fails",
                 partial["status"] == "failed" and "error" in partial)

    checks_schema = load_schema(root, "release-evidence.v1.json")["properties"]["checks"]
    baseline, declared = set(checks_schema["required"]), set(checks_schema["properties"])
    required = set(accept.REQUIRED_CHECKS)
    report.check("acceptance: every required check has a declared evidence shape",
                 required <= declared, ", ".join(sorted(required - declared)))
    report.check("acceptance: no check the evidence baseline requires was dropped",
                 baseline <= required, ", ".join(sorted(baseline - required)))


def validate_release(root, tag, report):
    """Check the release-gate invariants between VERSION, docs, and evidence."""
    version = (root / "VERSION").read_text(encoding="utf-8").strip()
    report.check(f"release: tag {tag} matches VERSION {version}", tag == f"v{version}")
    report.check(f"release: docs/releases/{tag}.md exists",
                 (root / "docs/releases" / f"{tag}.md").is_file())
    records = {}
    for path in (root / "evidence").glob("*.json"):
        record = json.loads(path.read_text(encoding="utf-8"))
        records.setdefault(record.get("release"), []).append(record)
    if not report.check(f"release: evidence records acceptance for {tag}", tag in records,
                        f"committed evidence covers {sorted(r for r in records if r)}"):
        return
    baseline = load_schema(root, "release-evidence.v1.json")["properties"]["checks"]["required"]
    for record in records[tag]:
        target = record["target"]
        lock_path = root / f"runtime.{slug_for_target(root, target)}.lock.json"
        if not report.check(f"release: {target} has a committed composition", lock_path.is_file(),
                            str(lock_path)):
            continue
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        report.check(f"release: {tag} accepts the committed {target} composition",
                     record["runtime_digest"] == lock["runtime_digest"],
                     f"{record['runtime_digest']} != {lock['runtime_digest']}")
        declared = required_checks_of(record, baseline)
        report.check(f"release: {tag} declares the current acceptance contract for {target}",
                     declared == set(accept.REQUIRED_CHECKS),
                     f"{sorted(declared)} != {sorted(accept.REQUIRED_CHECKS)}")


def slug_for_target(root, target):
    """Return the manifest slug whose lock resolves a target."""
    for manifest_path in runtime_metadata.manifests(root):
        slug = runtime_metadata.slug_of(manifest_path)
        lock_path = root / f"runtime.{slug}.lock.json"
        if lock_path.is_file():
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            if lock["resolved"]["target"] == target:
                return slug
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--release", metavar="TAG",
                        help="additionally check the release gate for a version tag")
    args = parser.parse_args(argv)
    root = args.root.resolve()

    report = Report()
    try:
        validate_composition(root, report)
        validate_evidence(root, report)
        validate_runtime_metadata(root, report)
        validate_acceptance_contract(root, report)
        if args.release:
            validate_release(root, args.release, report)
    except (jsonschema_lite.SchemaError, runtime_metadata.MetadataError, KeyError, OSError, ValueError) as error:
        print(f"FAIL validation aborted: {type(error).__name__}: {error}", file=sys.stderr)
        return 1

    if report.failures:
        print(f"\n{len(report.failures)} check(s) failed", file=sys.stderr)
        return 1
    print("\nall checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
