#!/usr/bin/env python3
"""Verify that maintained public Infiltrator repositories adopt the canonical philosophy."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request

ORG = "Infiltrator-Projects"
CANONICAL_REPO = "Libraries"
PHILOSOPHY_PATH = "docs/PHILOSOPHY.md"
CANONICAL_URL = (
    "https://github.com/Infiltrator-Projects/Libraries/"
    f"blob/main/{PHILOSOPHY_PATH}"
)


def request_text(url: str) -> str:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "infiltrator-philosophy-check",
    }
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read().decode("utf-8")


def repositories() -> list[dict[str, object]]:
    repos: list[dict[str, object]] = []
    page = 1
    while True:
        url = (
            f"https://api.github.com/orgs/{ORG}/repos"
            f"?type=public&per_page=100&page={page}"
        )
        batch = json.loads(request_text(url))
        if not batch:
            break
        repos.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    return repos


def main() -> int:
    failures: list[str] = []
    checked = 0

    for repo in repositories():
        if bool(repo.get("archived")):
            continue

        name = str(repo["name"])
        branch = str(repo.get("default_branch") or "main")
        raw_url = (
            f"https://raw.githubusercontent.com/{ORG}/{name}/{branch}/{PHILOSOPHY_PATH}"
        )

        try:
            content = request_text(raw_url)
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError) as exc:
            failures.append(f"{name}: missing/unreadable {PHILOSOPHY_PATH} ({exc})")
            continue

        checked += 1
        if name == CANONICAL_REPO:
            if "# Infiltrator Engineering Philosophy" not in content.splitlines():
                failures.append(
                    f"{name}: {PHILOSOPHY_PATH} is not the canonical philosophy document"
                )
        elif CANONICAL_URL not in content:
            failures.append(f"{name}: {PHILOSOPHY_PATH} does not point to canonical philosophy")

    if failures:
        print("Infiltrator philosophy adoption check FAILED:")
        for failure in failures:
            print(f" - {failure}")
        return 1

    print(f"Infiltrator philosophy adoption check passed for {checked} repositories.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
