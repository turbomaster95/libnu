#!/usr/bin/env python3

import json  # noqa: I001
import pathlib
import os
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
import socket
import time
import subprocess


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


def apply_patch(patch, file):
    path = pathlib.Path(file)

    data = path.read_text()

    old, new = patch["replace"]

    if old in data:
        path.write_text(data.replace(old, new))


def main():
    log = RegLog("regret")
    log.log("This is Regret v0.0.1")
    log.log(f"using depfile: {mainfile}")
    log.log("fetching dependencies...")
    print()

    jsf = load_conf()

    start_time = time.monotonic()
    count = 0

    for dep in jsf["depends"]:
        if jsf["depends"][dep]["rel"]:
            file = get_url(jsf["depends"][dep]["file"], jsf["depends"][dep]["rel"])
        else:
            file = get_url(jsf["depends"][dep]["file"])

        download(file, dep, jsf["depends"][dep]["pname"])

        try:
            if jsf["depends"][dep]["replace"]:
                print()
                log.log(f"Patching file {dep}")
                apply_patch(jsf["depends"][dep]["replace"], dep)

        except KeyError:
            pass

        count += 1

    elapsed = time.monotonic() - start_time

    print()
    print(f"{count} dependencies fetched in {elapsed:.1f}s")


if __name__ == "__main__":
    main()
