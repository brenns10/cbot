#!/bin/sh
podman build -t cbottest ./alpinetest/
exec podman run --rm -it \
    -v "$(pwd):$(pwd)" -w "$(pwd)" \
    --detach-keys "ctrl-z,z" \
    -e TZ=America/Los_Angeles \
    cbottest \
    /bin/sh -c "meson setup /tmp/build -Dwith_readline=true && ninja -C /tmp/build && /tmp/build/cbot $* || sh -i"
