EAPI=8

inherit cmake

KEYWORDS="alpha amd64 arm arm64 hppa ia64 m68k ppc ppc64 s390 sh sparc x86"

SLOT=0

DEPEND="
  >=dev-libs/utfcpp-3.2.1
  >=sys-libs/libxcrypt-4.4.28-r2
  >=dev-libs/libpcre-8.45-r1
  >=dev-libs/libpcre2-10.42-r1
  >=dev-libs/re2-0.2022.12.01
  >=app-crypt/argon2-3.0
  >=net-libs/http-parser-2.9.4-r2
  >=dev-libs/libmaxminddb-1.7.1
  >=dev-libs/openssl-1.1.1t
  >=media-gfx/qrencode-4.1.1
  >=net-libs/mbedtls-2.28.1
  >=net-libs/gnutls-3.7.8
  >=dev-db/sqlite-3.40.1
  >=net-nds/openldap-2.6.3-r7
  >=dev-db/postgresql-14.5
  >=dev-db/mysql-connector-c-8.0.31
"

CMAKE_MAKEFILE_GENERATOR=emake

src_unpack() {
        mkdir clandestine-9999/
	cp -rvp /mnt/. clandestine-9999/
}


src_configure() {
        cmake_src_configure
}
