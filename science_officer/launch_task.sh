#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
VENV_DIR="$PROJECT_DIR/abc"

DEFAULT_RESOLUTION="1280x720"
CAM1="rtsp://192.168.137.200:8554/cam"
CAM2="rtsp://192.168.137.200:8554/cam2"

#CAM1="rtsp://localhost:8554/mystream"
#CAM2="rtsp://localhost:8554/mystream"

CORAL_SCRIPT="$PROJECT_DIR/coral_garden/garden_scale.py"
CORAL_ARGS=(
  "--source-type" "video" "--capture-backend" "ffmpeg" "--resolution" "$DEFAULT_RESOLUTION" "--source" "$CAM2" "--fisheye-correction"
)

CORAL_LOCALHOST_URL="http://localhost:8000"
CORAL_ONSHAPE_URL="https://cad.onshape.com/documents"

ICEBERG_SCRIPT="$PROJECT_DIR/Iceberg/Iceberg_Max_Depth.py"
ICEBERG_ARGS=(
  "--source-type" "video" "--capture-backend" "ffmpeg" "--resolution" "$DEFAULT_RESOLUTION" "--source" "$CAM1" "--fisheye-correction" "--disable-vertical-pole"
)

ICEBERG_SPREADSHEET="$PROJECT_DIR/Iceberg/Iceberg_threat_calc_with_clear.xlsm"
ICEBERG_EXAMPLE_DOC="https://20693798.fs1.hubspotusercontent-na1.net/hubfs/20693798/2026/Supporting%20Documents/Iceberg%20Information%20Examples%20EX%20PN%20RN%20Updated%202_16.pdf"

CRAB_SCRIPT="$PROJECT_DIR/invasive_craba/yolo_detect.py"
CRAB_MODEL="yolo11n_dark"
CRAB_DEFAULT_THRESH="0.75"

CRAB_ARGS=(
  "--source-type" "video" "--capture-backend" "ffmpeg" "--resolution" "$DEFAULT_RESOLUTION" "--source" "$CAM1" "--min-thresh" "$CRAB_DEFAULT_THRESH" "--model-path" "$PROJECT_DIR/invasive_craba/$CRAB_MODEL/$CRAB_MODEL.pt" "--device" "cpu"
)

CRAB_ARGS_MACOS=(
  "--source-type" "video" "--capture-backend" "ffmpeg" "--resolution" "$DEFAULT_RESOLUTION" "--source" "$CAM1" "--min-thresh" "$CRAB_DEFAULT_THRESH" "--model-path" "$PROJECT_DIR/invasive_craba/$CRAB_MODEL/$CRAB_MODEL.mlpackage"
)

CRAB_REPORTING_FORM_URL="https://cbjfq.share.hsforms.com/2rHEWllQ5QO6D7Z4CwVM7IQ"

SCIENCE_OFFICER_GUI_PATH="$PROJECT_DIR/bluestar_officer_frontend/build/bluestar_officer_frontend"
echo $SCIENCE_OFFICER_GUI_PATH

# How long to wait before opening localhost:8000 after starting Coral.
CORAL_BROWSER_DELAY_SECONDS=2


quote() {
  printf "%q" "$1"
}

join_quoted() {
  local result=""
  local arg

  for arg in "$@"; do
    result+=" $(quote "$arg")"
  done

  printf "%s" "$result"
}

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    echo "Missing file:"
    echo "  $path"
    exit 1
  fi
}

require_venv() {
  if [ ! -f "$VENV_DIR/bin/activate" ]; then
    echo "Virtual environment not found:"
    echo "  $VENV_DIR"
    echo
    echo "Expected activate script:"
    echo "  $VENV_DIR/bin/activate"
    exit 1
  fi
}

require_configured() {
  local name="$1"
  local value="$2"

  if [ -z "$value" ] || [[ "$value" == *"REPLACE_ME"* ]]; then
    echo "Configuration needed:"
    echo "  Please set $name in this script."
    exit 1
  fi
}

build_python_command() {
  local script="$1"
  shift

  local args
  args="$(join_quoted "$@")"

  echo "cd $(quote "$PROJECT_DIR") && \
source $(quote "$VENV_DIR/bin/activate") && \
python $(quote "$script")$args;"
}

build_binary_command() {
  local binary="$1"
  shift

  local args
  args="$(join_quoted "$@")"

  echo "cd $(quote "$PROJECT_DIR") && \
source $(quote "$VENV_DIR/bin/activate") && \ 
ulimit -n 4096 && \
$(quote "$binary")$args;"
}


launch_terminal() {
  local title="$1"
  local cmd="$2"

  case "$(uname -s)" in
    Darwin)
      osascript - "$cmd" <<'APPLESCRIPT'
on run argv
  set commandText to item 1 of argv

  tell application "Terminal"
    activate
    do script commandText
  end tell
end run
APPLESCRIPT
      ;;

    Linux)
      if command -v gnome-terminal >/dev/null 2>&1; then
        gnome-terminal --title="$title" -- bash -lc "$cmd"
      else
        echo "Gnome terminal found."
        exit 1
      fi
      ;;

    *)
      echo "Unsupported OS: $(uname -s)"
      exit 1
      ;;
  esac
}

open_default() {
  local target="$1"

  case "$(uname -s)" in
    Darwin)
      open "$target"
      ;;

    Linux)
      if command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$target" >/dev/null 2>&1 &
      else
        echo "xdg-open not found. Cannot open:"
        echo "  $target"
        exit 1
      fi
      ;;

    *)
      echo "Unsupported OS: $(uname -s)"
      exit 1
      ;;
  esac
}

open_calc() {
  local spreadsheet="$1"

  case "$(uname -s)" in
    Darwin)
      if open -a "LibreOffice" "$spreadsheet" >/dev/null 2>&1; then
        return
      fi

      open_default "$spreadsheet"
      ;;

    Linux)
      if command -v libreoffice >/dev/null 2>&1; then
        libreoffice --calc "$spreadsheet" >/dev/null 2>&1 &
      else
        open_default "$spreadsheet"
      fi
      ;;

    *)
      echo "Unsupported OS: $(uname -s)"
      exit 1
      ;;
  esac
}

# -------------------------------------------------------------------
# Task launchers
# -------------------------------------------------------------------

start_coral() {
  require_file "$CORAL_SCRIPT"
  require_file "$SCIENCE_OFFICER_GUI_PATH"
  require_configured "CORAL_ONSHAPE_URL" "$CORAL_ONSHAPE_URL"

  open_default "$CORAL_ONSHAPE_URL"
  sleep 1
  open_default "$CORAL_LOCALHOST_URL"

  launch_terminal \
    "BlueStar Officer Frontend" \
    "$(build_binary_command "$SCIENCE_OFFICER_GUI_PATH")"

  launch_terminal \
    "Coral Garden Measurement" \
    "$(build_python_command "$CORAL_SCRIPT" "${CORAL_ARGS[@]}")"


  if [ $(uname -s) == "Darwin" ]; then
    if open -a "CloudCompare" >/dev/null 2>&1; then
      return
    fi
  fi
}

start_iceberg() {
  require_file "$ICEBERG_SCRIPT"
  require_file "$ICEBERG_SPREADSHEET"

  open_calc "$ICEBERG_SPREADSHEET"
  sleep 1
  open_default "$ICEBERG_EXAMPLE_DOC"
  sleep 1

  launch_terminal \
    "Iceberg Measurement" \
    "$(build_python_command "$ICEBERG_SCRIPT" "${ICEBERG_ARGS[@]}")"
  
  sleep 1
}

start_crab() {
  require_file "$CRAB_SCRIPT"
  require_configured "CRAB_REPORTING_FORM_URL" "$CRAB_REPORTING_FORM_URL"

  open_default "$CRAB_REPORTING_FORM_URL"

  case "$(uname -s)" in
    Darwin)
      launch_terminal "Invasive Crab Detection" "$(build_python_command "$CRAB_SCRIPT" "${CRAB_ARGS_MACOS[@]}")"

    ;;
    
    Linux)
      launch_terminal "Invasive Crab Detection" "$(build_python_command "$CRAB_SCRIPT" "${CRAB_ARGS[@]}")"
    ;;

    esac
}

usage() {
  cat <<EOF
Usage:
  ./launch_task.sh coral
  ./launch_task.sh iceberg
  ./launch_task.sh crab
  ./launch_task.sh coral crab
  ./launch_task.sh all
  ./launch_task.sh list

Available tasks:
  coral     Coral Garden measurement task
  iceberg   Iceberg measurement task
  crab      Invasive crab detection task
  all       Launch all tasks
EOF
}

start_target() {
  local target="$1"

  case "$target" in
    coral)
      start_coral
      ;;

    iceberg)
      start_iceberg
      ;;

    crab)
      start_crab
      ;;

    all)
      start_coral
      start_iceberg
      start_crab
      ;;

    list | help | -h | --help)
      usage
      ;;

    *)
      echo "Unknown task: $target"
      echo
      usage
      exit 1
      ;;
  esac
}

main() {
  require_venv

  if [ "$#" -eq 0 ]; then
    usage
    exit 1
  fi

  for target in "$@"; do
    start_target "$target"
  done
}

main "$@"