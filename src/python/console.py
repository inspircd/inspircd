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
import re
import sys

DEBUG       = int(os.getenv("INSPIRCD_DEBUG", "0")) > 0
RESET       = "\x1B[0m"    if sys.stdout.isatty() else ""
BOLD        = "\x1B[1m"    if sys.stdout.isatty() else ""
BOLD_RED    = "\x1B[1;31m" if sys.stdout.isatty() else ""
BOLD_GREEN  = "\x1B[1;32m" if sys.stdout.isatty() else ""
BOLD_YELLOW = "\x1B[1;33m" if sys.stdout.isatty() else ""
FAINT       = "\x1B[2m"    if sys.stdout.isatty() else ""


# Measures the length of a str with the ANSI escape codes removed.
def ansi_len(text):
    return len(re.sub(r"\x1b\[[0-9;]*m", "", text))


# Wraps some text with console colored text.
def color(text, color):
    return "".join([color, text, RESET])


# Prints a debug message to the standard output stream if INSPIRCD_DEBUG is set.
def debug(message):
    if DEBUG:
        print(f"{BOLD}!!!{RESET} {message}", file=sys.stderr)


# Prints an error message to the standard error stream and exits.
def error(*messages):
    print(f"{BOLD_RED}Error:{RESET} ", end="", file=sys.stderr)
    for message in messages:
        print(message, file=sys.stderr)
    sys.exit(1)


# Dispatches commands to their handler method.
def subcommand(args, start, commands, default="help"):
    commands["help"] = {
        "function": subcommand_help,
        "help": "Show this message and exit",
    }

    command = args[start].lower() if len(args) > start else default
    if command in commands:  # The user specified an exact match.
        command_match = command
    else:  # Also allow users to specify a partial command for convenience.
        command_matches = [c for c in commands if c.startswith(command)]
        command_match = command_matches[0] if len(command_matches) == 1 else None

    if not command_match:
        prefix = " ".join(sys.argv[0:start])
        error(
            f"{command} is not a recognised subcommand.",
            f"Use `{prefix} help` for a list of subcommands.",
        )

    debug(f"Command match: {command} => {command_match}")
    command_data = commands[command_match]
    return command_data["function"](commands, args, start + 1)


# Prints the help output for a subcommand.
def subcommand_help(commands, args, start):
    print(f"{BOLD}Usage{RESET}: {args[0]} <command> [options...]")
    print()
    print("Commands:")

    command_end = start - (1 if len(args) >= start else 0)
    command_prefix = "".join([f"{a} " for a in args[1:command_end]])

    help_entries = {}
    for command_name, command in commands.items():
        if "help" not in command:
            continue

        help_entries[f"{command_prefix}{command_name}"] = command["help"]
        option_padding = " " * len(command_prefix)
        for option_name, option_help in command.get("options", {}).items():
            help_entries[f"{option_padding}{command_name} {option_name}"] = option_help

    help_padding = max(len(e) for e in help_entries)
    for help_name, help_message in help_entries.items():
        print(f"  {help_name:<{help_padding}}    {help_message}")


# Formats and prints a table to the console.
def table(headers, rows, separator="|"):
    if headers is not None:
        rows.insert(0, ["-" * ansi_len(header) for header in headers])
        rows.insert(0, [color(header, BOLD) for header in headers])

    widths = [max(ansi_len(field) for field in col) for col in zip(*rows)]
    for row in rows:
        padded_row = []
        for i, item in enumerate(row):
            padding = " " * (widths[i] - ansi_len(item))
            padded_row.append(f"{item}{padding}")
        print(f" {separator} ".join(padded_row).strip())


# Prints a warning message to the standard error stream.
def warning(*messages):
    print(f"{BOLD_YELLOW}Warning:{RESET} ", end="", file=sys.stderr)
    for message in messages:
        print(message, file=sys.stderr)
