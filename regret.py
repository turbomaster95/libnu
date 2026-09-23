#!/usr/bin/env python3

import json
import pathlib
import os
from urllib.request import Request, urlopen


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
        return "https://raw.githack.com/" + file
    else:
        return file


def download(url, destination):
    request = Request(url, headers={"User-Agent": "regret/0.0.1"})

    pathlib.Path(destination).parent.mkdir(parents=True, exist_ok=True)

    with urlopen(request) as response, open(destination, "wb") as output:
        output.write(response.read())


def main():
    log = RegLog("regret")
    log.log("This is Regret v0.0.1")
    log.log(f"Using default config file at: {mainfile}")

    jsf = load_conf()

    for dep in jsf["depends"]:
        iname = dep
        pname = jsf["depends"][dep]["pname"]
        if jsf["depends"][dep]["rel"]:
            file = get_url(jsf["depends"][dep]["file"], jsf["depends"][dep]["rel"])
        else:
            file = get_url(jsf["depends"][dep]["file"])

        log.log(f"iname: {iname}, File rl: {file}, pretty name: {pname}")
        download(file, iname)


if __name__ == "__main__":
    main()
