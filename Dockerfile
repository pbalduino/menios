
FROM --platform=amd64 debian AS base
RUN apt-get update && \
    apt-get install \
      apt-utils \
      -y && \
    apt-get install \
      apt-utils \
      automake \
      bash \
      binutils \
      bison \
      coreutils \
      cppcheck \
      curl \
      flex \
      gcc \
      gcc-multilib \
      gdisk \
      git \
      grep \
      gzip \
      m4 \
      make \
      mtools \
      nasm \
      python3-pkg-resources \
      python3-pygments \
      sed \
      xorriso \
      -y

FROM base AS limine
WORKDIR /opt
ARG LIMINE_REPO="https://codeberg.org/Limine/Limine.git"
ARG LIMINE_BRANCH="v10.x-binary"
RUN set -eux; \
    git clone "${LIMINE_REPO}" --branch "${LIMINE_BRANCH}" --depth=1 limine || { \
      rm -rf limine; \
      git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1 limine; \
    }
WORKDIR /opt/limine
RUN make
ENV PATH="/opt/limine:${PATH}"
ADD . /mnt
