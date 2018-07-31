FROM ubuntu:16.04

# Install the required dependencies.
RUN apt-get update --assume-yes
RUN apt-get install --assume-yes --no-install-recommends \
	clang-3.8 \
	g++-5  \
	git \
	make \
	libc++-dev \
	pkg-config
RUN apt-get install --assume-yes --no-install-recommends \
	libgeoip-dev \
	libgnutls-dev \
	libldap2-dev \
	libmbedtls-dev \
	libmysqlclient-dev \
	libpcre3-dev \
	libpq-dev \
	libre2-dev \
	libsqlite3-dev \
	libssl-dev \
	libtre-dev

# Add a group/user to run the build as.
RUN groupadd --gid @GID@ -- travis
RUN useradd --create-home --gid @GID@ --uid @UID@ -- travis
USER @UID@:@GID@
