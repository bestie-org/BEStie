#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

IMAGE_NAME="bestie-dev-container"
DOCKERFILE="$SCRIPT_DIR/Dockerfile"
CACHE_MARKER="$SCRIPT_DIR/.docker_build_timestamp"

if [ ! -f "$DOCKERFILE" ]; then
	echo "Error: $DOCKERFILE not found in $SCRIPT_DIR" >&2
	exit 1
fi

SHOULD_BUILD=0
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
	SHOULD_BUILD=1
elif [ ! -f "$CACHE_MARKER" ] || [ "$DOCKERFILE" -nt "$CACHE_MARKER" ]; then
	SHOULD_BUILD=1
fi

if [ "$SHOULD_BUILD" -eq 1 ]; then
	echo "Dockerfile changed or image missing. Building container..."

	docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"
	touch "$CACHE_MARKER"
else
	echo "Using cached docker image."
fi

if [ $# -eq 0 ]; then
	CONTAINER_CMD="make -j"
else
	CONTAINER_CMD="$*"
fi

HOST_UID=$(id -u)
HOST_GID=$(id -g)

echo "Executing inside container as User $HOST_UID:$HOST_GID: $CONTAINER_CMD"

docker run --rm \
	--user "$HOST_UID:$HOST_GID" \
	-v "$SCRIPT_DIR":/workdir \
	-w /workdir \
	"$IMAGE_NAME" \
	sh -c "$CONTAINER_CMD"
