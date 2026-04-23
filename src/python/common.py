#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


import os
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]


# Executes a command and returns its stdout or None if it failed.
def command(*args):
    result = subprocess.run(
        args,
        stderr=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else None


# Retrieves the InspIRCd version from Git or the version file.
def version():
    segments = {"MAJOR": "0", "MINOR": "0", "PATCH": "0", "LABEL": None}

    # Attempt to retrieve version information from src/cmake/version.cmake
    with open(ROOT / "src" / "cmake" / "version.cmake", "r") as fh:
        for type, version in re.findall(r"\bset\(VERSION_([A-Z]+)\s+\"?(.+?)\"?\)", fh.read()):
            segments[type] = version

    # Attempt to retrieve missing version information from Git
    git_version = command(os.getenv("GIT", "git"), "describe", "--tags")
    if git_version:
        if match := re.fullmatch(r"v([0-9]+)\.([0-9]+)\.([0-9]+)(?:[a-z]+\d+)?(?:-\d+-g([0-9a-f]+))?", git_version):
            segments["MAJOR"] = match.group(1)
            segments["MINOR"] = match.group(2)
            segments["PATCH"] = match.group(3)
            if match.group(4):
                segments["LABEL"] = match.group(4)

    # Build the full version string
    segments["FULL"] = ".".join(str(segments[k]) for k in ["MAJOR", "MINOR", "PATCH"])
    if segments["LABEL"]:
        segments["FULL"] += "-" + segments["LABEL"]

    return {k: int(v) if v.isnumeric() else v for k, v in segments.items()}
