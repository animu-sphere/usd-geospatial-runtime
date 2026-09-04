"""Tests for the metadata, schema, and acceptance-report tooling.

Run with a bare CPython 3.13 host:

    python tests/tooling/test_metadata_tools.py
"""

import contextlib
import copy
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import accept
import jsonschema_lite
import runtime_metadata
import validate_metadata

DIGEST = "sha256:" + "0" * 64
COPIED = [
    "README.md",
    "VERSION",
    "runtime-composition.windows.toml",
    "runtime.windows.lock.json",
    "runtime-metadata.windows.json",
    "evidence/v0.1.0-windows.json",
    "docs/releases/v0.1.0.md",
    "schemas/acceptance-report.v1.json",
    "schemas/release-evidence.v1.json",
    "schemas/runtime-metadata.v1.json",
]


def schema(name):
    return json.loads((ROOT / "schemas" / name).read_text(encoding="utf-8"))


def evidence():
    return json.loads((ROOT / "evidence/v0.1.0-windows.json").read_text(encoding="utf-8"))


def rejects(instance, document, reason):
    errors = jsonschema_lite.errors_for(instance, document)
    assert errors, f"schema accepted {reason}"


def test_lite_validator_enforces_keywords():
    document = {
        "type": "object",
        "required": ["name"],
        "additionalProperties": False,
        "properties": {
            "name": {"type": "string", "pattern": "^a+$", "minLength": 2},
            "count": {"type": "integer", "minimum": 1},
            "tags": {"type": "array", "minItems": 1, "items": {"enum": ["x", "y"]}},
        },
    }
    assert not jsonschema_lite.errors_for({"name": "aa", "count": 1, "tags": ["x"]}, document)
    rejects({}, document, "a missing required property")
    rejects({"name": "b"}, document, "a value that fails its pattern")
    rejects({"name": "a"}, document, "a value shorter than minLength")
    rejects({"name": "aa", "extra": 1}, document, "an undeclared property")
    rejects({"name": "aa", "count": 0}, document, "a value below minimum")
    rejects({"name": "aa", "count": True}, document, "a boolean used as an integer")
    rejects({"name": "aa", "tags": []}, document, "an array below minItems")
    rejects({"name": "aa", "tags": ["z"]}, document, "a value outside its enum")


def test_lite_validator_rejects_unsupported_keywords():
    try:
        jsonschema_lite.errors_for({}, {"type": "object", "patternProperties": {}})
    except jsonschema_lite.SchemaError:
        return
    raise AssertionError("an unsupported keyword was silently ignored")


def test_lite_validator_resolves_references_and_alternatives():
    document = {
        "type": "object",
        "properties": {"result": {"$ref": "#/$defs/check"}},
        "$defs": {
            "check": {"oneOf": [
                {"enum": ["passed", "failed"]},
                {"type": "object", "required": ["status"], "properties": {"status": {"enum": ["passed", "failed"]}}},
            ]},
        },
    }
    assert not jsonschema_lite.errors_for({"result": "passed"}, document)
    assert not jsonschema_lite.errors_for({"result": {"status": "failed", "detail": 1}}, document)
    rejects({"result": "unknown"}, document, "a status outside the enum")
    rejects({"result": {}}, document, "an object without a status")


def test_release_evidence_schema_rejects_incomplete_records():
    document = schema("release-evidence.v1.json")
    assert not jsonschema_lite.errors_for(evidence(), document)

    record = evidence()
    del record["checks"]["raster"]
    rejects(record, document, "evidence missing a required check")

    record = evidence()
    record["status"] = "ok"
    rejects(record, document, "an unknown status value")

    record = evidence()
    record["runtime_digest"] = "sha256:abc"
    rejects(record, document, "a malformed digest")

    record = evidence()
    record["checks"]["http"] = "passed"
    rejects(record, document, "an http check without its measurements")

    record = evidence()
    record["clean_public_input_compose"] = "yes"
    rejects(record, document, "a non-boolean reproduction claim")

    record = evidence()
    record["notes"] = "extra"
    rejects(record, document, "an undeclared top-level property")

    record = evidence()
    record["checks"]["vector"] = "passed"
    assert not jsonschema_lite.errors_for(record, document), "an extra capability check must be allowed"


def test_runtime_metadata_schema_rejects_tampered_documents():
    document = schema("runtime-metadata.v1.json")
    generated = runtime_metadata.build(ROOT, ROOT / "runtime-composition.windows.toml")
    assert not jsonschema_lite.errors_for(generated, document)

    tampered = copy.deepcopy(generated)
    tampered["identity"]["oci_locator"] = "oci://ghcr.io/animu-sphere/usd-geospatial-runtime:latest"
    rejects(tampered, document, "a locator that is not pinned by digest")

    tampered = copy.deepcopy(generated)
    tampered["target"]["os"] = "freebsd"
    rejects(tampered, document, "an unsupported operating system")

    tampered = copy.deepcopy(generated)
    tampered["components"][0]["kind"] = "probe"
    rejects(tampered, document, "an unknown component kind")

    tampered = copy.deepcopy(generated)
    del tampered["capabilities"][0]["artifact"]
    rejects(tampered, document, "a capability without its artifact")


def test_target_identity_requires_a_canonical_target():
    identity = runtime_metadata.target_identity("windows-x86_64-msvc143-py313", "26.08", False)
    assert identity == {
        "id": "windows-x86_64-msvc143-py313",
        "os": "windows",
        "arch": "x86_64",
        "toolchain": "msvc143",
        "host_abi": "py313",
        "usd_version": "26.08",
        "host_python": {"version": "3.13", "bundled": False},
    }
    assert runtime_metadata.target_identity("linux-x86_64-gcc13-py313", "26.08", True)["os"] == "linux"
    for target in ("windows-x86_64-msvc143", "windows_x86_64_msvc143_py313", "plan9-x86_64-gcc13-py313"):
        try:
            runtime_metadata.target_identity(target, "26.08", False)
        except runtime_metadata.MetadataError:
            continue
        raise AssertionError(f"accepted non-canonical target {target!r}")


def test_acceptance_report_tracks_required_checks():
    document = schema("acceptance-report.v1.json")
    report = accept.new_report(DIGEST, "windows-x86_64-msvc143-py313")
    for name in accept.REQUIRED_CHECKS[:-1]:
        accept.record_check(report, name, 0, f"{name}.json")
    accept.finalize(report)
    assert not jsonschema_lite.errors_for(report, document)
    assert report["status"] == "failed" and not report["complete"], "a missing check must fail the report"

    accept.record_check(report, accept.REQUIRED_CHECKS[-1], 1, "raster.json")
    accept.finalize(report)
    assert report["checks"]["raster"]["status"] == "failed"
    assert report["status"] == "failed"

    accept.record_check(report, accept.REQUIRED_CHECKS[-1], 0, "raster.json")
    accept.finalize(report)
    assert report["status"] == "passed" and report["complete"]
    assert not jsonschema_lite.errors_for(report, document)

    accept.finalize(report, RuntimeError("tier2 verification failed"))
    assert report["status"] == "failed" and report["error"] == "tier2 verification failed"


def test_tier2_verification_rejects_unstable_evidence():
    def scenario(name, **extra):
        row = {"scenario": name, "opened": True, "pointCount": 3, "pointDigest": "d",
               "origin": {"rangeRequests": 2}}
        row.update(extra)
        return row

    record = {"scenarios": [
        scenario("full-local"),
        scenario("full-http", revision="A", hasValidationToken=True, validationTokenDigest="a"),
        scenario("full-http", revision="B", hasValidationToken=True, validationTokenDigest="b"),
    ]}
    accept.verify_tier2(record)

    for mutate, reason in (
        (lambda rows: rows[2].update(validationTokenDigest="a"), "an unchanged validator after a revision change"),
        (lambda rows: rows[2].update(hasValidationToken=False), "a strong ETag without a validation identity"),
        (lambda rows: rows[1].update(pointDigest="other"), "remote points that differ from the local baseline"),
        (lambda rows: rows[0].update(pointCount=0), "a baseline that authored no points"),
        (lambda rows: rows[2].update(origin={"rangeRequests": 0}) or rows[1].update(origin={"rangeRequests": 0}),
         "remote reads without range requests"),
    ):
        broken = copy.deepcopy(record)
        mutate(broken["scenarios"])
        try:
            accept.verify_tier2(broken)
        except ValueError:
            continue
        raise AssertionError(f"tier2 verification accepted {reason}")


def run_validator(root):
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return validate_metadata.main(["--root", str(root)])


def test_validator_detects_committed_drift():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for name in COPIED:
            destination = root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(ROOT / name, destination)
        assert run_validator(root) == 0, "an unmodified copy must validate"

        readme = root / "README.md"
        original = readme.read_text(encoding="utf-8")
        readme.write_text(original.replace("sha256:182e5382", "sha256:000e5382"), encoding="utf-8")
        assert run_validator(root) == 1, "a stale README locator must fail"
        readme.write_text(original, encoding="utf-8")

        record = json.loads((root / "evidence/v0.1.0-windows.json").read_text(encoding="utf-8"))
        record["runtime_digest"] = DIGEST
        (root / "evidence/v0.1.0-windows.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
        assert run_validator(root) == 1, "evidence that contradicts the lock must fail"


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    failures = 0
    for test in tests:
        try:
            test()
        except AssertionError as error:
            failures += 1
            print(f"FAIL {test.__name__}: {error}")
        else:
            print(f"ok   {test.__name__}")
    if failures:
        print(f"\n{failures} of {len(tests)} tests failed", file=sys.stderr)
        return 1
    print(f"\n{len(tests)} tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
