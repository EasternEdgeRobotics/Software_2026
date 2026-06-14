#!/usr/bin/env bash

set -u
set -o pipefail

CAPTURES_DIR="${1:-/Users/peyton/Pictures/bluestar_captures/}"
OUTPUT_DIR="${2:-bluestar_realitykit_outputs}"
DETAIL="${DETAIL:-full}"
PHOTOMODEL_BIN="${PHOTOMODEL_BIN:-/Users/peyton/Developer/Eastern Edge/Software_2026/science_officer/coral_garden/realitykit_scale/.build/release/realitykit_scale}"
FORCE="${FORCE:-0}"
SEQUENTIAL="${SEQUENTIAL:-1}"
KEEP_STAGING="${KEEP_STAGING:-0}"

mkdir -p "$OUTPUT_DIR"

LOG_DIR="$OUTPUT_DIR/logs"
mkdir -p "$LOG_DIR"

SUMMARY_FILE="$OUTPUT_DIR/summary.tsv"
SECTION_LIST="$(mktemp -t realitykit_sections.XXXXXX)"

cleanup() {
  rm -f "$SECTION_LIST"
}

trap cleanup EXIT

printf "input_folder\toutput_file\tstatus\n" > "$SUMMARY_FILE"

echo "Captures dir: $CAPTURES_DIR"
echo "Output dir:   $OUTPUT_DIR"
echo "Detail:       $DETAIL"
echo "CLI:          $PHOTOMODEL_BIN"
echo ""

if [[ ! -x "$PHOTOMODEL_BIN" ]]; then
  if ! command -v "$PHOTOMODEL_BIN" >/dev/null 2>&1; then
    echo "Error: could not find executable photomodel CLI:"
    echo "  $PHOTOMODEL_BIN"
    echo ""
    echo "Either fix PHOTOMODEL_BIN in the script, or run like:"
    echo "  PHOTOMODEL_BIN=/path/to/realitykit_scale ./realitykit_batch_tester.sh"
    exit 1
  fi
fi

if [[ ! -d "$CAPTURES_DIR" ]]; then
  echo "Error: captures directory does not exist:"
  echo "  $CAPTURES_DIR"
  exit 1
fi

find "$CAPTURES_DIR" \
  -mindepth 2 \
  -maxdepth 2 \
  -type d \
  -name 'section_*' \
  | sort > "$SECTION_LIST"

TOTAL="$(wc -l < "$SECTION_LIST" | tr -d ' ')"

if [[ "$TOTAL" -eq 0 ]]; then
  echo "No section folders found under:"
  echo "  $CAPTURES_DIR"
  exit 1
fi

echo "Found $TOTAL section folder(s)."
echo ""

INDEX=0
SUCCESS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

while IFS= read -r SECTION_DIR; do
  INDEX=$((INDEX + 1))

  CAPTURE_NAME="$(basename "$(dirname "$SECTION_DIR")")"
  SECTION_NAME="$(basename "$SECTION_DIR")"

  OUTPUT_NAME="${CAPTURE_NAME}_${SECTION_NAME}.usdz"
  OUTPUT_FILE="$OUTPUT_DIR/$OUTPUT_NAME"

  LOG_NAME="${CAPTURE_NAME}_${SECTION_NAME}.log"
  LOG_FILE="$LOG_DIR/$LOG_NAME"

  echo "[$INDEX/$TOTAL] $SECTION_DIR"
  echo "  -> $OUTPUT_FILE"

  if [[ -f "$OUTPUT_FILE" && "$FORCE" != "1" ]]; then
    echo "  Skipping; output already exists."
    printf "%s\t%s\tskipped\n" \
      "$SECTION_DIR" \
      "$OUTPUT_FILE" \
      >> "$SUMMARY_FILE"
    SKIP_COUNT=$((SKIP_COUNT + 1))
    echo ""
    continue
  fi

  TIFF_COUNT="$(
    find "$SECTION_DIR" \
      -maxdepth 1 \
      -type f \
      \( -iname '*.tif' -o -iname '*.tiff' \) \
      | wc -l \
      | tr -d ' '
  )"

  if [[ "$TIFF_COUNT" -eq 0 ]]; then
    echo "  Skipping; no TIFFs found."
    printf "%s\t%s\tno_tiffs\n" \
      "$SECTION_DIR" \
      "$OUTPUT_FILE" \
      >> "$SUMMARY_FILE"
    SKIP_COUNT=$((SKIP_COUNT + 1))
    echo ""
    continue
  fi

  echo "  TIFFs: $TIFF_COUNT"

  ARGS=(
    "--input"
    "$SECTION_DIR"
    "--output"
    "$OUTPUT_FILE"
    "--detail"
    "$DETAIL"
  )

  if [[ "$SEQUENTIAL" == "1" ]]; then
    ARGS+=("--sequential")
  else
    ARGS+=("--unordered")
  fi

  if [[ "$KEEP_STAGING" == "1" ]]; then
    STAGING_DIR="$OUTPUT_DIR/staging/${CAPTURE_NAME}_${SECTION_NAME}"
    mkdir -p "$STAGING_DIR"
    ARGS+=("--staging" "$STAGING_DIR" "--keep-staging")
  fi

  START_SECONDS="$(date +%s)"

  if "$PHOTOMODEL_BIN" "${ARGS[@]}" > "$LOG_FILE" 2>&1; then
    END_SECONDS="$(date +%s)"
    DURATION=$((END_SECONDS - START_SECONDS))

    echo "  Success in ${DURATION}s"
    printf "%s\t%s\tsuccess\n" \
      "$SECTION_DIR" \
      "$OUTPUT_FILE" \
      >> "$SUMMARY_FILE"
    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
  else
    END_SECONDS="$(date +%s)"
    DURATION=$((END_SECONDS - START_SECONDS))

    echo "  Failed after ${DURATION}s"
    echo "  Log: $LOG_FILE"
    printf "%s\t%s\tfailed\n" \
      "$SECTION_DIR" \
      "$OUTPUT_FILE" \
      >> "$SUMMARY_FILE"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi

  echo ""
done < "$SECTION_LIST"

echo "Done."
echo ""
echo "Successful: $SUCCESS_COUNT"
echo "Failed:     $FAIL_COUNT"
echo "Skipped:    $SKIP_COUNT"
echo ""
echo "Outputs:    $OUTPUT_DIR"
echo "Logs:       $LOG_DIR"
echo "Summary:    $SUMMARY_FILE"