FROM ubuntu:20.04
COPY files .
RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && apt-get upgrade -y && \
    apt-get install -y apt-utils gcc g++ build-essential dpkg-dev curl cmake nano python3 python3-pip && \
    ./xe-config_2021-04-09-1555.sh -y
RUN ln -s /usr/include/level_zero /usr/local/include/level_zero
