#!/bin/sh
tail -n 1 $1
tmp=$(mktemp)
head -n -1 $1 > $tmp
mv $tmp $1
