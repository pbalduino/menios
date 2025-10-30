#!/bin/sh
set -eu

PROJECT_ROOT=$(pwd)
SUMMARY_DIR=${1:-build/gcov}
JSON_DIR="${SUMMARY_DIR}/json"
SUMMARY_FILE="${SUMMARY_DIR}/summary.txt"

if ! command -v gcov >/dev/null 2>&1; then
  echo "error: gcov not found in PATH" >&2
  exit 1
fi

rm -rf "${SUMMARY_DIR}"
mkdir -p "${JSON_DIR}"

GCNO_LIST=$(find "${PROJECT_ROOT}" -name '*.gcno')
if [ -z "${GCNO_LIST}" ]; then
  echo "error: no .gcno files found. Run 'make GCOV=1 test' or 'make coverage' first." >&2
  exit 1
fi

for gcno in ${GCNO_LIST}; do
  gcno_abs=$(cd "${PROJECT_ROOT}" && realpath "${gcno}")
  gcno_dir=$(dirname "${gcno_abs}")
  (
    cd "${JSON_DIR}"
    gcov --json-format -o "${gcno_dir}" "${gcno_abs}" >/dev/null
  )
done

python3 - "${JSON_DIR}" "${SUMMARY_FILE}" <<'PY'
import gzip
import json
import os
import sys

json_dir = os.path.abspath(sys.argv[1])
summary_file = os.path.abspath(sys.argv[2])

files_data = {}

for entry in os.listdir(json_dir):
    if not entry.endswith('.gcov.json.gz'):
        continue
    path = os.path.join(json_dir, entry)
    with gzip.open(path, 'rt', encoding='utf-8') as handle:
        content = json.load(handle)
    for file_entry in content.get('files', []):
        filename = file_entry.get('file')
        if not filename:
            continue
        data = files_data.setdefault(
            filename,
            {
                'lines_all': set(),
                'lines_hit': set(),
                'branches_all': set(),
                'branches_hit': set(),
                'funcs_all': set(),
                'funcs_hit': set(),
            },
        )

        for line in file_entry.get('lines', []):
            line_no = line.get('line_number')
            if line_no is None:
                continue
            data['lines_all'].add(line_no)
            if line.get('count', 0) > 0:
                data['lines_hit'].add(line_no)
            for idx, branch in enumerate(line.get('branches', [])):
                key = (line_no, idx)
                data['branches_all'].add(key)
                if branch.get('count', 0) > 0:
                    data['branches_hit'].add(key)

        for func in file_entry.get('functions', []):
            key = (func.get('name'), func.get('start_line'))
            data['funcs_all'].add(key)
            if func.get('execution_count', 0) > 0 or func.get('blocks_executed', 0) > 0:
                data['funcs_hit'].add(key)

def pct(hit, total):
    if total == 0:
        return 0.0
    return (hit / total) * 100.0

overall_lines_total = sum(len(data['lines_all']) for data in files_data.values())
overall_lines_hit = sum(len(data['lines_hit']) for data in files_data.values())
overall_branches_total = sum(len(data['branches_all']) for data in files_data.values())
overall_branches_hit = sum(len(data['branches_hit']) for data in files_data.values())
overall_funcs_total = sum(len(data['funcs_all']) for data in files_data.values())
overall_funcs_hit = sum(len(data['funcs_hit']) for data in files_data.values())

with open(summary_file, 'w', encoding='utf-8') as out:
    out.write("Overall coverage\n")
    out.write("================\n")
    out.write(
        f"Lines:     {overall_lines_hit}/{overall_lines_total} "
        f"({pct(overall_lines_hit, overall_lines_total):.2f}%)\n"
    )
    out.write(
        f"Functions: {overall_funcs_hit}/{overall_funcs_total} "
        f"({pct(overall_funcs_hit, overall_funcs_total):.2f}%)\n"
    )
    out.write(
        f"Branches:  {overall_branches_hit}/{overall_branches_total} "
        f"({pct(overall_branches_hit, overall_branches_total):.2f}%)\n\n"
    )
    out.write("Per-file coverage (lines / functions / branches)\n")
    out.write("-----------------------------------------------\n")

    for filename in sorted(files_data.keys()):
        data = files_data[filename]
        lines_total = len(data['lines_all'])
        lines_hit = len(data['lines_hit'])
        funcs_total = len(data['funcs_all'])
        funcs_hit = len(data['funcs_hit'])
        branches_total = len(data['branches_all'])
        branches_hit = len(data['branches_hit'])
        out.write(
            f"{filename}: "
            f"{pct(lines_hit, lines_total):5.2f}% "
            f"/ {pct(funcs_hit, funcs_total):5.2f}% "
            f"/ {pct(branches_hit, branches_total):5.2f}%\n"
        )
PY

echo "Coverage summary written to ${SUMMARY_FILE}"
