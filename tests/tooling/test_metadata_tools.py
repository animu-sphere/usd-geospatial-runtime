"""Tests for the metadata, schema, and acceptance-report tooling.

Run with a bare CPython 3.13 host:

    python tests/tooling/test_metadata_tools.py
"""

import contextlib
import copy
import io
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import traceback

ROOT = Path(__file__).resolve().parents[2]
SDK_TEST_DATA = ROOT / "sdk/core/tests/data"
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
    unreached = {
        "type": "object",
        "properties": {"detail": {"type": "string", "maxLength": 8}},
        "$defs": {"unused": {"type": "string", "format": "uri"}},
    }
    for document, reason in (
        ({"type": "object", "patternProperties": {}}, "a keyword on the root schema"),
        (unreached, "a keyword under a property the instance omits"),
    ):
        try:
            jsonschema_lite.errors_for({}, document)
        except jsonschema_lite.SchemaError:
            continue
        raise AssertionError(f"an unsupported keyword was silently ignored: {reason}")


def test_lite_validator_separates_booleans_from_numbers():
    document = {"properties": {"schema": {"const": 1}, "count": {"enum": [0, 1]}}}
    assert not jsonschema_lite.errors_for({"schema": 1, "count": 0}, document)
    rejects({"schema": True}, document, "true used where the const is 1")
    rejects({"count": False}, document, "false used where the enum holds 0")


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


def runtime_info_document():
    return json.loads((SDK_TEST_DATA / "runtime-info.json").read_text(encoding="utf-8"))


def test_runtime_info_schema_matches_the_document_the_sdk_emits():
    """The committed document is the contract shared by the SDK and this schema.

    `sdk/core/tests/test_core.cpp` asserts that `RuntimeInfo::to_json` produces
    this exact text, and this asserts that the same text satisfies the schema.
    Neither side can move alone.
    """
    document = schema("runtime-info.v1.json")
    instance = runtime_info_document()
    assert not jsonschema_lite.errors_for(instance, document)

    record = runtime_info_document()
    del record["identity"]["runtime_digest"]
    rejects(record, document, "a runtime with no runtime digest")

    record = runtime_info_document()
    record["target"]["id"] = "windows-x86_64-py313"
    rejects(record, document, "a target that is not canonical")

    record = runtime_info_document()
    record["capabilities"] = []
    rejects(record, document, "a runtime that resolves no capability")

    record = runtime_info_document()
    record["components"][0]["kind"] = "bundle"
    rejects(record, document, "a component of an unknown kind")

    record = runtime_info_document()
    record["evidence"] = {"release": "v0.1.0"}
    rejects(record, document, "release facts a materialized prefix cannot know")


def test_runtime_info_document_describes_the_lock_it_was_read_from():
    """The document is generated, so it must agree with its input fixture."""
    lock = json.loads((SDK_TEST_DATA / "composition.lock.json").read_text(encoding="utf-8"))
    info = runtime_info_document()
    resolved = lock["resolved"]

    assert info["runtime"]["name"] == resolved["name"]
    assert info["identity"]["runtime_digest"] == lock["runtime_digest"]
    assert info["identity"]["manifest_digest"] == resolved["manifest_digest"]
    assert info["identity"]["composition_digest"] == resolved["composition_digest"]

    # The SDK decomposes the target exactly as runtime_metadata.target_identity
    # does, so the two descriptions of one runtime cannot disagree.
    expected = runtime_metadata.target_identity(
        resolved["target"],
        info["target"]["usd_version"],
        any(runtime_metadata.INTERPRETER_PATTERN.search(item["destination"])
            for item in resolved["install"]),
    )
    assert info["target"] == expected

    assert [item["id"] for item in info["components"]] == sorted(
        item["id"] for item in resolved["components"]
    ), "components must be sorted by id so equal runtimes serialize equally"
    assert [item["capability"] for item in info["capabilities"]] == sorted(
        item["capability"] for item in resolved["providers"]
    )
    for component in info["components"]:
        source = next(item for item in resolved["components"] if item["id"] == component["id"])
        assert component["artifact"] == source["digest"]
        assert component["version"] == source["version"]

    usd = next(item for item in info["capabilities"] if item["capability"] == "usd")
    assert info["target"]["usd_version"] == usd["version"]


def test_diagnostic_reference_matches_the_sdk_source():
    """The published code table cannot drift from the one the SDK compiles.

    Codes are an automation contract: a caller branches on `UGEO-E031` because
    this repository published it. Reading both tables here means a renamed or
    renumbered code fails CI rather than silently invalidating the reference.
    """
    source = (ROOT / "sdk/core/src/diagnostics.cpp").read_text(encoding="utf-8")
    ids = dict(re.findall(r'case DiagnosticCode::(\w+): return "(UGEO-E\d+)";', source))
    categories = {}
    grouped = re.findall(r"((?:\s*case DiagnosticCode::\w+:\n)+)\s*return Category::(\w+);", source)
    for group, name in grouped:
        for member in re.findall(r"DiagnosticCode::(\w+)", group):
            categories[member] = name
    assert ids, "no diagnostic ids were found in the SDK source"
    assert set(categories) == set(ids), "every code must be given a category"

    page = (ROOT / "docs/reference/diagnostics.md").read_text(encoding="utf-8")
    rows = re.findall(r"^\| `(UGEO-E\d+)` \| `(\w+)` \| (\w+) \|", page, re.MULTILINE)
    assert rows, "the diagnostics reference lists no codes"

    documented = {name: (code, category) for code, name, category in rows}
    assert set(documented) == set(ids), (
        "the diagnostics reference and the SDK source list different codes: "
        f"{sorted(set(documented) ^ set(ids))}"
    )
    for name, (code, category) in documented.items():
        assert ids[name] == code, f"{name} is {ids[name]} in the SDK and {code} in the reference"
        assert categories[name] == category, f"{name} has a different category in the reference"

    assert len(set(ids.values())) == len(ids), "two codes share an id"


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

    accept.finalize(report, StopIteration())
    assert report["error"] == "StopIteration", "an errorless exception must still name itself"
    assert not jsonschema_lite.errors_for(report, document), "a failure report must match its schema"


def test_acceptance_report_records_derived_verification():
    document = schema("acceptance-report.v1.json")
    report = accept.new_report(DIGEST, "windows-x86_64-msvc143-py313")
    for name in accept.REQUIRED_CHECKS:
        accept.record_check(report, name, 0, f"{name}.json")
    accept.record_verification(report, "tier2", False)
    accept.finalize(report, ValueError("no remote range reads observed"))
    assert report["checks"]["tier2"] == {
        "status": "failed", "exit_code": 0, "output": "tier2.json", "verification": "failed",
    }, "a probe that exits zero but fails verification must say both"
    assert report["status"] == "failed"
    assert not jsonschema_lite.errors_for(report, document)

    report.pop("error")
    accept.record_check(report, "tier2", 0, "tier2.json")
    accept.record_verification(report, "tier2", True)
    accept.finalize(report)
    assert report["checks"]["tier2"]["verification"] == "passed"
    assert report["status"] == "passed"


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


def test_metadata_generation_requires_digest_pinned_sources():
    manifest = (ROOT / "runtime-composition.windows.toml").read_text(encoding="utf-8")
    tagged = manifest.replace(
        "oci://ghcr.io/animu-sphere/usd-raster-plugins@sha256:29bb6712a73aa422c1196cb7cbaa64403703dbeb7af64f47c5d9536c773024de",
        "oci://ghcr.io/animu-sphere/usd-raster-plugins:0.1.0")
    assert tagged != manifest
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for name in COPIED:
            destination = root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(ROOT / name, destination)
        (root / "runtime-composition.windows.toml").write_text(tagged, encoding="utf-8")
        try:
            runtime_metadata.build(root, root / "runtime-composition.windows.toml")
        except runtime_metadata.MetadataError:
            pass
        else:
            raise AssertionError("a tag-pinned source produced metadata")
        assert run_validator(root) == 1, "a tag-pinned source must fail validation"


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

        evidence_path = root / "evidence/v0.1.0-windows.json"
        original_evidence = evidence_path.read_text(encoding="utf-8")

        record = json.loads(original_evidence)
        record["runtime_digest"] = DIGEST
        evidence_path.write_text(json.dumps(record, indent=2), encoding="utf-8")
        assert run_validator(root) == 1, "evidence that contradicts the lock must fail"

        for check, value in (("sdk", "failed"), ("raster", {"status": "failed", "format": "tif"})):
            record = json.loads(original_evidence)
            record["checks"][check] = value
            evidence_path.write_text(json.dumps(record, indent=2), encoding="utf-8")
            assert run_validator(root) == 1, f"a failed {check} check must fail the record"
        evidence_path.write_text(original_evidence, encoding="utf-8")

        lock = root / "runtime.windows.lock.json"
        lock.unlink()
        assert run_validator(root) == 1, "a missing lock must fail rather than crash"


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    failures = 0
    for test in tests:
        try:
            test()
        except AssertionError as error:
            failures += 1
            print(f"FAIL {test.__name__}: {error}")
        except Exception:
            failures += 1
            print(f"ERROR {test.__name__}:")
            traceback.print_exc()
        else:
            print(f"ok   {test.__name__}")
    if failures:
        print(f"\n{failures} of {len(tests)} tests failed", file=sys.stderr)
        return 1
    print(f"\n{len(tests)} tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
