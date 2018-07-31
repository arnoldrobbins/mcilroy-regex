#!/bin/sh
# this test script builds and runs testre under 'american fuzzy lop'
# http://lcamtuf.coredump.cx/afl/

set -e

beginswith() { case $2 in "$1"*) true;; *) false;; esac; }

#: ${AFL_CXX=afl-g++}
: ${AFL_CXX=afl-clang++}
: ${AFL_FUZZ=afl-fuzz}
: ${AFL_TMIN=afl-tmin}

# download and build afl if it's not already in the path
if ! command -v "${AFL_CXX}" > /dev/null ; then
    AFL_VERSION=2.52b
    AFL_CXX=./afl-${AFL_VERSION}/${AFL_CXX}
    AFL_FUZZ=./afl-${AFL_VERSION}/${AFL_FUZZ}
    AFL_TMIN=./afl-${AFL_VERSION}/${AFL_TMIN}
    if ! [ -x "${AFL_CXX}" ]; then
        # doesn't exist, download and build it
        if ! [ -f afl-${AFL_VERSION}.tgz ]; then
            wget http://lcamtuf.coredump.cx/afl/releases/afl-${AFL_VERSION}.tgz
        fi
        tar xzvf afl-${AFL_VERSION}.tgz
        cd afl-${AFL_VERSION} && make && cd ..
    fi
fi

# rebuild testre
make clean
CXX="$AFL_CXX" make testre

# chop up the data file into one-liners
mkdir -p fuzz-data
lineno=0
while read line || [ -n "$line" ]; do
    lineno=$(( lineno = lineno + 1 ))
    if [ "x$line" = "x" ] || beginswith "#" "$line"; then continue ; fi
    echo "$line" > fuzz-data/${lineno}.dat
done < testre.dat

# go ahead and fuzz.  crash inputs in fuzz-out/crashes/id*
echo "to run additional parallel slave fuzzers in other terminals:"
echo "cd \"$(pwd)\" && $AFL_FUZZ -i fuzz-data -o fuzz-out -S fuzzer01 ./testre"
echo "cd \"$(pwd)\" && $AFL_FUZZ -i fuzz-data -o fuzz-out -S fuzzer02 ./testre"
echo "...and so on..."

set +e
"$AFL_FUZZ" -i fuzz-data -o fuzz-out -M fuzzer00 ./testre

# when finished/bored, minimize the crash cases:
echo "minimizing crash cases..."
mkdir -p fuzz-out-min
set -e
for f in fuzz-out/fuzzer*/crashes/id*; do
    "$AFL_TMIN" -i "$f" -o "fuzz-out-min/$(basename "$f").min" -- ./testre
done
