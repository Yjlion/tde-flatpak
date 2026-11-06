# Debian Trixie build environment for tdeflatpak (TDE R14.1.x)
FROM debian:trixie

ARG DEBIAN_FRONTEND=noninteractive
ARG TDE_MIRROR=http://mirror.ppa.trinitydesktop.org/trinity/deb/trinity-r14.1.x

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl \
 && curl -fsSL -o /tmp/trinity-keyring.deb \
      http://mirror.ppa.trinitydesktop.org/trinity/deb/trinity-keyring.deb \
 && dpkg -i /tmp/trinity-keyring.deb \
 && rm /tmp/trinity-keyring.deb \
 && echo "deb ${TDE_MIRROR} trixie main deps" > /etc/apt/sources.list.d/trinity.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential devscripts debhelper fakeroot lintian \
      cmake pkg-config file \
      tde-cmake tdelibs14-trinity-dev libtqt3-mt-dev tqt3-dev-tools libtqtinterface-dev \
      libflatpak-dev libglib2.0-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
