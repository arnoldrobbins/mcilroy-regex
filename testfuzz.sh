#!/bin/sh
# this test script builds and runs testre under 'american fuzzy lop'
# http://lcamtuf.coredump.cx/afl/

set -e

beginswith() { case $2 in "$1"*) true;; *) false;; esac; }

# download and build afl if it's not already in the path
AFL_CXX=afl-g++
if ! [ command -v ${AFL_CXX} > /dev/null ]; then
    AFL_VERSION=2.52b
    AFL_CXX=afl-${AFL_VERSION}/afl-g++
    AFL_CXX=afl-${AFL_VERSION}/afl-clang++
    AFL_FUZZ=afl-${AFL_VERSION}/afl-fuzz
    if ! [ -x ${AFL_CXX} ]; then
        # doesn't exist, download and build it
        if ! [ -f afl-${AFL_VERSION}.tgz ]; then
            wget http://lcamtuf.coredump.cx/afl/releases/afl-${AFL_VERSION}.tgz
        fi
        tar xzvf afl-${AFL_VERSION}.tgz
        cd afl-${AFL_VERSION} && make && cd ..
    fi
else
    AFL_CXX=afl-clang++
    AFL_CXX=afl-g++
    AFL_FUZZ=afl-fuzz
fi

# rebuild testre
make clean
CXX="$AFL_CXX" make testre

# chop up the data file into one-liners
mkdir -p fuzz-data
lineno=0
while read line || [[ -n "$line" ]]; do
    lineno=$(( lineno = lineno + 1 ))
    if [ "x$line" = "x" ] || beginswith "#" "$line"; then continue ; fi
    echo "$line" > fuzz-data/${lineno}.dat
done < testre.dat
pwd
$AFL_FUZZ -i fuzz-data -o fuzz-out ./testre
