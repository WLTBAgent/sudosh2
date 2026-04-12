#!/bin/bash
set -euo pipefail
cd /tmp/sudosh2

test_distro() {
    local image="$1"
    local pkg_cmd="$2"
    local name="$3"
    echo "=== Testing: $name ==="
    docker run --rm -v "$PWD:/src:ro" "$image" bash -c "
        set -e
        $pkg_cmd > /dev/null 2>&1
        cp -r /src /tmp/b && cd /tmp/b
        autoreconf -fi 2>&1 | tail -1
        ./configure --prefix=/usr --sysconfdir=/etc > /dev/null 2>&1
        make CFLAGS='-Wall -Werror -pedantic' 2>&1 | tail -3
        echo BUILD_SUCCESS
    " 2>&1 | tail -5
    echo ""
}

# Test strict mode (Gentoo sim)
echo "=== Testing: Gentoo sim (strict flags) ==="
docker run --rm -v "$PWD:/src:ro" ubuntu:24.04 bash -c "
    set -e
    apt-get update -qq > /dev/null 2>&1
    apt-get install -y -qq gcc make autoconf automake libtool > /dev/null 2>&1
    cp -r /src /tmp/b && cd /tmp/b
    autoreconf -fi 2>&1 | tail -1
    ./configure --prefix=/usr --sysconfdir=/etc > /dev/null 2>&1
    make CFLAGS='-Wall -Werror -pedantic -Werror=implicit-function-declaration -Werror=int-conversion -std=c17 -O2' 2>&1 | tail -5
    echo BUILD_SUCCESS
" 2>&1 | tail -8
echo ""
