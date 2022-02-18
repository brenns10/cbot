#!/bin/sh
/opt/signald/bin/signald --system-socket &
while [ ! -S /var/run/signald/signald.sock ]; do
    sleep 0.1
done
cbot /home/cbot/config/cbot.cfg
