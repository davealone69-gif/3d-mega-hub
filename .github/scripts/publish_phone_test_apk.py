#!/usr/bin/env python3
"""Publishes the verified phone-test debug APK of one commit as a direct download.

The debug APK for the phone test is built, inspected and tested by the workflow
"AvatarForge diagnostic APK and tests". That workflow keeps it as a workflow artifact, which can
only be downloaded by a signed-in GitHub account and arrives as a zip. This script takes the same
file and attaches it to a GitHub pre-release, where it is a plain .apk that downloads without
signing in.

It publishes nothing unless the owner's readiness gate (BUILD_AND_VERIFY.md) holds for the commit:

  * the latest finished run of each of the four workflows for that commit succeeded;
  * the diagnostic run and the Android CI run were started on the branch, so they built the
    commit itself and not GitHub's merge of a pull request, and none of their jobs was skipped;
  * the diagnostic run's own download check recorded chain_complete=true for this commit and
    this run, with the production render test at PIXELS_VERIFIED;
  * the APK taken from the artifact has the SHA-256 that record names, and the SHA-256 the
    caller expects, when one is given;
  * the commit is the head of the branch the workflow was started on, or the head differs from
    it only by documents and by this publishing workflow. Any other difference means the APK no
    longer represents the branch, and the four workflows have to run on the head first.

After publishing, the file is downloaded again from the release without credentials and compared.
A release whose download differs is deleted and the job fails.

    publish_phone_test_apk.py --commit <sha> [--expected-sha256 <sha256>] [--run <run id>]
                              [--output <dir>] [--dry-run]
    publish_phone_test_apk.py --self-test

Needs the GitHub CLI with a token that can read Actions and write repository contents.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

APK_ARTIFACT = "3D-Mega-Hub-generated-avatar-diagnostic-debug-apk"
APK_FILE = "3D-Mega-Hub-generated-avatar-diagnostic-debug.apk"
VERIFICATION_ARTIFACT = "3D-Mega-Hub-debug-apk-download-verification"
BUILD_JOB = "Build debug APK and verify generated avatar pixels"
VERIFY_JOB = "Download the published debug APK and verify it"

DIAGNOSTIC = ".github/workflows/avatarforge-diagnostic-apk.yml"
ANDROID_CI = ".github/workflows/android.yml"
RUNTIME = ".github/workflows/avatarforge-generated-runtime.yml"
RELEASE_PREVIEW = ".github/workflows/release-preview.yml"

# (workflow file, name used in messages, must the evidence run have built the commit itself)
REQUIRED_WORKFLOWS = [
    (DIAGNOSTIC, "AvatarForge diagnostic APK and tests", True),
    (ANDROID_CI, "Android CI", True),
    (RUNTIME, "AvatarForge generated-avatar runtime verification", False),
    (RELEASE_PREVIEW, "Release preview and Android emulator smoke", False),
]
# A run started by a pull request builds GitHub's merge commit; these events build the commit.
BUILDS_THE_COMMIT = ("workflow_dispatch", "push")
IGNORED_CONCLUSIONS = ("cancelled", "skipped")

# Paths that may differ between the APK's commit and the branch head without making the APK stale:
# documents, and the files that publish. Nothing here is compiled, packaged or run by a gate.
PUBLISHING_FILES = (
    ".github/workflows/publish-phone-test-apk.yml",
    ".github/scripts/publish_phone_test_apk.py",
)


def leaves_the_apk_current(path: str) -> bool:
    return (path in PUBLISHING_FILES
            or re.fullmatch(r"[^/]+\.md", path) is not None
            or re.fullmatch(r"docs/.+\.md", path) is not None)


class Refused(Exception):
    """The gate does not hold, or the evidence is incomplete. Nothing is published."""


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def read_key_values(path: pathlib.Path) -> dict:
    values = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values.setdefault(key.strip(), value.strip())
    return values


def find_one(root: pathlib.Path, name: str, what: str) -> pathlib.Path:
    matches = sorted(p for p in root.rglob(name) if p.is_file())
    if len(matches) != 1:
        raise Refused("expected exactly one %s (%s), found %d" % (what, name, len(matches)))
    return matches[0]


class GitHub:
    """The calls this script makes, through the GitHub CLI. The self-test replaces this class."""

    def __init__(self, repo: str):
        self.repo = repo

    def _gh(self, *args: str, check: bool = True) -> subprocess.CompletedProcess:
        result = subprocess.run(["gh", *args], capture_output=True, text=True)
        if check and result.returncode != 0:
            raise RuntimeError("gh %s failed: %s" % (" ".join(args), result.stderr.strip()))
        return result

    def _api(self, path: str):
        return json.loads(self._gh("api", path).stdout)

    def resolve_commit(self, commit: str) -> str:
        return self._api("repos/%s/commits/%s" % (self.repo, commit))["sha"]

    def branch_head(self) -> str:
        """The commit of the checkout this script runs in: the head of the branch it was started on."""
        return subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True,
                              check=True).stdout.strip()

    def changes_between(self, commit: str, head: str):
        """Paths that differ between the commit and the head; None if the head does not contain it."""
        contained = subprocess.run(["git", "merge-base", "--is-ancestor", commit, head],
                                   capture_output=True, text=True)
        if contained.returncode != 0:
            return None
        listing = subprocess.run(["git", "diff", "--name-only", "--no-renames", commit, head],
                                 capture_output=True, text=True, check=True).stdout
        return [line for line in listing.splitlines() if line]

    def runs_for_commit(self, commit: str) -> list:
        path = "repos/%s/actions/runs?head_sha=%s&per_page=100" % (self.repo, commit)
        return self._api(path)["workflow_runs"]

    def jobs_of_run(self, run_id: int) -> list:
        path = "repos/%s/actions/runs/%d/jobs?per_page=100&filter=latest" % (self.repo, run_id)
        return self._api(path)["jobs"]

    def download_artifact(self, run_id: int, name: str, destination: pathlib.Path) -> None:
        destination.mkdir(parents=True, exist_ok=True)
        self._gh("run", "download", str(run_id), "--repo", self.repo,
                 "--name", name, "--dir", str(destination))

    def release_by_tag(self, tag: str):
        result = self._gh("api", "repos/%s/releases/tags/%s" % (self.repo, tag), check=False)
        if result.returncode == 0:
            return json.loads(result.stdout)
        if "HTTP 404" in result.stderr:
            return None
        raise RuntimeError("release lookup for %s failed: %s" % (tag, result.stderr.strip()))

    def create_release(self, tag: str, commit: str, title: str,
                       notes: pathlib.Path, asset: pathlib.Path) -> None:
        self._gh("release", "create", tag, str(asset), "--repo", self.repo, "--target", commit,
                 "--title", title, "--notes-file", str(notes), "--prerelease")

    def delete_release(self, tag: str) -> None:
        self._gh("release", "delete", tag, "--repo", self.repo, "--cleanup-tag", "--yes",
                 check=False)

    def anonymous_download(self, url: str, destination: pathlib.Path) -> None:
        """Fetches the URL the way a phone browser does: no token, redirects followed."""
        request = urllib.request.Request(url, headers={"User-Agent": "phone-test-apk-check"})
        last_error = None
        for attempt in range(5):
            try:
                with urllib.request.urlopen(request, timeout=120) as response, \
                        open(destination, "wb") as handle:
                    shutil.copyfileobj(response, handle)
                return
            except Exception as error:  # the asset can take a moment to become available
                last_error = error
                time.sleep(5 * (attempt + 1))
        raise RuntimeError("anonymous download of %s failed: %s" % (url, last_error))


def select_evidence_runs(runs: list, commit: str, pinned_run: int | None) -> dict:
    """Returns the evidence run of each required workflow, or refuses."""
    by_workflow: dict = {}
    for run in runs:
        if run.get("head_sha") == commit:
            by_workflow.setdefault(run.get("path"), []).append(run)

    def order(run):
        return (run.get("created_at") or "", run.get("id") or 0)

    evidence = {}
    for path, title, must_build_commit in REQUIRED_WORKFLOWS:
        candidates = by_workflow.get(path, [])
        for run in candidates:
            if run.get("status") != "completed":
                raise Refused("run %s of \"%s\" is still %s for this commit; wait for it to finish"
                              % (run.get("id"), title, run.get("status")))
        finished = [r for r in candidates if r.get("conclusion") not in IGNORED_CONCLUSIONS]
        if not finished:
            raise Refused("\"%s\" has no finished run for commit %s" % (title, commit))
        latest = max(finished, key=order)
        if latest.get("conclusion") != "success":
            raise Refused("the latest run of \"%s\" for this commit, %s, ended with %s"
                          % (title, latest.get("id"), latest.get("conclusion")))
        chosen = latest
        if must_build_commit:
            built_here = [r for r in finished if r.get("conclusion") == "success"
                          and r.get("event") in BUILDS_THE_COMMIT]
            if path == DIAGNOSTIC and pinned_run is not None:
                built_here = [r for r in built_here if r.get("id") == pinned_run]
            if not built_here:
                raise Refused("\"%s\" has no successful run that was started on the branch for "
                              "commit %s%s. A run started by a pull request builds GitHub's merge "
                              "commit, not this commit" % (
                                  title, commit,
                                  "" if pinned_run is None or path != DIAGNOSTIC
                                  else " with run id %d" % pinned_run))
            chosen = max(built_here, key=order)
        evidence[path] = chosen
    return evidence


def require_all_jobs_passed(github, run: dict, title: str, named: tuple = ()) -> None:
    jobs = github.jobs_of_run(run["id"])
    if not jobs:
        raise Refused("run %s of \"%s\" lists no jobs" % (run["id"], title))
    for job in jobs:
        if job.get("conclusion") != "success":
            raise Refused("job \"%s\" of run %s (\"%s\") ended with %s, not success"
                          % (job.get("name"), run["id"], title, job.get("conclusion")))
    names = {job.get("name") for job in jobs}
    for name in named:
        if name not in names:
            raise Refused("run %s of \"%s\" has no job \"%s\"; it predates the download check"
                          % (run["id"], title, name))


def number(text: str) -> str:
    return "{:,}".format(int(text)) if re.fullmatch(r"\d+", text or "") else (text or "?")


def release_notes(record: dict, repo: str) -> str:
    runs = "https://github.com/%s/actions/runs" % repo
    gate = "https://github.com/%s/blob/%s/BUILD_AND_VERIFY.md#phone-gate-samsung-sm-s921b" % (
        repo, record["commit"])
    return "\n".join([
        "Debug build for the owner's test on the Samsung SM-S921B. It is not a signed store "
        "release.",
        "",
        "Tap **%s** under Assets to download it. Uninstall any earlier 3D Mega Hub build first: "
        "each CI debug build is signed with its own throwaway key, so Android will not install "
        "it over another build. Then follow the [phone gate](%s)." % (record["asset"], gate),
        "",
        "| | |",
        "|---|---|",
        "| Source commit | `%s` |" % record["commit"],
        "| APK SHA-256 | `%s` |" % record["sha256"],
        "| Size | %s bytes |" % number(record["bytes"]),
        "| App | `%s` %s (%s), minSdk %s, targetSdk %s, ABIs %s |" % (
            record["applicationId"], record["versionName"], record["versionCode"],
            record["minSdk"], record["targetSdk"], record["abis"]),
        "| Signing certificate SHA-256 | `%s` (debug key created on the CI machine for one job) |"
        % record["certificate_sha256"],
        "| Built, inspected and tested by | [run %s](%s/%s) |" % (
            record["diagnostic_run"], runs, record["diagnostic_run"]),
        "| Emulator render test | %s, %s geometry pixels (minimum %s), on %s |" % (
            record["render_test_status"], number(record["render_test_visible_pixels"]),
            number(record["render_test_minimum_visible_pixels"]), record["render_test_device"]),
        "| Other gates on this commit | [Android CI](%s/%s), [generated-avatar runtime](%s/%s), "
        "[release preview](%s/%s) |" % (runs, record["android_ci_run"], runs,
                                        record["runtime_run"], runs,
                                        record["release_preview_run"]),
        "| Branch head when published | %s |" % (
            "the source commit itself" if record["branch_head"] == record["commit"]
            else "`%s`. It differs from the source commit only in documents and the publishing "
                 "workflow (%s), so no file that is compiled, packaged or tested changed"
                 % (record["branch_head"], record["branch_head_differs_by"])),
        "",
        "This is the file from the artifact of that run. It was taken from there, checked "
        "against the recorded SHA-256, attached here, and downloaded again from this page "
        "without signing in.",
        "",
        "Verified on an Android emulator only. Rendering, performance and lifecycle on the "
        "phone are **UNVERIFIED** until the owner's test.",
        "",
    ])


def publish(github, commit_argument: str, expected_sha256: str, pinned_run: int | None,
            output: pathlib.Path, dry_run: bool) -> dict:
    if not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit_argument or ""):
        raise Refused("the commit must be given as 7 to 40 hexadecimal characters, not '%s'"
                      % commit_argument)
    expected_sha256 = (expected_sha256 or "").strip().lower()
    if expected_sha256 and not re.fullmatch(r"[0-9a-f]{64}", expected_sha256):
        raise Refused("the expected APK SHA-256 must be 64 hexadecimal characters")
    commit = github.resolve_commit(commit_argument.lower())
    if not re.fullmatch(r"[0-9a-f]{40}", commit or ""):
        raise Refused("commit '%s' could not be resolved" % commit_argument)
    output.mkdir(parents=True, exist_ok=True)

    # 0. The APK must still represent the branch this was started on.
    head = github.branch_head()
    changed = github.changes_between(commit, head)
    if changed is None:
        raise Refused("commit %s is not part of the branch this workflow was started on (head %s)"
                      % (commit, head))
    stale = [path for path in changed if not leaves_the_apk_current(path)]
    if stale:
        raise Refused("the branch head %s has changes that are not in the APK of %s: %s%s. Run the "
                      "four workflows on the head and publish that commit instead"
                      % (head, commit, ", ".join(stale[:8]),
                         "" if len(stale) <= 8 else " and %d more" % (len(stale) - 8)))

    # 1. The four workflows on this commit.
    evidence = select_evidence_runs(github.runs_for_commit(commit), commit, pinned_run)
    diagnostic = evidence[DIAGNOSTIC]
    require_all_jobs_passed(github, diagnostic, "AvatarForge diagnostic APK and tests",
                            named=(BUILD_JOB, VERIFY_JOB))
    require_all_jobs_passed(github, evidence[ANDROID_CI], "Android CI")

    # 2. The APK and the record of the diagnostic run's own download check.
    apk_directory = output / "artifact"
    verification_directory = output / "verification"
    for directory in (apk_directory, verification_directory):
        if directory.exists():
            shutil.rmtree(directory)
    github.download_artifact(diagnostic["id"], APK_ARTIFACT, apk_directory)
    github.download_artifact(diagnostic["id"], VERIFICATION_ARTIFACT, verification_directory)
    apk = find_one(apk_directory, APK_FILE, "debug APK in the artifact")
    sums = find_one(apk_directory, "SHA256SUMS", "checksum file in the artifact")
    verification = read_key_values(
        find_one(verification_directory, "DOWNLOAD_VERIFICATION.txt", "download-check record"))
    inspection = read_key_values(
        find_one(verification_directory, "APK_INSPECTION.txt", "inspection of the downloaded APK"))

    sha256 = sha256_of(apk)
    size = apk.stat().st_size
    listed = (sums.read_text(encoding="utf-8", errors="replace").split() or [""])[0]
    checks = [
        (verification.get("chain_complete") == "true" and verification.get("failures") == "0",
         "the download check of run %s did not record chain_complete=true with failures=0"
         % diagnostic["id"]),
        (verification.get("source_commit") == commit,
         "the download-check record names commit '%s', not %s"
         % (verification.get("source_commit"), commit)),
        (verification.get("workflow_run") == str(diagnostic["id"]),
         "the download-check record names workflow run '%s', not %s"
         % (verification.get("workflow_run"), diagnostic["id"])),
        (verification.get("render_test_status") == "PIXELS_VERIFIED",
         "the production render test is recorded as '%s', not PIXELS_VERIFIED"
         % verification.get("render_test_status")),
        (verification.get("downloaded_sha256") == sha256,
         "the download check verified APK %s, the artifact now holds %s"
         % (verification.get("downloaded_sha256"), sha256)),
        (inspection.get("sha256") == sha256,
         "the inspection record is for APK %s, the artifact holds %s"
         % (inspection.get("sha256"), sha256)),
        (listed == sha256, "SHA256SUMS names %s, the artifact holds %s" % (listed, sha256)),
        (verification.get("downloaded_bytes") == str(size),
         "the download check recorded %s bytes, the artifact holds %d"
         % (verification.get("downloaded_bytes"), size)),
        (not expected_sha256 or expected_sha256 == sha256,
         "the APK of run %s is %s, not the expected %s" % (diagnostic["id"], sha256,
                                                           expected_sha256)),
    ]
    for holds, message in checks:
        if not holds:
            raise Refused(message)

    tag = "phone-test-%s" % commit[:7]
    asset_name = "3D-Mega-Hub-debug-%s.apk" % commit[:7]
    record = {
        "commit": commit,
        "tag": tag,
        "asset": asset_name,
        "sha256": sha256,
        "bytes": str(size),
        "applicationId": inspection.get("applicationId", "?"),
        "versionName": inspection.get("versionName", "?"),
        "versionCode": inspection.get("versionCode", "?"),
        "minSdk": inspection.get("minSdk", "?"),
        "targetSdk": inspection.get("targetSdk", "?"),
        "abis": inspection.get("abis", "?"),
        "certificate_sha256": verification.get("certificate_sha256", "?"),
        "diagnostic_run": str(diagnostic["id"]),
        "android_ci_run": str(evidence[ANDROID_CI]["id"]),
        "runtime_run": str(evidence[RUNTIME]["id"]),
        "runtime_run_event": str(evidence[RUNTIME].get("event")),
        "release_preview_run": str(evidence[RELEASE_PREVIEW]["id"]),
        "release_preview_run_event": str(evidence[RELEASE_PREVIEW].get("event")),
        "render_test_status": verification.get("render_test_status", "?"),
        "render_test_visible_pixels": verification.get("render_test_visible_pixels", "?"),
        "render_test_minimum_visible_pixels":
            verification.get("render_test_minimum_visible_pixels", "?"),
        "render_test_device": verification.get("render_test_device", "?"),
        "branch_head": head,
        "branch_head_differs_by": ", ".join(changed) if changed else "nothing",
        "gate": "holds",
    }

    # 3. Publish, or confirm what is already published. Never replace a published file.
    def asset_of(release: dict):
        for asset in release.get("assets") or []:
            if asset.get("name") == asset_name:
                return asset
        return None

    def downloaded_matches(url: str) -> bool:
        check = output / "anonymous-download.apk"
        github.anonymous_download(url, check)
        matches = sha256_of(check) == sha256
        check.unlink()
        return matches

    existing = github.release_by_tag(tag)
    if existing is not None:
        asset = asset_of(existing)
        if asset is None:
            raise Refused("release %s exists but does not hold %s; remove it by hand first"
                          % (tag, asset_name))
        if not downloaded_matches(asset["browser_download_url"]):
            raise Refused("release %s already holds a different file under %s; it is not replaced"
                          % (tag, asset_name))
        record.update(action="already published", url=asset["browser_download_url"],
                      page=existing.get("html_url", ""), anonymous_download_matches="true")
    elif dry_run:
        record.update(action="dry run, nothing published", url="", page="",
                      anonymous_download_matches="not checked")
    else:
        staged = output / asset_name
        shutil.copyfile(apk, staged)
        notes = output / "RELEASE_NOTES.md"
        notes.write_text(release_notes(record, github.repo), encoding="utf-8")
        github.create_release(tag, commit, "3D Mega Hub phone-test build %s (debug)" % commit[:7],
                              notes, staged)
        created = github.release_by_tag(tag)
        asset = asset_of(created) if created else None
        if asset is None:
            github.delete_release(tag)
            raise RuntimeError("release %s was created without its APK; it was deleted" % tag)
        if not downloaded_matches(asset["browser_download_url"]):
            github.delete_release(tag)
            raise RuntimeError("the file downloaded from release %s differs from the verified "
                               "APK; the release was deleted" % tag)
        record.update(action="published", url=asset["browser_download_url"],
                      page=created.get("html_url", ""), anonymous_download_matches="true")

    (output / "PUBLICATION.txt").write_text(
        "".join("%s=%s\n" % item for item in record.items()), encoding="utf-8")
    return record


# --------------------------------------------------------------------------------------------
# Self-test: the rules above, exercised against an in-memory stand-in for GitHub.
# --------------------------------------------------------------------------------------------

COMMIT = "797d1567ab8972a4a6930e12f51ee149c5dc877a"
OTHER_COMMIT = "3facc3d051aa31fed31e6087da69b1cb9fb40616"
APK_BYTES = b"PK\x03\x04 stand-in for the debug APK"


def _run(run_id, path, conclusion="success", event="workflow_dispatch", status="completed",
         created="2026-11-15T00:10:00Z", commit=COMMIT):
    return {"id": run_id, "path": path, "conclusion": conclusion, "event": event,
            "status": status, "created_at": created, "head_sha": commit}


class FakeGitHub:
    repo = "owner/repo"

    def __init__(self):
        self.runs = [
            _run(1, ANDROID_CI), _run(2, DIAGNOSTIC), _run(3, RUNTIME, event="pull_request"),
            _run(4, RELEASE_PREVIEW),
        ]
        self.jobs = {
            1: [{"name": "Unit tests", "conclusion": "success"},
                {"name": "Android lint", "conclusion": "success"}],
            2: [{"name": "Native host regression tests", "conclusion": "success"},
                {"name": BUILD_JOB, "conclusion": "success"},
                {"name": VERIFY_JOB, "conclusion": "success"}],
        }
        self.apk = APK_BYTES
        sha = hashlib.sha256(APK_BYTES).hexdigest()
        self.sums = "%s  %s\n" % (sha, APK_FILE)
        self.verification = {
            "downloaded_bytes": str(len(APK_BYTES)), "downloaded_sha256": sha,
            "certificate_sha256": "c" * 64, "source_commit": COMMIT, "workflow_run": "2",
            "render_test_status": "PIXELS_VERIFIED", "render_test_visible_pixels": "560418",
            "render_test_minimum_visible_pixels": "5000",
            "render_test_device": "Google sdk_gphone_x86_64, Android 11 (API 30), x86_64",
            "chain_complete": "true", "failures": "0",
        }
        self.inspection = {"sha256": sha, "applicationId": "com.davemega.hub",
                           "versionName": "0.1.0", "versionCode": "1", "minSdk": "26",
                           "targetSdk": "35", "abis": "arm64-v8a x86_64"}
        self.releases = {}
        self.served = None       # bytes an anonymous download returns; None = the uploaded file
        self.created = []
        self.deleted = []
        self.attach_asset = True
        self.head = COMMIT
        self.changed = []        # paths that differ between the commit and the head; None = not contained

    def resolve_commit(self, commit):
        return COMMIT if COMMIT.startswith(commit) else commit

    def branch_head(self):
        return self.head

    def changes_between(self, commit, head):
        return self.changed

    def runs_for_commit(self, commit):
        return list(self.runs)

    def jobs_of_run(self, run_id):
        return self.jobs.get(run_id, [])

    def download_artifact(self, run_id, name, destination):
        destination.mkdir(parents=True, exist_ok=True)
        if name == APK_ARTIFACT:
            (destination / APK_FILE).write_bytes(self.apk)
            (destination / "SHA256SUMS").write_text(self.sums)
        else:
            (destination / "inspection").mkdir(exist_ok=True)
            (destination / "DOWNLOAD_VERIFICATION.txt").write_text(
                "".join("%s=%s\n" % item for item in self.verification.items()))
            (destination / "inspection" / "APK_INSPECTION.txt").write_text(
                "".join("%s=%s\n" % item for item in self.inspection.items()))

    def release_by_tag(self, tag):
        return self.releases.get(tag)

    def create_release(self, tag, commit, title, notes, asset):
        self.created.append(tag)
        url = "https://example.invalid/%s/%s" % (tag, asset.name)
        assets = [{"name": asset.name, "browser_download_url": url}] if self.attach_asset else []
        self.releases[tag] = {"html_url": "https://example.invalid/" + tag, "assets": assets,
                              "bytes": asset.read_bytes(), "notes": notes.read_text()}

    def delete_release(self, tag):
        self.deleted.append(tag)
        self.releases.pop(tag, None)

    def anonymous_download(self, url, destination):
        tag = url.split("/")[3]
        data = self.served if self.served is not None else self.releases[tag]["bytes"]
        destination.write_bytes(data)


def self_test() -> int:
    failures = []
    tag = "phone-test-%s" % COMMIT[:7]
    asset = "3D-Mega-Hub-debug-%s.apk" % COMMIT[:7]

    def case(name, expect, prepare=None, commit=COMMIT, expected_sha="", pinned=None,
             dry_run=False, check=None):
        """expect: 'published', 'already', 'dry', a text that the refusal must contain,
        or 'error:<text>' for a failure after publishing."""
        fake = FakeGitHub()
        if prepare:
            prepare(fake)
        with tempfile.TemporaryDirectory() as directory:
            try:
                record = publish(fake, commit, expected_sha, pinned, pathlib.Path(directory),
                                 dry_run)
                outcome = {"published": "published", "already published": "already",
                           "dry run, nothing published": "dry"}[record["action"]]
                detail = ""
            except Refused as refusal:
                outcome, detail, record = "refused", str(refusal), None
            except RuntimeError as error:
                outcome, detail, record = "error", str(error), None
        if expect in ("published", "already", "dry"):
            passed = outcome == expect
        elif expect.startswith("error:"):
            passed = outcome == "error" and expect[6:] in detail
        else:
            passed = outcome == "refused" and expect in detail
        if passed and outcome == "refused" and fake.created:
            passed, detail = False, "a release was created although the gate was refused"
        if passed and check:
            problem = check(fake, record)
            if problem:
                passed, detail = False, problem
        print("%s  %s%s" % ("ok  " if passed else "FAIL", name,
                            "" if passed else "  -> got %s: %s" % (outcome, detail)))
        if not passed:
            failures.append(name)

    def replace_run(index, **changes):
        def apply(fake):
            fake.runs[index] = dict(fake.runs[index], **changes)
        return apply

    def add_run(run):
        def apply(fake):
            fake.runs.append(run)
        return apply

    def set_verification(**changes):
        def apply(fake):
            fake.verification.update(changes)
        return apply

    def published_correctly(fake, record):
        release = fake.releases.get(tag)
        if not release or release["bytes"] != APK_BYTES:
            return "the release does not hold the verified APK"
        if record["url"] != "https://example.invalid/%s/%s" % (tag, asset):
            return "unexpected download address %s" % record["url"]
        for needed in (COMMIT, record["sha256"], "UNVERIFIED", "PIXELS_VERIFIED", "560,418"):
            if needed not in release["notes"]:
                return "the release notes do not contain %s" % needed
        return None

    case("gate holds: the APK is published and downloads back identical", "published",
         check=published_correctly)
    case("a short commit is resolved to the full one", "published", commit=COMMIT[:7])

    def head_ahead_by(*paths):
        def apply(fake):
            fake.head, fake.changed = "a" * 40, list(paths)
        return apply

    case("the head is ahead by documents and the publishing files only", "published",
         head_ahead_by("README.md", "BUILD_AND_VERIFY.md", "docs/BRAVO_REVIEW.md",
                       *PUBLISHING_FILES),
         check=lambda fake, record: None if "a" * 40 in fake.releases[tag]["notes"]
         and "README.md" in fake.releases[tag]["notes"] else "the notes do not name the head")
    case("the head changed the build configuration", "changes that are not in the APK",
         head_ahead_by("README.md", "app/build.gradle.kts"))
    case("the head changed native source", "changes that are not in the APK",
         head_ahead_by("avatarforge-engine/src/main/cpp/src/af_glb.cpp"))
    case("the head changed a gate workflow", "changes that are not in the APK",
         head_ahead_by(".github/workflows/android.yml"))
    case("the head changed a gate script", "changes that are not in the APK",
         head_ahead_by(".github/scripts/verify-downloaded-apk.sh"))
    case("a markdown file inside the app is a packaged asset, not a document",
         "changes that are not in the APK", head_ahead_by("app/src/main/assets/notes.md"))
    case("a non-markdown file under docs", "changes that are not in the APK",
         head_ahead_by("docs/model.glb"))
    case("the commit is not part of the branch", "is not part of the branch",
         lambda fake: setattr(fake, "changed", None))
    case("the expected SHA-256 matches", "published",
         expected_sha=hashlib.sha256(APK_BYTES).hexdigest())
    case("dry run publishes nothing", "dry", dry_run=True,
         check=lambda fake, record: "a release was created" if fake.created else None)
    case("Android CI failed", "ended with failure", replace_run(0, conclusion="failure"))
    case("Android CI never ran", "no finished run",
         lambda fake: fake.runs.pop(0))
    case("Android CI ran only for another commit", "no finished run",
         replace_run(0, head_sha=OTHER_COMMIT))
    case("release preview still running", "is still in_progress",
         replace_run(3, status="in_progress", conclusion=None))
    case("runtime verification failed", "ended with failure",
         replace_run(2, conclusion="failure"))
    case("diagnostic run was started by a pull request", "started on the branch",
         replace_run(1, event="pull_request"))
    case("Android CI was started by a pull request", "started on the branch",
         replace_run(0, event="pull_request"))
    case("a cancelled later run is ignored", "published",
         add_run(_run(9, DIAGNOSTIC, conclusion="cancelled", created="2026-11-15T01:00:00Z")))
    case("a failed later run blocks", "ended with failure",
         add_run(_run(9, DIAGNOSTIC, conclusion="failure", created="2026-11-15T01:00:00Z")))
    case("a job of the diagnostic run was skipped", "ended with skipped",
         lambda fake: fake.jobs[2].__setitem__(
             0, {"name": "Native host regression tests", "conclusion": "skipped"}))
    case("the diagnostic run has no download-check job", "predates the download check",
         lambda fake: fake.jobs[2].pop())
    case("a job of Android CI failed", "ended with failure",
         lambda fake: fake.jobs[1].__setitem__(
             0, {"name": "Unit tests", "conclusion": "failure"}))
    case("the download check did not complete the chain", "chain_complete=true",
         set_verification(chain_complete="false", failures="1"))
    case("the record names another commit", "names commit",
         set_verification(source_commit=OTHER_COMMIT))
    case("the record names another run", "names workflow run",
         set_verification(workflow_run="77"))
    case("the render test did not verify pixels", "not PIXELS_VERIFIED",
         set_verification(render_test_status="FRAME_PRESENTED"))
    case("the APK in the artifact is not the verified file", "the artifact now holds",
         lambda fake: setattr(fake, "apk", APK_BYTES + b"!"))
    case("SHA256SUMS names another file", "SHA256SUMS names",
         lambda fake: setattr(fake, "sums", "%s  x\n" % ("0" * 64)))
    case("the expected SHA-256 differs", "not the expected", expected_sha="1" * 64)
    case("the expected SHA-256 is malformed", "64 hexadecimal", expected_sha="abc")
    case("a branch name is not accepted as the commit", "hexadecimal characters",
         commit="main")
    case("a pinned run that is not a run of this commit", "with run id 55", pinned=55)
    case("a pinned run selects that run", "published", pinned=2)
    later = _run(9, DIAGNOSTIC, created="2026-11-15T01:00:00Z")
    later_jobs = lambda fake: fake.jobs.__setitem__(9, list(fake.jobs[2]))
    case("two successful diagnostic runs: the later one is taken, and its record must name it",
         "names workflow run", lambda fake: (add_run(later)(fake), later_jobs(fake)))
    case("two successful diagnostic runs: the earlier one can be pinned", "published",
         lambda fake: (add_run(later)(fake), later_jobs(fake)), pinned=2)
    case("the release exists with the same file", "already",
         lambda fake: fake.releases.__setitem__(tag, {
             "html_url": "https://example.invalid/" + tag, "bytes": APK_BYTES,
             "assets": [{"name": asset,
                         "browser_download_url": "https://example.invalid/%s/%s" % (tag, asset)}]}),
         check=lambda fake, record: "a second release was created" if fake.created else None)
    case("the release exists with a different file", "holds a different file",
         lambda fake: fake.releases.__setitem__(tag, {
             "html_url": "https://example.invalid/" + tag, "bytes": b"something else",
             "assets": [{"name": asset,
                         "browser_download_url": "https://example.invalid/%s/%s" % (tag, asset)}]}))
    case("the release exists without the APK", "does not hold",
         lambda fake: fake.releases.__setitem__(tag, {"html_url": "x", "assets": []}))
    case("the published file downloads back different: the release is deleted",
         "error:the release was deleted",
         lambda fake: setattr(fake, "served", b"corrupted on the way"),
         check=lambda fake, record: None if fake.deleted == [tag] and tag not in fake.releases
         else "the bad release was left in place")
    case("the release is created without its file: it is deleted", "error:it was deleted",
         lambda fake: setattr(fake, "attach_asset", False),
         check=lambda fake, record: None if fake.deleted == [tag] else "left in place")

    print("%d case(s) failed" % len(failures) if failures else "all publishing rules hold")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--commit")
    parser.add_argument("--expected-sha256", default="")
    parser.add_argument("--run", default="", help="diagnostic run to take the APK from")
    parser.add_argument("--output", default="phone-test-publication")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    if not repo or not arguments.commit:
        parser.error("--commit and the GITHUB_REPOSITORY environment variable are required")
    run_text = arguments.run.strip()
    if run_text and not run_text.isdigit():
        parser.error("--run must be a workflow run id")
    try:
        record = publish(GitHub(repo), arguments.commit.strip(), arguments.expected_sha256,
                         int(run_text) if run_text else None, pathlib.Path(arguments.output),
                         arguments.dry_run)
    except Refused as refusal:
        print("PHONE_TEST_APK_NOT_PUBLISHED reason=%s" % refusal)
        print("::error::Not published: %s" % refusal)
        return 1
    print("PHONE_TEST_APK action=\"%s\" commit=%s tag=%s sha256=%s bytes=%s diagnostic_run=%s "
          "anonymous_download_matches=%s url=%s" % (
              record["action"], record["commit"], record["tag"], record["sha256"],
              record["bytes"], record["diagnostic_run"], record["anonymous_download_matches"],
              record["url"]))
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary and record["url"]:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write("### Phone-test APK: %s\n\n" % record["action"])
            handle.write("- Direct download: %s\n" % record["url"])
            handle.write("- SHA-256: `%s` (%s bytes)\n" % (record["sha256"],
                                                          number(record["bytes"])))
            handle.write("- Commit: `%s`\n" % record["commit"])
            handle.write("- Downloaded again without signing in and found identical: %s\n"
                         % record["anonymous_download_matches"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
