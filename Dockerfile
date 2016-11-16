FROM ubuntu:16.04

MAINTAINER mathias.bylund@kreativet.org

RUN apt-get update && apt-get install -y \
    build-essential \
    libreadline6-dev

VOLUME /home/lab


