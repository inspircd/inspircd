## About

InspIRCd is a modular C++ Internet Relay Chat (IRC) server for UNIX-like and Windows systems.

## Supported Platforms

InspIRCd is supported on the following platforms:

- Most recent BSD variants using the Clang or GCC compilers and the GNU toolchains (Make, etc).

- Most recent Linux distributions using the Clang or GCC compilers and the GNU toolchain.

- The most recent three major releases of macOS using the AppleClang, Clang, or GCC (*not* LLVM-GCC) compilers and the GNU toolchains.

- Windows 7 or newer using the MSVC 14 (Visual Studio 2015) compiler and CMake 2.8 or newer.

Alternate platforms and toolchains may also work but are not officially supported by the InspIRCd team. Generally speaking if you are using a reasonably modern UNIX-like system you should be able to build InspIRCd on it.

If you encounter any bugs then [please file an issue](https://github.com/inspircd/inspircd/issues/new/choose).

## Installation

Most InspIRCd users running a UNIX-like system build from source. A guide about how to do this is available on [the InspIRCd docs site](https://docs.inspircd.org/3/installation/source).

Building from source on Windows is generally not recommended but [a guide is available](https://github.com/inspircd/inspircd/blob/master/win/README.txt) if you wish to do this.

If you are running on CentOS 7/8, Debian 10/11, Ubuntu 18.04/20.04, or Windows 7+ binary packages are available from [the downloads page](https://github.com/inspircd/inspircd/releases/latest).

A [Docker](https://www.docker.com) image is also available. See [the inspircd-docker repository](https://github.com/inspircd/inspircd-docker) for more information.

Some distributions ship an InspIRCd package in their package managers. We generally do not recommend the use of such packages as in the past distributions have made broken modifications to InspIRCd and not kept their packages up to date with essential security updates.

## License

InspIRCd is licensed under [version 2 of the GNU General Public License](https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html).

## External Links

* [Website](https://www.inspircd.org)
* [Documentation](https://docs.inspircd.org)
* [GitHub](https://github.com/inspircd)
* Support IRC channel &mdash; \#inspircd on irc.inspircd.org
* Development IRC channel &mdash; \#inspircd.dev on irc.inspircd.org
* InspIRCd test network &mdash; testnet.inspircd.org
