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


import pathlib

from . import common

DIR         = common.ROOT / "modules"
CONTRIB_DIR = DIR         / "contrib"
EXTRA_DIR   = DIR         / "extra"


# Converts a module name to a file name.
def file_name(module):
    return pathlib.Path(module).with_suffix(".cpp").name


# Convert a module file name or path to a short name.
def short_name(module):
    return pathlib.Path(module).stem


# Compares two module versions and returns -1 if older, 0 if equal, or 1 if newer.
def version_compare(lhs, rhs):
    for lhs, rhs in zip(lhs, rhs):
        if lhs is None or rhs is None:
            break
        elif lhs < rhs:
            return -1
        elif lhs > rhs:
            return 1
    return 0


# Checks whether a module version matches a requirement range.
def version_in_range(version, requirement):
    if not version:
        return requirement

    min_requirement, max_requirement = requirement
    if version_compare(min_requirement, version) > 0:
        return False
    elif version_compare(version, max_requirement) > 0:
        return False

    return True


# Converts a module version string to a tuple of three elements.
def version_parse(version):
    if not version:
        return [None, None, None]

    segments = [int(s) for s in version.split(".") if s.isdigit()]
    return tuple((segments + [None, None, None])[:3])


# Converts a tuple of three elements to a module version string.
def version_string(segments):
    return ".".join(str(segment) for segment in segments if segment is not None)


# Converts a two tuples of three elements to a module version range.
def version_range_string(range):
    return "-".join(version_string(version) for version in set(range))
