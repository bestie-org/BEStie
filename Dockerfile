FROM fedora:44

USER root

RUN dnf clean all && dnf update -y && dnf install -y \
	make \
	arm-none-eabi-gcc-cs \
	arm-none-eabi-newlib \
	protobuf-compiler \
	python3 \
	python3-protobuf \
	protobuf \
	&& dnf clean all

WORKDIR /workdir

# Handle runtime edge-cases for numeric users (optional but highly recommended)
# This gives your host user a home directory target inside the container.
# Without this, some Python tools, GCC components, or Clang-Format
# may throw errors because $HOME defaults to / (which is write-protected).
ENV HOME=/tmp
