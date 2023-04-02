#/usr/bin/env bash

profile=release

mellow build --profile $profile

echo "Running test 1"

time ./build/$profile/blackbit/blackbit xboard < blackbit/tests/test1.in > blackbit/tests/test1.out

echo "Running test 2"

time ./build/$profile/blackbit/blackbit xboard < blackbit/tests/test2.in > blackbit/tests/test2.out

