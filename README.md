## About

InspIRCd is a modular C++ Internet Relay Chat (IRC) server for UNIX-like and Windows systems.

**IMPORTANT** InspIRCd v3 is no longer maintained. Please upgrade as soon as possible.

## Supported Platforms

InspIRCd is supported on the following platforms:

- Most recent BSD variants using the Clang or GCC compilers and the GNU toolchains (Make, etc).

- Most recent Linux distributions using the Clang or GCC compilers and the GNU toolchain.

- The most recent three major releases of macOS using the AppleClang, Clang, or GCC (*not* LLVM-GCC) compilers and the GNU toolchains.

- Windows 7 or newer using the MSVC 14 (Visual Studio 2015) compiler and CMake 3.8 or newer.

Other platforms and toolchains may also work but are not officially supported by the InspIRCd team. Generally speaking if you are using a reasonably modern UNIX-like system you should be able to build InspIRCd on it. If you can not and you wish to submit a patch we are happy to accept it as long as it is not extremely large.

If you encounter any bugs then [please file an issue](https://github.com/inspircd/inspircd/issues/new/choose).

## Installation

Most InspIRCd users running a UNIX-like system build from source. A guide about how to do this is available on [the InspIRCd docs site](https://docs.inspircd.org/3/installation/source).

Building from source on Windows is generally not recommended but [a guide is available](https://docs.inspircd.org/3/installation/windows-source/) if you wish to do this.

If you are running on Debian 11/12/13, RHEL 8/9, Ubuntu 20.04/22.04, or Windows 8+ binary packages are available from [the downloads page](https://github.com/inspircd/inspircd/releases/latest).

A [Docker](https://www.docker.com) image is also available. See [the inspircd-docker repository](https://github.com/inspircd/inspircd-docker) for more information.

Some distributions ship an InspIRCd package in their package managers. We generally do not recommend the use of such packages as in the past distributions have made broken modifications to InspIRCd and not kept their packages up to date with essential security updates.

## License

InspIRCd is licensed under [version 2 of the GNU General Public License](https://docs.inspircd.org/license).

## External Links

* [Website](https://www.inspircd.org)
* [Documentation](https://docs.inspircd.org)
* [GitHub](https://github.com/inspircd)
* [Social Media](https://docs.inspircd.org/social)
* Support IRC channel &mdash; \#inspircd on irc.teranova.net (TLS only)
* Development IRC channel &mdash; \#inspircd.dev on irc.teranova.net (TLS only)
* InspIRCd test network &mdash; testnet.inspircd.org (TLS only)
