import argparse
import datetime
import json
import mimetypes
import os
from pathlib import Path

import requests


def parse_args():
    parser = argparse.ArgumentParser(description="Upload images from BlueStar to WebODM")

    parser.add_argument(
        "--image-dir",
        default=os.getenv("IMAGE_DIR", "/home/peyton/test-set"),
        help="Directory containing images to upload",
    )

    parser.add_argument(
        "--section",
        default=os.getenv("PHOTO_SECTION", ""),
        help="Photo section name, such as section_0",
    )

    return parser.parse_args()


args = parse_args()

WEBODM_URL = os.getenv("WEBODM_URL", "http://127.0.0.1:8000").rstrip("/")
USERNAME = os.getenv("WEBODM_USERNAME", "peyton")
PASSWORD = os.getenv("WEBODM_PASSWORD", "easternedge")

IMAGE_DIR = Path(args.image_dir)
SECTION = args.section or IMAGE_DIR.name

if not IMAGE_DIR.exists():
    raise SystemExit(f"Image directory does not exist: {IMAGE_DIR}")

image_paths = sorted(
    path
    for path in IMAGE_DIR.iterdir()
    if path.suffix.lower() in {".jpg", ".jpeg", ".png", ".tif", ".tiff"}
)

if len(image_paths) < 2:
    raise SystemExit("WebODM tasks require at least 2 images.")

session = requests.Session()

auth_response = session.post(
    f"{WEBODM_URL}/api/token-auth/",
    data={
        "username": USERNAME,
        "password": PASSWORD,
    },
)
auth_response.raise_for_status()

token = auth_response.json()["token"]
session.headers.update({"Authorization": f"JWT {token}"})

timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
project_name = f"Coral Garden {SECTION} {timestamp}"

project_response = session.post(
    f"{WEBODM_URL}/api/projects/",
    data={
        "name": project_name,
    },
)
project_response.raise_for_status()

project_id = project_response.json()["id"]

options = [
    {
        "name": "auto-boundary",
        "value": True,
    },
    {
        "name": "feature-quality",
        "value": "ultra",
    },
    {
        "name": "pc-quality",
        "value": "ultra",
    },
    {
        "name": "bg-removal",
        "value": True,
    },
    {
        "name": "use-3dmesh",
        "value": True,
    },
    {
        "name": "mesh-size",
        "value": 300000,
    },
    {
        "name": "mesh-octree-depth",
        "value": 12,
    },
    {
        "name": "matcher-order",
        "value": 120,
    },
    {
        "name": "matcher-type",
        "value": "bruteforce",
    },
    {
        "name": "min-num-features",
        "value": 20000,
    },
]

files = []
open_files = []

try:
    for image_path in image_paths:
        file_handle = image_path.open("rb")
        open_files.append(file_handle)

        content_type = mimetypes.guess_type(image_path.name)[0]
        if content_type is None:
            content_type = "application/octet-stream"

        files.append(
            (
                "images",
                (
                    image_path.name,
                    file_handle,
                    content_type,
                ),
            )
        )

    task_response = session.post(
        f"{WEBODM_URL}/api/projects/{project_id}/tasks/",
        files=files,
        data={
            "name": f"Coral Garden {SECTION} {timestamp}",
            "options": json.dumps(options),
            "auto_processing_node": "true",
        },
    )
    task_response.raise_for_status()

finally:
    for file_handle in open_files:
        file_handle.close()

task = task_response.json()
task_id = task["id"]

print(f"Created task {task_id} in project {project_id}")
print(f"Uploaded section: {SECTION}")
print(f"Image directory: {IMAGE_DIR}")