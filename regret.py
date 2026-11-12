#!/usr/bin/env python3

import json  # noqa: I001
import pathlib
import os
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
import socket
import time
import subprocess
import hashlib


class RegLog:
    def __init__(self, name):
        self.logname = name

    def log(self, msg):
        print(f"[{self.logname}] {msg}")


mainfile = pathlib.Path("./.regretconf")


def load_conf():
    if not os.path.exists(mainfile):
        return {"depends": {}}
    with open(mainfile, "r") as f:
        return json.load(f)

def save_conf(config):
    mainfile.write_text(json.dumps(config, indent=4) + "\n")

def get_url(file, release=False):
    if release == False:
        return "https://cdn.jsdelivr.net/gh/" + file
    else:
        return file


def download(url, destination, pname, retries=5, timeout=8):
    request = Request(
        url,
        headers={
            "User-Agent": "regret/0.0.1",
            "Accept": "*/*",
            "Connection": "keep-alive",
        },
    )

    destination = pathlib.Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)

    temporary = destination.with_name(destination.name + ".tmp")

    def clear_line():
        print("\r\033[2K", end="")

    def format_size(size):
        if size >= 1024 * 1024:
            return f"{size / (1024 * 1024):.2f} MiB"
        return f"{size / 1024:.0f} KiB"

    def format_speed(speed):
        if speed >= 1024 * 1024:
            return f"{speed / (1024 * 1024):.1f} MiB/s"
        return f"{speed / 1024:.1f} KiB/s"

    def draw_progress(done, total, elapsed):
        if total > 0:
            percent = min(100, done * 100 // total)
            filled = min(20, done * 20 // total)
        else:
            percent = 0
            filled = 0

        bar = "#" * filled + "-" * (20 - filled)

        speed = done / max(elapsed, 0.001)

        print(
            f"\r\033[2K  {pname:<16} "
            f"[{bar}] {percent:3d}%"
            f"  {format_size(done):>8}"
            f"  {format_speed(speed):>10}",
            end="",
            flush=True,
        )

    for attempt in range(1, retries + 1):
        try:
            clear_line()
            print(
                f"  {pname:<16} [--------------------]   0%  connecting...",
                end="",
                flush=True,
            )

            started = time.monotonic()

            with urlopen(request, timeout=timeout) as response:
                total_size = int(response.headers.get("Content-Length", 0))

                downloaded = 0
                last_update = 0.0

                with temporary.open("wb") as output:
                    while True:
                        chunk = response.read(1024 * 1024)

                        if not chunk:
                            break

                        output.write(chunk)
                        downloaded += len(chunk)

                        now = time.monotonic()

                        if now - last_update >= 0.08:
                            draw_progress(
                                downloaded,
                                total_size,
                                now - started,
                            )
                            last_update = now

                    output.flush()

                temporary.replace(destination)

            elapsed = max(time.monotonic() - started, 0.001)
            speed = downloaded / elapsed

            clear_line()
            print(
                f"  {pname:<16} ✓"
                f"  {format_size(downloaded):>8}"
                f"  {format_speed(speed):>10}",
                flush=True,
            )

            return

        except HTTPError as error:
            retryable = error.code in {
                408,
                425,
                429,
                500,
                502,
                503,
                504,
            }

            if not retryable or attempt >= retries:
                clear_line()
                raise

            retry_after = error.headers.get("Retry-After")

            if retry_after:
                try:
                    delay = max(1, int(retry_after))
                except ValueError:
                    delay = min(2 ** (attempt - 1), 8)
            else:
                delay = min(2 ** (attempt - 1), 8)

            clear_line()
            print(
                f"  {pname:<16} ↳ HTTP {error.code}"
                f" · retrying in {delay}s ({attempt}/{retries})",
                flush=True,
            )

            time.sleep(delay)

        except (
            URLError,
            socket.gaierror,
            TimeoutError,
            ConnectionError,
        ) as error:
            if attempt >= retries:
                clear_line()
                raise

            delay = min(2 ** (attempt - 1), 8)

            clear_line()
            print(
                f"  {pname:<16} ↳ connection failed"
                f" · retrying in {delay}s ({attempt}/{retries})",
                flush=True,
            )

            time.sleep(delay)

        finally:
            if temporary.exists() and attempt >= retries:
                try:
                    temporary.unlink()
                except OSError:
                    pass

def update_gitignore(paths):
    gitignore = pathlib.Path(".gitignore")
    content = gitignore.read_text() if gitignore.exists() else ""
    lines = content.splitlines()

    marker = "# Files managed by regret"
    existing = {line.strip() for line in lines}
    new_entries = [str(path) for path in paths if str(path) not in existing]

    if not new_entries:
        return

    if lines and lines[-1].strip():
        lines.append("")

    if marker not in existing:
        lines.append(marker)

    lines.extend(new_entries)
    gitignore.write_text("\n".join(lines) + "\n")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def apply_patch(patch, file):
    path = pathlib.Path(file)
    data = path.read_text()

    old, new = patch

    if old in data:
        path.write_text(data.replace(old, new))
    elif new not in data:
        raise RuntimeError(f"Patch text not found in {file}")

def run_regret(check_only=False, update=False):
    log = RegLog("regret")
    log.log("This is Regret v0.0.1")
    log.log(f"using depfile: {mainfile}")
    log.log("checking dependencies..." if check_only else "fetching dependencies...")
    print()

    config = load_conf()
    dependencies = config.get("depends", {})
    start_time = time.monotonic()

    for dep, info in dependencies.items():
        path = pathlib.Path(dep)
        expected_hash = info.get("sha256")

        valid = path.is_file()
        if valid and expected_hash:
            valid = sha256_file(path).lower() == expected_hash.lower()

        if check_only and valid:
            print(f"  {info['pname']:<16} ✓ already present")
        else:
            url = get_url(info["file"], info.get("rel", False))
            download(url, dep, info["pname"])

        patch = info.get("replace")
        if patch:
            apply_patch(patch, dep)

        actual_hash = sha256_file(path)

        if expected_hash and not update:
            if actual_hash.lower() != expected_hash.lower():
                raise RuntimeError(
                    f"SHA-256 mismatch for {dep}: expected "
                    f"{expected_hash}, got {actual_hash}. "
                    "Run 'regret.py update' to accept a new version."
                )

        if not expected_hash or update:
            info["sha256"] = actual_hash
            save_conf(config)

        patch = info.get("replace")
        if patch:
            apply_patch(patch, dep)

    elapsed = time.monotonic() - start_time
    print()
    print(f"{len(dependencies)} dependencies checked in {elapsed:.1f}s")
    update_gitignore(dependencies.keys())


def main():
    import sys

    if len(sys.argv) == 2 and sys.argv[1] == "check":
        run_regret(check_only=True)
    elif len(sys.argv) == 2 and sys.argv[1] == "update":
        run_regret(update=True)
    elif len(sys.argv) == 1:
        run_regret()
    else:
        raise SystemExit(f"Usage: {sys.argv[0]} [check|update]")

if __name__ == "__main__":
    main()
