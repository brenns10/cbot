#!/bin/bash

HASH_FILE=hash.txt
CMD=~/repos/hashchain/hashchain

if ! [ -a "$HASH_FILE" ]; then
	echo -n 'Enter a starting password to start the hash chain: '
	read -s password
	echo
	$CMD create sha1 1000 "$password" > "$HASH_FILE"
	chmod 600 "$HASH_FILE"
fi

current_hash=$(./auth.sh "$HASH_FILE")

build/cbot \
	irc \
	--plugin-dir build \
	--name cbot \
	--hash "$current_hash" \
	# more args here
