#!/bin/sh

for filename in /root/tests/*.app; do
    echo $filename
    sh -c "$filename /dev/cardev--1"
done
