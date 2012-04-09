#!/bin/bash
LD_PRELOAD=./libmyalloc.so ./test
LD_PRELOAD=./libmyalloc.so ls -la
LD_PRELOAD=./libmyalloc.so cat launch.sh
