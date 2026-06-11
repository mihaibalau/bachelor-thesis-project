#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./run-benchmark.sh -b <c|rust> -w <w1|w2|w3|w4|all> [options]

Options:
  -b BACKEND   c or rust (label for output files)
  -w WORKLOAD  w1, w2, w3, w4, or all
  -u URL       BASE_URL (default: http://localhost:6767)
  -n RUNS      number of runs (default: 3)
  -h           show this help

Examples:
  ./run-benchmark.sh -b c -w w1
  ./run-benchmark.sh -b rust -w all -u http://127.0.0.1:6767
EOF
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS="$ROOT/results"
BACKEND=""
WORKLOAD=""
BASE_URL="http://localhost:6767"
RUNS=3

while getopts ":b:w:u:n:h" opt; do
  case "$opt" in
    b) BACKEND="$OPTARG" ;;
    w) WORKLOAD="$OPTARG" ;;
    u) BASE_URL="$OPTARG" ;;
    n) RUNS="$OPTARG" ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
  esac
done

if [[ -z "$BACKEND" || -z "$WORKLOAD" ]]; then
  usage
  exit 1
fi

case "$BACKEND" in
  c|rust) ;;
  *) echo "Invalid backend: $BACKEND (use c or rust)" >&2; exit 1 ;;
esac

case "$WORKLOAD" in
  w1|w2|w3|w4|all) ;;
  *) echo "Invalid workload: $WORKLOAD" >&2; exit 1 ;;
esac

if ! command -v k6 >/dev/null 2>&1; then
  echo "k6 is not installed. On WSL/Ubuntu: sudo apt-get install k6" >&2
  exit 1
fi

preflight_backend() {
  local url="$1/health"
  if curl -sf --connect-timeout 3 "$url" >/dev/null 2>&1; then
    return 0
  fi
  return 1
}

# WSL: backend on Windows is not reachable via localhost — try the Windows host IP.
if ! preflight_backend "$BASE_URL"; then
  if grep -qi microsoft /proc/version 2>/dev/null; then
    WIN_HOST="$(grep -m1 nameserver /etc/resolv.conf | awk '{print $2}')"
    ALT="http://${WIN_HOST}:6767"
    if [[ -n "$WIN_HOST" ]] && preflight_backend "$ALT"; then
      echo "Note: localhost:6767 refused — backend found on Windows host at $ALT"
      BASE_URL="$ALT"
      export BASE_URL
    fi
  fi
fi

if ! preflight_backend "$BASE_URL"; then
  cat >&2 <<EOF
ERROR: Backend not reachable at ${BASE_URL}/health

  connection refused usually means:
  1. The C/Rust backend is not running, or
  2. It runs on Windows while k6 runs in WSL (localhost does not forward).

Fix:
  • Start one backend on port 6767 (C or Rust).
  • From WSL, if the backend is on Windows:
      WIN_HOST=\$(grep -m1 nameserver /etc/resolv.conf | awk '{print \$2}')
      ./run-benchmark.sh -b c -w w1 -u "http://\${WIN_HOST}:6767"

  • Quick check:
      curl -v ${BASE_URL}/health

Press Ctrl+C if a k6 run is already flooding errors — it was hitting a dead port.
EOF
  exit 1
fi

echo "Backend OK at ${BASE_URL}/health"

declare -A SCRIPT_MAP=(
  [w1]="k6/w1_login.js"
  [w2]="k6/w2_statement.js"
  [w3]="k6/w3_analytics.js"
  [w4]="k6/w4_transfer.js"
)

mkdir -p "$RESULTS"
export BASE_URL

if [[ "$WORKLOAD" == "all" ]]; then
  WORKLOADS=(w1 w2 w3 w4)
else
  WORKLOADS=("$WORKLOAD")
fi

for w in "${WORKLOADS[@]}"; do
  script="$ROOT/${SCRIPT_MAP[$w]}"
  if [[ ! -f "$script" ]]; then
    echo "Script not found: $script" >&2
    exit 1
  fi

  echo "=== $w ($BACKEND) -> $BASE_URL ==="

  for ((i = 1; i <= RUNS; i++)); do
    out="$RESULTS/${w}-${BACKEND}-run${i}.json"
    echo "Run $i/$RUNS -> $out"
    k6 run --summary-export "$out" "$script"
  done
done

echo "Done. Use http_req_duration p(95) and http_reqs rate from each JSON summary."
