
FROM --platform=amd64 debian as base
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

FROM base as limine
# WORKDIR /limine
# RUN git clone https://github.com/limine-bootloader/limine.git --branch=v8.x . && \
#     ./bootstrap  && \
#     ./configure --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd  && \
#     make  && \
#     curl -Lo bin/limine.h https://github.com/limine-bootloader/limine/raw/trunk/limine.h
# RUN git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary limine/bin
ADD . /mnt