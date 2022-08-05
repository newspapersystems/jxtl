# syntax=docker/dockerfile:1
FROM docker.io/library/almalinux:8 as deps
ENV container docker

# This file acts as both an isolated build solution
# and a series of instructions for anyone wanting to
# build this repository

RUN yum update && \
    yum install -y \
    epel-release automake autoconf libtool \
    git-core wget gcc make rpm-build \
    glibc-locale-source glibc-langpack-en.x86_64 \
    bison flex rpm python3 python3-devel \
    && yum install -y ruby rubygems ruby-devel \
       apr apr-util-devel swig && \
    yum clean all && \
    gem install fpm -v 1.11.0 && \
    pip3 install wheel && \
    rm -rf /var/cache/yum

WORKDIR /root/jxtl

ADD . .

RUN ./autogen.sh && mkdir -p packages
RUN make rpm && cp *.rpm packages/
RUN make install
RUN make python-rpm && cp bindings/python/dist/*.whl packages/

RUN yum install -y packages/*.x86_64.rpm && yum clean all && rm -rf /var/cache/yum \
    && pip3 -vvvv install packages/*.whl && python3 -c "import libjxtl; print(libjxtl.Template())"
