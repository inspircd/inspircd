## About

InspIRCd is a modular C++ Internet Relay Chat (IRC) server for UNIX-like and Windows systems.

## Supported Platforms

InspIRCd is supported on the following platforms:

- Most recent BSD variants using the Clang 5+ or GCC 7+ compilers and the GNU toolchains (Make, etc).

- Most recent Linux distributions using the Clang 5+ or GCC 7+ compilers and the GNU toolchain.

- The three most recent major releases of macOS using the AppleClang 10, Clang 5+, or GCC 7+ compilers and the GNU toolchain.

- Windows 10 April 2018 Update or newer using the MSVC 19.15+ (Visual Studio 15.8 2017) compiler and CMake 3.20 or newer.

Other platforms and toolchains may also work but are not officially supported by the InspIRCd team. Generally speaking if you are using a reasonably modern UNIX-like system you should be able to build InspIRCd on it. If you can not and you wish to submit a patch we are happy to accept it as long as it is not extremely large.

If you encounter any bugs then [please file an issue](https://github.com/inspircd/inspircd/issues/new/choose).

## Installation

Most InspIRCd users running a UNIX-like system build from source. A guide about how to do this is available on [the InspIRCd docs site](https://docs.inspircd.org/4/installation/source).

Building from source on Windows is generally not recommended but [a guide is available](https://docs.inspircd.org/4/installation/windows-source/) if you wish to do this.

If you are running on Debian 12/13, RHEL 8/9, Ubuntu 22.04/24.04, or Windows 10+ binary packages are available from [the downloads page](https://github.com/inspircd/inspircd/releases/latest).

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
