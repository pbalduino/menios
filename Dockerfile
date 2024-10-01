FROM --platform=amd64 dotdot0/zig:0.13.0 as base
RUN apt-get update && \
    apt-get install \
#       automake \
#       bash \
#       binutils \
#       bison \
#       coreutils \
#       curl \
#       flex \
#       gcc \
#       gcc-multilib \
      gdisk \
#       git \
#       grep \
#       gzip \
#       m4 \
      make \
      mtools \
#       nasm \
#       python3-pkg-resources \
#       python3-pygments \
#       sed \
      xorriso \
      -y

FROM base as menios

ADD . /mnt