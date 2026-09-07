#!/usr/bin/env python3
#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2026 Sadie Powell <sadie@sadiepowell.dev>
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

import base64
import getpass
import hashlib
import hmac
import os
import secrets
import sys
import textwrap

import argon2  # pip3 install argon2-cffi
import bcrypt  # pip3 install bcrypt

program = os.path.basename(__file__)
if len(sys.argv) < 2:
    print(
        textwrap.dedent(f"""
        Usage: {program} <algorithm> [password]

        The algorithm can be one of:
            * argon2d               (hash_argon2)
            * argon2i               (hash_argon2)
            * argon2id              (hash_argon2)
            * bcrypt                (hash_bcrypt)
            * hmac-sha1             (hash_gnutls or hash_sha1)
            * hmac-sha224           (hash_gnutls or hash_sha2)
            * hmac-sha256           (hash_gnutls or hash_sha2)
            * hmac-sha384           (hash_gnutls or hash_sha2)
            * hmac-sha512           (hash_gnutls or hash_sha2)
            * hmac-sha3-224         (hash_gnutls)
            * hmac-sha3-256         (hash_gnutls)
            * hmac-sha3-384         (hash_gnutls)
            * hmac-sha3-512         (hash_gnutls)
            * pbkdf2-hmac-sha1      (hash_pbkdf2 and hash_gnutls or hash_sha1)
            * pbkdf2-hmac-sha224    (hash_pbkdf2 and hash_gnutls or hash_sha2)
            * pbkdf2-hmac-sha256    (hash_pbkdf2 and hash_gnutls or hash_sha2)
            * pbkdf2-hmac-sha384    (hash_pbkdf2 and hash_gnutls or hash_sha2)
            * pbkdf2-hmac-sha512    (hash_pbkdf2 and hash_gnutls or hash_sha2)
            * pbkdf2-hmac-sha3-224  (hash_pbkdf2 and hash_gnutls)
            * pbkdf2-hmac-sha3-256  (hash_pbkdf2 and hash_gnutls)
            * pbkdf2-hmac-sha3-384  (hash_pbkdf2 and hash_gnutls)
            * pbkdf2-hmac-sha3-512  (hash_pbkdf2 and hash_gnutls)

        If a password is not specified it will be read from stdin.
        """).strip()
    )
    sys.exit(1)

algorithm = sys.argv[1]
password = sys.argv[2] if len(sys.argv) >= 3 else getpass.getpass()


def do_argon2(variant, password):
    ph = argon2.PasswordHasher(
        time_cost=int(os.getenv("ARGON2_TIME_COST", "3")),
        memory_cost=int(os.getenv("ARGON2_TIME_COST", "131072")),
        parallelism=int(os.getenv("ARGON2_PARALLELISM", "1")),
        hash_len=int(os.getenv("ARGON2_SALT_LENGTH", "32")),
        salt_len=int(os.getenv("ARGON2_HASH_LENGTH", "32")),
        type=variant,
    )
    return ph.hash(password)


def do_bcrypt(password):
    rounds = int(os.getenv("BCRYPT_ROUNDS", "10"))
    salt = bcrypt.gensalt(rounds)
    return bcrypt.hashpw(password.encode("utf-8"), salt).decode("utf-8")


def do_hmac(digest, password):
    key = secrets.token_bytes(digest().digest_size)
    key_b64 = base64.b64encode(key).decode("utf-8").rstrip("=")
    mac = hmac.HMAC(key, password.encode("utf-8"), digestmod=digest)
    mac_b64 = base64.b64encode(mac.digest()).decode("utf-8").rstrip("=")
    return f"{key_b64}${mac_b64}"


def do_pbkdf2(digestname, password):
    iterations = int(os.getenv("PBKDF2_ITERATIONS", "15000"))
    salt = secrets.token_bytes(int(os.getenv("PBKDF2_SALT_LENGTH", "32")))
    salt_b64 = base64.b64encode(salt).decode("utf-8").rstrip("=")
    mac = hashlib.pbkdf2_hmac(
        digestname,
        password.encode("utf-8"),
        salt,
        iterations,
        dklen=int(os.getenv("PBKDF2_HASH_LENGTH", "32")),
    )
    mac_b64 = base64.b64encode(mac).decode("utf-8").rstrip("=")
    return f"{iterations}:{mac_b64}:{salt_b64}"


function = None
modules = []
match algorithm.lower():
    case "argon2d":
        function = lambda pw: do_argon2(argon2.Type.D, pw)
        modules.append("hash_argon2")
    case "argon2i":
        function = lambda pw: do_argon2(argon2.Type.I, pw)
        modules.append("hash_argon2")
    case "argon2id":
        function = lambda pw: do_argon2(argon2.Type.ID, pw)
        modules.append("hash_argon2")
    case "bcrypt":
        function = lambda pw: do_bcrypt(password)
        modules.append("hash_bcrypt")
    case "hmac-sha1":
        function = lambda pw: do_hmac(hashlib.sha224, pw)
        modules.append("hash_gnutls or hash_sha1")
    case "hmac-sha224":
        function = lambda pw: do_hmac(hashlib.sha224, pw)
        modules.append("hash_gnutls or hash_sha2")
    case "hmac-sha256":
        function = lambda pw: do_hmac(hashlib.sha256, pw)
        modules.append("hash_gnutls or hash_sha2")
    case "hmac-sha384":
        function = lambda pw: do_hmac(hashlib.sha384, pw)
        modules.append("hash_gnutls or hash_sha2")
    case "hmac-sha512":
        function = lambda pw: do_hmac(hashlib.sha512, pw)
        modules.append("hash_gnutls or hash_sha2")
    case "hmac-sha3-224":
        function = lambda pw: do_hmac(hashlib.sha3_224, pw)
        modules.append("hash_gnutls")
    case "hmac-sha3-256":
        function = lambda pw: do_hmac(hashlib.sha3_256, pw)
        modules.append("hash_gnutls")
    case "hmac-sha3-384":
        function = lambda pw: do_hmac(hashlib.sha3_384, pw)
        modules.append("hash_gnutls")
    case "hmac-sha3-512":
        function = lambda pw: do_hmac(hashlib.sha3_512, pw)
        modules.append("hash_gnutls")
    case "pbkdf2-hmac-sha1":
        function = lambda pw: do_pbkdf2("sha224", pw)
        modules.append("hash_pbkdf2 and hash_gnutls or hash_sha1")
    case "pbkdf2-hmac-sha224":
        function = lambda pw: do_pbkdf2("sha224", pw)
        modules.append("hash_pbkdf2 and hash_gnutls or hash_sha2")
    case "pbkdf2-hmac-sha256":
        function = lambda pw: do_pbkdf2("sha256", pw)
        modules.append("hash_pbkdf2 and hash_gnutls or hash_sha2")
    case "pbkdf2-hmac-sha384":
        function = lambda pw: do_pbkdf2("sha384", pw)
        modules.append("hash_pbkdf2 and hash_gnutls or hash_sha2")
    case "pbkdf2-hmac-sha512":
        function = lambda pw: do_pbkdf2("sha512", pw)
        modules.append("hash_pbkdf2 and hash_gnutls or hash_sha2")
    case "pbkdf2-hmac-sha3-224":
        function = lambda pw: do_pbkdf2("sha3_224", pw)
        modules.append("hash_pbkdf2 and hash_gnutls")
    case "pbkdf2-hmac-sha3-256":
        function = lambda pw: do_pbkdf2("sha3_256", pw)
        modules.append("hash_pbkdf2 and hash_gnutls")
    case "pbkdf2-hmac-sha3-384":
        function = lambda pw: do_pbkdf2("sha3_384", pw)
        modules.append("hash_pbkdf2 and hash_gnutls")
    case "pbkdf2-hmac-sha3-512":
        function = lambda pw: do_pbkdf2("sha3_512", pw)
        modules.append("hash_pbkdf2 and hash_gnutls")

if not function:
    print(f"Error: unknown algorithm: {algorithm}", file=sys.stderr)
    sys.exit(1)

password_hash = function(password)
print(
    textwrap.dedent(f"""
    The {algorithm} hash for the password you specified is:")
        {password_hash}

    For use in the server configuration:
        password="{password_hash}"
        hash="{algorithm}"

    Make sure you have the following modules loaded:
    """).strip()
)

for module in modules:
    print(f"  * {module}")
