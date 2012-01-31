#!/bin/bash

sed -e '
N
N
N

s/0011\n1011\n0010\n0000/aabb\nccdd\nmmmm\nnnnn/; tend
s/0011\n1011\n0010\n0001/aabb\nccdd\nmmmm\nnnnn/; tend
s/aabb\nccdd\nmmmm\nnnnn/xxxx/; tend

:end;
' field.txt
