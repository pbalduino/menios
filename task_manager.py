#!/usr/bin/env python3
"""
Utility script for managing meniOS task metadata.

Supports listing and locally closing tasks, plus regenerating `tasks.json`
directly from GitHub issues when credentials are available.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, List
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

TASKS_FILE = Path("tasks.json")


def load_tasks() -> List[Dict[str, Any]]:
    if not TASKS_FILE.exists():
        raise SystemExit(f"{TASKS_FILE} not found")
    with TASKS_FILE.open() as fh:
        return json.load(fh)


def save_tasks(tasks: List[Dict[str, Any]]) -> None:
    with TASKS_FILE.open("w") as fh:
        json.dump(tasks, fh, indent=2)
        fh.write("\n")


def fetch_github_issues(owner: str, repo: str, include_closed: bool = False, token: str | None = None) -> List[Dict[str, Any]]:
    issues: List[Dict[str, Any]] = []
    headers = {"Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"

    params: Dict[str, Any] = {"state": "all" if include_closed else "open", "per_page": 100, "page": 1}

    while True:
        query = urlencode(params)
        url = f"https://api.github.com/repos/{owner}/{repo}/issues?{query}"

        try:
            with urlopen(Request(url, headers=headers)) as response:
                payload = json.load(response)
        except HTTPError as exc:
            raise SystemExit(f"GitHub API request failed ({exc.code}): {exc.reason}") from exc
        except URLError as exc:
            raise SystemExit(f"GitHub API request failed: {exc.reason}") from exc

        if not payload:
            break

        for item in payload:
            if "pull_request" in item:
                continue
            issues.append(item)

        params["page"] += 1

    return issues


def issues_to_tasks(issues: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    tasks: List[Dict[str, Any]] = []
    for issue in issues:
        task = {
            "title": issue.get("title", "<no title>"),
            "github_issue": {
                "number": issue.get("number"),
                "url": issue.get("html_url"),
                "state": issue.get("state", "open").upper(),
            },
            "labels": [label.get("name") for label in issue.get("labels", []) if label.get("name")],
            "assignees": [assignee.get("login") for assignee in issue.get("assignees", []) if assignee.get("login")],
        }
        tasks.append(task)

    tasks.sort(key=lambda item: item["github_issue"]["number"], reverse=True)
    return tasks


def sync_tasks(owner: str, repo: str, include_closed: bool = False, token_env: str | None = "GITHUB_TOKEN") -> None:
    token = os.environ.get(token_env) if token_env else None
    issues = fetch_github_issues(owner, repo, include_closed, token)
    tasks = issues_to_tasks(issues)
    save_tasks(tasks)
    scope = "including" if include_closed else "excluding"
    print(f"Wrote {len(tasks)} tasks to {TASKS_FILE} ({scope} closed issues)")


def list_tasks(state: str | None = None) -> None:
    tasks = load_tasks()
    for task in tasks:
        issue = task.get("github_issue", {})
        status = issue.get("state", "UNKNOWN")
        if state and status.upper() != state.upper():
            continue
        title = task.get("title", "<no title>")
        number = issue.get("number")
        url = issue.get("url", "")
        labels = ", ".join(task.get("labels", []))
        assignees = ", ".join(task.get("assignees", []))
        print(f"#{number} [{status}] {title}\n  Labels: {labels}\n  Assignees: {assignees}\n  {url}\n")


def mark_closed(numbers: List[int]) -> None:
    if not numbers:
        return
    tasks = load_tasks()
    to_close = set(numbers)
    updated = False
    for task in tasks:
        issue = task.get("github_issue", {})
        num = issue.get("number")
        if num in to_close and issue.get("state") != "CLOSED":
            issue["state"] = "CLOSED"
            updated = True
            print(f"Marked issue #{num} as CLOSED in {TASKS_FILE}")
    if updated:
        save_tasks(tasks)
    else:
        print("No matching open tasks found.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Manage tasks.json metadata.")
    sub = parser.add_subparsers(dest="command", required=True)

    ls = sub.add_parser("list", help="List tasks")
    ls.add_argument("--state", choices=["OPEN", "CLOSED"], help="Filter by state")

    close = sub.add_parser("close", help="Mark tasks closed locally")
    close.add_argument("numbers", nargs="+", type=int, help="Issue numbers to mark closed")

    sync = sub.add_parser("sync", help="Regenerate tasks.json from GitHub issues")
    sync.add_argument("--owner", required=True, help="GitHub repository owner")
    sync.add_argument("--repo", required=True, help="GitHub repository name")
    sync.add_argument("--include-closed", action="store_true", help="Include closed issues")
    sync.add_argument(
        "--token-env",
        default="GITHUB_TOKEN",
        help="Environment variable with a GitHub token (empty string to skip)",
    )

    args = parser.parse_args()
    if args.command == "list":
        list_tasks(args.state)
    elif args.command == "close":
        mark_closed(args.numbers)
    elif args.command == "sync":
        token_env = args.token_env if args.token_env else None
        sync_tasks(args.owner, args.repo, args.include_closed, token_env)


if __name__ == "__main__":
    main()
