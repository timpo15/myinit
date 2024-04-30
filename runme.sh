#!/usr/bin/bash

cp config1 /tmp/myinit1.conf

rm -rf /tmp/myinit.logg

cmake .
make
./main_myinit /tmp/myinit1.conf
sleep 1
echo "check is 3 sleep after start\n"
ps aux | grep "sleep"
echo "killing one\n"
pkill -n sleep

sleep 2
echo "check is 3 sleep after start\n"

ps aux | grep "sleep"

cp config2 /tmp/myinit1.conf
echo "changing config\n"

pkill --signal SIGHUP main_myinit
echo "check is 1 sleep after restart\n"
sleep 1
ps aux | grep "sleep"
echo "logs:\n"

cat /tmp/myinit.logg

pkill --signal SIGTERM main_myinit

pkill -n sleep
pkill -n sleep
pkill -n sleep