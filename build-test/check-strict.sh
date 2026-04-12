#!/bin/bash
cd /tmp/sudosh2
echo "=== Strict gcc-15 simulation ==="
docker run --rm -v "$PWD:/src:ro" ubuntu:24.04 bash -c '
apt-get update -qq > /dev/null 2>&1
apt-get install -y -qq gcc make autoconf automake libtool > /dev/null 2>&1
cp -r /src /tmp/b
cd /tmp/b
autoreconf -fi 2>&1 | tail -1
./configure --prefix=/usr --sysconfdir=/etc > /dev/null 2>&1
echo "--- MAKE OUTPUT ---"
make CFLAGS="-Werror=implicit-function-declaration -Werror=int-conversion -std=c17 -O2" 2>&1
echo "--- EXIT: $? ---"
'
