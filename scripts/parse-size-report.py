#!/usr/bin/env python3
import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")
RAM_RE = re.compile(
    r"RAM:\s+\[[^\]]*\]\s+\s*[0-9.]+%\s+\(used\s+([0-9]+)\s+bytes\s+from\s+([0-9]+)\s+bytes\)",
    re.IGNORECASE,
)
FLASH_RE = re.compile(
    r"Flash:\s+\[[^\]]*\]\s+\s*[0-9.]+%\s+\(used\s+([0-9]+)\s+bytes\s+from\s+([0-9]+)\s+bytes\)",
    re.IGNORECASE,
)


def strip_ansi(text):
    return ANSI_RE.sub("", text)


def metric_from_match(match):
    if not match:
        return None, None
    return int(match.group(1)), int(match.group(2))


def parse_log(path):
    if not path.exists():
        return None, None, None, None, False

    text = strip_ansi(path.read_text(errors="replace"))
    ram_match = None
    flash_match = None
    for match in RAM_RE.finditer(text):
        ram_match = match
    for match in FLASH_RE.finditer(text):
        flash_match = match

    ram_used, ram_total = metric_from_match(ram_match)
    flash_used, flash_total = metric_from_match(flash_match)
    return ram_used, ram_total, flash_used, flash_total, bool(ram_match and flash_match)


def artifact_sizes(artifact_dir, env):
    sizes = {}
    if not artifact_dir.exists():
        return sizes

    for path in sorted(artifact_dir.glob(f"{env}-*")):
        if path.is_file():
            sizes[path.name] = path.stat().st_size
    return sizes


def load_baseline(path):
    data = json.loads(path.read_text())
    return {entry["env"]: entry for entry in data.get("environments", [])}


def artifact_kind(name):
    path = Path(name)
    suffix = path.suffix.lstrip(".") or "no_extension"
    if path.stem.endswith("-merged") and suffix == "bin":
        return "merged.bin"
    return suffix


def artifact_kind_sizes(artifacts):
    sizes = {}
    for name, size in artifacts.items():
        sizes[artifact_kind(name)] = size
    return sizes


def add_delta(entry, baseline_by_env):
    baseline = baseline_by_env.get(entry["env"])
    if not baseline:
        return

    delta = {}
    for key in ("ram_used", "flash_used"):
        current_value = entry.get(key)
        baseline_value = baseline.get(key)
        if current_value is not None and baseline_value is not None:
            delta[key] = current_value - baseline_value
        else:
            delta[key] = None

    baseline_artifacts = artifact_kind_sizes(baseline.get("artifact_bytes", {}))
    current_artifacts = artifact_kind_sizes(entry.get("artifact_bytes", {}))
    artifact_delta = {}
    for suffix, current_size in current_artifacts.items():
        baseline_size = baseline_artifacts.get(suffix)
        artifact_delta[suffix] = current_size - baseline_size if baseline_size is not None else None
    delta["artifact_bytes_by_kind"] = artifact_delta
    entry["delta"] = delta


def metric_limit(config, metric, level):
    value = config.get(metric, {})
    if isinstance(value, dict):
        return value.get(f"{level}_delta", value.get(level))
    return None


def env_threshold_config(thresholds, env):
    config = dict(thresholds.get("defaults", {}))
    config.update(thresholds.get("environments", {}).get(env, {}))
    return config


def evaluate_thresholds(summary, thresholds):
    checks = []
    status = "ok"
    for entry in summary["environments"]:
        delta = entry.get("delta")
        if not delta:
            checks.append({"env": entry["env"], "status": "not_evaluated", "reason": "no baseline delta"})
            continue

        config = env_threshold_config(thresholds, entry["env"])
        for metric in ("flash_used", "ram_used"):
            value = delta.get(metric)
            if value is None:
                checks.append({"env": entry["env"], "metric": metric, "status": "not_evaluated", "reason": "missing delta"})
                continue
            warn_limit = metric_limit(config, metric, "warn")
            fail_limit = metric_limit(config, metric, "fail")
            check_status = "ok"
            if fail_limit is not None and value > fail_limit:
                check_status = "fail"
                status = "fail"
            elif warn_limit is not None and value > warn_limit:
                check_status = "warn"
                if status == "ok":
                    status = "warn"
            checks.append({
                "env": entry["env"],
                "metric": metric,
                "delta": value,
                "warn_delta": warn_limit,
                "fail_delta": fail_limit,
                "status": check_status,
            })

        artifact_config = config.get("artifact_bytes_by_kind", {})
        for kind, value in delta.get("artifact_bytes_by_kind", {}).items():
            limits = artifact_config.get(kind, {})
            warn_limit = limits.get("warn_delta", limits.get("warn"))
            fail_limit = limits.get("fail_delta", limits.get("fail"))
            check_status = "ok"
            if fail_limit is not None and value > fail_limit:
                check_status = "fail"
                status = "fail"
            elif warn_limit is not None and value > warn_limit:
                check_status = "warn"
                if status == "ok":
                    status = "warn"
            checks.append({
                "env": entry["env"],
                "metric": f"artifact_bytes_by_kind.{kind}",
                "delta": value,
                "warn_delta": warn_limit,
                "fail_delta": fail_limit,
                "status": check_status,
            })

    return {"status": status, "checks": checks}


def report_lines(summary):
    lines = [f"Size summary ({summary['mode']})"]
    for entry in summary["environments"]:
        lines.append(f"- {entry['env']}:")
        lines.append(f"  RAM: {entry['ram_used']} / {entry['ram_total']} bytes")
        lines.append(f"  Flash: {entry['flash_used']} / {entry['flash_total']} bytes")
        if entry.get("delta"):
            delta = entry["delta"]
            lines.append(f"  Delta: RAM {delta.get('ram_used')} bytes, flash {delta.get('flash_used')} bytes")
        for name, size in sorted(entry.get("artifact_bytes", {}).items()):
            lines.append(f"  Artifact: {name} = {size} bytes")
    thresholds = summary.get("thresholds")
    if thresholds:
        lines.append(f"Threshold status: {thresholds['status']}")
        for check in thresholds["checks"]:
            if check["status"] == "ok":
                continue
            reason = check.get("reason", "")
            metric = check.get("metric", "all")
            delta = check.get("delta", "n/a")
            lines.append(f"- {check['env']} {metric}: {check['status']} delta={delta} {reason}".rstrip())
    return lines


def build_summary(args):
    log_dir = args.logs
    artifact_dir = args.artifacts
    envs = args.env or sorted(path.stem for path in log_dir.glob("*.log"))
    baseline_by_env = load_baseline(args.compare) if args.compare else {}

    entries = []
    for env in envs:
        log_path = log_dir / f"{env}.log"
        ram_used, ram_total, flash_used, flash_total, metrics_found = parse_log(log_path)
        entry = {
            "env": env,
            "ram_used": ram_used,
            "ram_total": ram_total,
            "flash_used": flash_used,
            "flash_total": flash_total,
            "artifact_bytes": artifact_sizes(artifact_dir, env),
            "log": str(log_path),
            "metrics_found": metrics_found,
        }
        add_delta(entry, baseline_by_env)
        entries.append(entry)

    mode = "compare" if args.compare else "baseline" if args.baseline else "summary"
    return {
        "mode": mode,
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "environments": entries,
    }


def main():
    parser = argparse.ArgumentParser(description="Parse PlatformIO size output and firmware artifact sizes.")
    parser.add_argument("--logs", type=Path, required=True, help="Directory containing <env>.log files")
    parser.add_argument("--artifacts", type=Path, required=True, help="Directory containing copied firmware artifacts")
    parser.add_argument("--env", action="append", help="Environment name to include; may be passed more than once")
    parser.add_argument("--baseline", action="store_true", help="Mark output as a baseline summary")
    parser.add_argument("--compare", type=Path, help="Baseline JSON to compare against")
    parser.add_argument("--thresholds", type=Path, help="JSON file with warn/fail size delta thresholds")
    parser.add_argument("--enforce-thresholds", action="store_true", help="Exit nonzero when threshold status is fail")
    parser.add_argument("--report", type=Path, help="Write a human-readable size report to this path")
    parser.add_argument("--output", type=Path, help="Write JSON summary to this path")
    args = parser.parse_args()

    summary = build_summary(args)
    if args.thresholds:
        thresholds = json.loads(args.thresholds.read_text())
        summary["thresholds"] = evaluate_thresholds(summary, thresholds)

    text = json.dumps(summary, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text("\n".join(report_lines(summary)) + "\n")
    print(text)

    if args.enforce_thresholds and summary.get("thresholds", {}).get("status") == "fail":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
