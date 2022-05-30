#!/bin/sh
while [ ! -S /var/signald/signald.sock ]; do
    echo Waiting for signald
    sleep 0.1
done
cbot /home/cbot/config/cbot.cfg
