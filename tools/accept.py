"""Run the installed probes for the v0.1.0 Windows composition."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


def verify_tier2(record):
    rows = record["scenarios"]
    local = next(row for row in rows if row["scenario"] == "full-local")
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
    report = {
        "schema": 1,
        "scope": "v0.1.0-windows-acceptance",
        "runtime_digest": lock["runtime_digest"],
        "status": "failed",
        "full_v0_1_0_acceptance": False,
        "commands": [],
    }

    def run(label, command):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        (output / f"{label}.json").write_text(result.stdout, encoding="utf-8")
        (output / f"{label}.stderr.txt").write_text(result.stderr, encoding="utf-8")
        report["commands"].append({"probe": label, "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{label} failed; see {output / (label + '.json')}")
        return json.loads(result.stdout)

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
        verify_tier2(json.loads((output / "tier2-measurements.json").read_text(encoding="utf-8")))
        run("raster", base + [str(prefix / "share/usd-raster-plugins/probes/packaged_probe.py"),
                              "--prefix", str(prefix)])
        report["status"] = "passed"
        report["full_v0_1_0_acceptance"] = True
    except (RuntimeError, ValueError, OSError, subprocess.TimeoutExpired) as error:
        report["error"] = str(error)
    finally:
        (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
