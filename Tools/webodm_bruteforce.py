import datetime
import itertools
import json
import mimetypes
import os
import time
from pathlib import Path

import requests

WEBODM_URL = os.getenv("WEBODM_URL", "http://192.168.2.48:8000").rstrip("/")
USERNAME = os.getenv("WEBODM_USERNAME", "peyton")
PASSWORD = os.getenv("WEBODM_PASSWORD", "easternedge")
IMAGE_DIR = Path(os.getenv("IMAGE_DIR", "/Users/peyton/exif-underw/"))

PROJECT_NAME_PREFIX = os.getenv("PROJECT_NAME_PREFIX", "Coral Garden Brute Force")
TASK_NAME_PREFIX = os.getenv("TASK_NAME_PREFIX", "Coral Garden")

CREATE_PROJECT = True
EXISTING_PROJECT_ID = os.getenv("WEBODM_PROJECT_ID")

SUBMIT_DELAY_SECONDS = float(os.getenv("SUBMIT_DELAY_SECONDS", "0.1"))
DRY_RUN = os.getenv("DRY_RUN", "false").lower() == "true"
MANIFEST_PATH = Path(os.getenv("MANIFEST_PATH", "webodm_bruteforce_manifest.jsonl"))



OPTION_GRID = {
    "auto-boundary": [True],
    "skip-orthophoto": [True],
    "texturing-keep-unseen-faces": [True],
    "use-exif": [True],
    "rolling-shutter": [True],
    "feature-quality": ["ultra"],
    "pc-quality": ["ultra"],
    "use-3dmesh": [True],
    "mesh-size": [
        300000,
        500000,
    ],
    "mesh-octree-depth": [
        14,
    ],
    "matcher-type": [
        "bruteforce",
        "flann",
        "bow",
    ],
    "matcher-order": [
        50,
        100,
    ],
    "min-num-features": [
        10000,
        15000,
        25000,
    ],
    "feature-type": [
        "hahog",
        "orb",
        "sift",
    ],
}



def get_image_paths(image_dir: Path) -> list[Path]:
    image_paths = sorted(
        path
        for path in image_dir.iterdir()
        if path.suffix.lower() in {".jpg", ".jpeg", ".png", ".tif", ".tiff"}
    )

    if len(image_paths) < 2:
        raise SystemExit("WebODM tasks require at least 2 images.")

    return image_paths


def authenticate() -> requests.Session:
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

    return session


def create_project(session: requests.Session) -> int:
    now = datetime.datetime.now().isoformat(timespec="seconds")

    project_response = session.post(
        f"{WEBODM_URL}/api/projects/",
        data={
            "name": f"{PROJECT_NAME_PREFIX} {now}",
        },
    )
    project_response.raise_for_status()

    return project_response.json()["id"]


def build_option_combinations(
    option_grid: dict[str, list],
) -> list[list[dict[str, object]]]:
    option_names = list(option_grid.keys())
    option_values = [option_grid[name] for name in option_names]

    combinations = []

    for values in itertools.product(*option_values):
        options = [
            {
                "name": name,
                "value": value,
            }
            for name, value in zip(option_names, values)
        ]

        combinations.append(options)

    return combinations


def options_to_short_name(options: list[dict[str, object]]) -> str:
    parts = []

    for option in options:
        name = option["name"]
        value = option["value"]

        short_name = (
            str(name)
            .replace("feature-quality", "fq")
            .replace("pc-quality", "pcq")
            .replace("auto-boundary", "ab")
            .replace("use-3dmesh", "3dm")
            .replace("mesh-size", "ms")
            .replace("mesh-octree-depth", "mod")
        )

        parts.append(f"{short_name}={value}")

    return ", ".join(parts)


def submit_task(
    session: requests.Session,
    project_id: int,
    image_paths: list[Path],
    options: list[dict[str, object]],
    combination_index: int,
    total_combinations: int,
) -> dict:
    now = datetime.datetime.now().isoformat(timespec="seconds")
    option_summary = options_to_short_name(options)

    task_name = (
        f"{TASK_NAME_PREFIX} combo {combination_index:04d} of "
        f"{total_combinations:04d} - {now}"
    )

    files = []
    open_files = []

    try:
        for image_path in image_paths:
            file_handle = image_path.open("rb")
            open_files.append(file_handle)

            mime_type = mimetypes.guess_type(image_path.name)[0]
            if mime_type is None:
                mime_type = "application/octet-stream"

            files.append(
                (
                    "images",
                    (
                        image_path.name,
                        file_handle,
                        mime_type,
                    ),
                )
            )

        print()
        print(f"Submitting combo {combination_index}/{total_combinations}")
        print(f"Task name: {task_name}")
        print(f"Options: {option_summary}")

        if DRY_RUN:
            return {
                "id": None,
                "dry_run": True,
                "name": task_name,
                "options": options,
            }

        task_response = session.post(
            f"{WEBODM_URL}/api/projects/{project_id}/tasks/",
            files=files,
            data={
                "name": task_name,
                "options": json.dumps(options),
                "auto_processing_node": "true",
            },
        )
        task_response.raise_for_status()

        return task_response.json()

    finally:
        for file_handle in open_files:
            file_handle.close()


def append_manifest_record(record: dict) -> None:
    with MANIFEST_PATH.open("a", encoding="utf-8") as file:
        file.write(json.dumps(record, sort_keys=True) + "\n")


def main() -> None:
    image_paths = get_image_paths(IMAGE_DIR)
    option_combinations = build_option_combinations(OPTION_GRID)

    total_combinations = len(option_combinations)

    print(f"Found {len(image_paths)} images in {IMAGE_DIR}")
    print(f"Generated {total_combinations} WebODM option combinations")

    if total_combinations == 0:
        raise SystemExit("No option combinations generated.")

    if DRY_RUN:
        print("Running in DRY_RUN mode. No tasks will be submitted.")

    session = authenticate()

    if CREATE_PROJECT:
        project_id = create_project(session)
    else:
        if not EXISTING_PROJECT_ID:
            raise SystemExit(
                "CREATE_PROJECT is False but WEBODM_PROJECT_ID was not provided."
            )

        project_id = int(EXISTING_PROJECT_ID)

    print(f"Using project ID: {project_id}")
    print(f"Writing manifest to: {MANIFEST_PATH}")

    for index, options in enumerate(option_combinations, start=1):
        task = submit_task(
            session=session,
            project_id=project_id,
            image_paths=image_paths,
            options=options,
            combination_index=index,
            total_combinations=total_combinations,
        )

        record = {
            "submitted_at": datetime.datetime.now().isoformat(timespec="seconds"),
            "project_id": project_id,
            "task_id": task.get("id"),
            "task_name": task.get("name"),
            "combination_index": index,
            "total_combinations": total_combinations,
            "options": options,
            "response": task,
        }

        append_manifest_record(record)

        print(f"Created task: {task.get('id')}")

        if index < total_combinations and SUBMIT_DELAY_SECONDS > 0:
            time.sleep(SUBMIT_DELAY_SECONDS)

    print()
    print("Done.")
    print(f"Submitted {total_combinations} combinations.")
    print(f"Manifest saved to {MANIFEST_PATH}")


if __name__ == "__main__":
    main()