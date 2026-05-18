import json
import os
import time
from pathlib import Path
import datetime

import requests

WEBODM_URL = os.getenv("WEBODM_URL", "http://192.168.2.48:8000").rstrip("/")
USERNAME = os.getenv("WEBODM_USERNAME", "peyton")
PASSWORD = os.getenv("WEBODM_PASSWORD", "easternedge")
IMAGE_DIR = Path(os.getenv("IMAGE_DIR", "/home/peyton/test-set"))

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

project_response = session.post(
    f"{WEBODM_URL}/api/projects/",
    data={
        "name": f"Coral Garden {datetime.datetime.now()}"    
        },
)
project_response.raise_for_status()

project_id = project_response.json()["id"]

options = [
    {
        "name": "auto-boundary",
        "value": True
    },
    {
        "name": "feature-quality",
        "value": "ultra"
    },
    {
        "name": "pc-quality",
        "value": "ultra"
    },
    {
        "name": "use-3dmesh",
        "value": True
    },
    {
        "name": "mesh-size",
        "value": 300000
    },
    {
        "name": "mesh-octree-depth",
        "value": 12
    }
]

files = []
open_files = []

try:
    for image_path in image_paths:
        file_handle = image_path.open("rb")
        open_files.append(file_handle)

        files.append(
            (
                "images",
                (
                    image_path.name,
                    file_handle,
                    "image/jpeg",
                ),
            )
        )

    task_response = session.post(
        f"{WEBODM_URL}/api/projects/{project_id}/tasks/",
        files=files,
        data={
            "name": f"Coral Garden {datetime.datetime.now()}",
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