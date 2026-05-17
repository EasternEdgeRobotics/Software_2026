#!/usr/bin/env bash
set -euo pipefail

FOLDER="${1:-.}"

MAKE="Eastern Edge"

# Edit these names/positions to match your rig.
CAMERA_1_MODEL="BlueStar Front Camera"
CAMERA_1_POSITION="front camera"

CAMERA_2_MODEL="BlueStar Back Camera"
CAMERA_2_POSITION="back camera"

# If a PNG filename does not contain Camera_1 or Camera_2:
#   skip  = leave it untouched
#   tag   = tag it as unknown camera
UNKNOWN_CAMERA_POLICY="skip"

UNKNOWN_MODEL="IMX708 Underwater Unknown Camera"
UNKNOWN_POSITION="unknown position"

LENS_MODEL="Fixed-focus 2.75mm F2.2 lens"

# Physical lens is 2.75mm.
PHYSICAL_FOCAL_LENGTH_MM="2.75"

# Approx flat-port underwater correction.
# If this is not underwater behind a flat port, set this to 1.0.
WATER_REFRACTION_FACTOR="1.333"

FOCAL_LENGTH_MM="$(
  awk -v f="$PHYSICAL_FOCAL_LENGTH_MM" \
    -v r="$WATER_REFRACTION_FACTOR" \
    'BEGIN { printf "%.3f", f * r }'
)"

F_NUMBER="2.2"

# IMX708 full sensor 4608x2592, 1.4um pixels
# full-FOV downscale to 1280x720
# then crop further
#
# Full sensor width = 4608 * 0.0014mm = 6.4512mm
# 720p downscale width = 1280px
# effective pixel size = 6.4512 / 1280 = 0.00504mm/px
EFFECTIVE_PIXEL_SIZE_MM="0.00504"

# pixels per inch = 25.4 / 0.00504
FOCAL_PLANE_RESOLUTION="5039.683"

if ! command -v exiftool >/dev/null 2>&1; then
  echo "Error: exiftool is not installed."
  echo "Install with: brew install exiftool"
  exit 1
fi

if [ ! -d "$FOLDER" ]; then
  echo "Error: folder does not exist: $FOLDER"
  exit 1
fi

FOUND=0
TAGGED=0
SKIPPED=0

while IFS= read -r -d '' image; do
  FOUND=$((FOUND + 1))

  filename="$(basename "$image")"

  CAMERA_ID=""

  if [[ "$filename" =~ Camera_([0-9]+) ]]; then
    CAMERA_ID="${BASH_REMATCH[1]}"
  fi

  case "$CAMERA_ID" in
    1)
      CAMERA_MODEL="$CAMERA_1_MODEL"
      CAMERA_POSITION="$CAMERA_1_POSITION"
      ;;
    2)
      CAMERA_MODEL="$CAMERA_2_MODEL"
      CAMERA_POSITION="$CAMERA_2_POSITION"
      ;;
    *)
      if [ "$UNKNOWN_CAMERA_POLICY" = "skip" ]; then
        echo "Skipping unknown camera filename: $image"
        SKIPPED=$((SKIPPED + 1))
        continue
      fi

      CAMERA_MODEL="$UNKNOWN_MODEL"
      CAMERA_POSITION="$UNKNOWN_POSITION"
      ;;
  esac

  WIDTH="$(exiftool -s3 -ImageWidth "$image" | tr -d '\r')"
  HEIGHT="$(exiftool -s3 -ImageHeight "$image" | tr -d '\r')"

  if [ -z "$WIDTH" ] || [ -z "$HEIGHT" ]; then
    echo "Skipping unreadable image dimensions: $image"
    SKIPPED=$((SKIPPED + 1))
    continue
  fi

  FOCAL_35MM="$(
    awk -v w="$WIDTH" \
      -v h="$HEIGHT" \
      -v f="$FOCAL_LENGTH_MM" \
      -v p="$EFFECTIVE_PIXEL_SIZE_MM" '
        BEGIN {
          full_frame_diag = 43.2666;
          used_diag = sqrt((w * w) + (h * h)) * p;
          equiv = f * full_frame_diag / used_diag;
          printf "%.0f", equiv;
        }
      '
  )"

  # Include crop size in the model string so WebODM does not accidentally group
  # different crop sizes as the same camera calibration.
  MODEL_WITH_MODE="${CAMERA_MODEL} 720p crop ${WIDTH}x${HEIGHT}"

  DESCRIPTION="IMX708 USB camera ${CAMERA_SERIAL}, ${CAMERA_POSITION}; full-FOV downscaled to 1280x720; physical lens ${PHYSICAL_FOCAL_LENGTH_MM}mm F${F_NUMBER}; effective underwater focal length ${FOCAL_LENGTH_MM}mm"

  COMMENT="sensor=IMX708; camera_id=${CAMERA_SERIAL}; camera_position=${CAMERA_POSITION}; environment=underwater; physical_focal_length=${PHYSICAL_FOCAL_LENGTH_MM}mm; effective_focal_length=${FOCAL_LENGTH_MM}mm; refraction_factor=${WATER_REFRACTION_FACTOR}; native_active_area=4608x2592; native_pixel_size=1.4um; pipeline=full_fov_1280x720; final_size=${WIDTH}x${HEIGHT}; effective_pixel_size=5.04um; shutter=rolling; focus=fixed; focus_range=1.5m-infinity; ir_cut=true"

  echo "Tagging: $image"
  echo "  model: ${MODEL_WITH_MODE}"
  echo "  position: ${CAMERA_POSITION}"
  echo "  size: ${WIDTH}x${HEIGHT}"
  echo "  focal length: ${FOCAL_LENGTH_MM}mm underwater effective"
  echo "  35mm equivalent: ${FOCAL_35MM}mm"

  exiftool -overwrite_original \
    -EXIF:Make="$MAKE" \
    -EXIF:Model="$MODEL_WITH_MODE" \
    -EXIF:LensModel="$LENS_MODEL" \
    -EXIF:FNumber="$F_NUMBER" \
    -EXIF:FocalLength="${FOCAL_LENGTH_MM} mm" \
    -EXIF:FocalLengthIn35mmFormat="$FOCAL_35MM" \
    -EXIF:FocalPlaneXResolution="$FOCAL_PLANE_RESOLUTION" \
    -EXIF:FocalPlaneYResolution="$FOCAL_PLANE_RESOLUTION" \
    -EXIF:FocalPlaneResolutionUnit="inches" \
    -EXIF:SensingMethod="One-chip color area" \
    -EXIF:ColorSpace="sRGB" \
    -EXIF:ImageDescription="$DESCRIPTION" \
    -EXIF:UserComment="$COMMENT" \
    "$image"

  TAGGED=$((TAGGED + 1))
done < <(
  find "$FOLDER" -maxdepth 1 -type f -iname "*.png" -print0
)

echo
echo "Done."
echo "Found:   $FOUND PNG file(s)"
echo "Tagged:  $TAGGED PNG file(s)"
echo "Skipped: $SKIPPED PNG file(s)"