"""Run the installed acceptance probes against a composed runtime prefix.

The runner writes an acceptance report that conforms to
`schemas/acceptance-report.v1.json`. The report is generated output, not a
release record: release evidence is written only after the published composed
artifact has also been pulled, verified, and reconstructed.
"""

import argparse
import json
from pathlib import Path
import subprocess
import sys

CONTRACT = "usd-geospatial-runtime.acceptance/v1"
REQUIRED_CHECKS = ("sdk", "http", "pointcloud", "tier2", "raster", "vector")


def new_report(runtime_digest, target=None):
    """Return an acceptance report with no check recorded yet."""
    return {
        "schema": 1,
        "contract": CONTRACT,
        "runtime_digest": runtime_digest,
        "target": target or "unknown",
        "status": "failed",
        "complete": False,
        "required_checks": list(REQUIRED_CHECKS),
        "checks": {},
    }


def record_check(report, name, exit_code, output):
    report["checks"][name] = {
        "status": "passed" if exit_code == 0 else "failed",
        "exit_code": exit_code,
        "output": output,
    }


def record_verification(report, name, passed):
    """Record the derived verification applied to a check's probe output."""
    check = report["checks"][name]
    check["verification"] = "passed" if passed else "failed"
    if not passed:
        check["status"] = "failed"


def finalize(report, error=None):
    """Set the terminal status from the recorded checks."""
    if error is not None:
        report["error"] = str(error) or type(error).__name__
    checks = report["checks"]
    passed = all(checks.get(name, {}).get("status") == "passed" for name in report["required_checks"])
    report["complete"] = bool(passed and error is None)
    report["status"] = "passed" if report["complete"] else "failed"
    return report


def verify_tier2(record):
    rows = record["scenarios"]
    local = next((row for row in rows if row["scenario"] == "full-local"), None)
    if local is None:
        raise ValueError("the tier2 measurements contain no local COPC baseline")
    if not local.get("opened") or local.get("pointCount", 0) < 1:
        raise ValueError("local COPC baseline did not author points")
    for row in rows:
        if not row.get("opened"):
            raise ValueError("a COPC scenario did not open")
        if row["scenario"] != "metadata" and row.get("pointDigest") != local["pointDigest"]:
            raise ValueError("remote COPC points differ from the local baseline")
    remote = [row for row in rows if "revision" in row]
    if not remote or not any(row["origin"]["rangeRequests"] > 0 for row in remote):
        raise ValueError("no remote range reads observed")
    tokens = {}
    for row in remote:
        if row["revision"] in ("A", "B"):
            if not row.get("hasValidationToken"):
                raise ValueError("strong ETag did not produce a validation identity")
            tokens[row["revision"]] = row["validationTokenDigest"]
        elif row.get("hasValidationToken"):
            raise ValueError("weak ETag was incorrectly treated as stable")
    if set(tokens) != {"A", "B"} or tokens["A"] == tokens["B"]:
        raise ValueError("changed strong ETag did not invalidate the identity")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--composition", type=Path, required=True)
    parser.add_argument("--python", type=Path, default=Path(sys.executable))
    parser.add_argument("--ost", default="ost")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prefix, output = args.composition.resolve(), args.output.resolve()
    if output == prefix or prefix in output.parents:
        parser.error("evidence must be outside the immutable prefix")
    output.mkdir(parents=True, exist_ok=False)
    lock = json.loads((prefix / "metadata/composition.lock.json").read_text(encoding="utf-8"))
    report = new_report(lock["runtime_digest"], lock.get("resolved", {}).get("target"))

    def run(label, command):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        (output / f"{label}.json").write_text(result.stdout, encoding="utf-8")
        (output / f"{label}.stderr.txt").write_text(result.stderr, encoding="utf-8")
        record_check(report, label, result.returncode, f"{label}.json")
        if result.returncode:
            raise RuntimeError(f"{label} failed; see {output / (label + '.json')}")
        return json.loads(result.stdout)

    failure = None
    try:
        run("sdk", [args.ost, "--json", "runtime", "validate", "--composition", str(prefix), "--sdk"])
        base = [args.ost, "--json", "runtime", "exec", "--composition", str(prefix), "--", str(args.python.resolve())]
        run("http", base + [str(prefix / "share/usd-http-resolver/probes/packaged_probe.py")])
        fixture = Path(__file__).resolve().parents[1] / "fixtures/three-points.copc"
        run("pointcloud", base + [str(prefix / "share/usd-pointcloud-plugins/probes/packaged_probe.py"),
                                  "--prefix", str(prefix), "--copc-fixture", str(fixture)])
        run("tier2", base + [str(prefix / "share/usd-pointcloud-plugins/probes/tier2_resolver_integration.py"),
                             "--fixture", str(fixture),
                             "--resolver-resources", str(prefix / "bundles/http-resolver/plugin/resources/httpResolver"),
                             "--copc-resources", str(prefix / "bundles/pointcloud-copc/plugin/resources/pointcloud-copc"),
                             "--output", str(output / "tier2-measurements.json")])
        try:
            verify_tier2(json.loads((output / "tier2-measurements.json").read_text(encoding="utf-8")))
        except (ValueError, KeyError):
            record_verification(report, "tier2", False)
            raise
        record_verification(report, "tier2", True)
        run("raster", base + [str(prefix / "share/usd-raster-plugins/probes/packaged_probe.py"),
                              "--prefix", str(prefix)])
        run("vector", base + [str(prefix / "share/usd-vector-plugins/probes/packaged_probe.py"),
                              "--prefix", str(prefix)])
    except (RuntimeError, ValueError, KeyError, OSError, subprocess.TimeoutExpired) as error:
        failure = error
    finally:
        finalize(report, failure)
        (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
