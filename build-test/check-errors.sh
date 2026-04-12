#!/bin/bash
cd /tmp/sudosh2
docker run --rm -v "$PWD:/src:ro" ubuntu:24.04 bash <<'DOCKER'
apt-get update -qq > /dev/null 2>&1
apt-get install -y -qq gcc make autoconf automake libtool > /dev/null 2>&1
cp -r /src /tmp/b
cd /tmp/b
autoreconf -fi 2>&1 | tail -1
./configure --prefix=/usr --sysconfdir=/etc > /dev/null 2>&1
make CFLAGS='-Wall -Werror -pedantic' 2>&1 | grep -E 'error:|warning:|Error' | head -20
echo "---EXIT:$?---"
DOCKER
