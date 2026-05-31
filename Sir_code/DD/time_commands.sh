#!/bin/bash

make
sudo insmod timer_lab_latest.ko
ls -l /dev/timer_lab
sudo chmod 666 /dev/timer_lab

cat /dev/timer_lab

echo "start 1000" | sudo tee /dev/timer_lab
sleep 5
cat /dev/timer_lab

echo "period 500" | sudo tee /dev/timer_lab
sleep 3
cat /dev/timer_lab

echo "stop" | sudo tee /dev/timer_lab
cat /dev/timer_lab

echo "reset" | sudo tee /dev/timer_lab
cat /dev/timer_lab

echo "oneshot 2000" | sudo tee /dev/timer_lab
sleep 3
cat /dev/timer_lab

sudo rmmod timer_lab_latest
make clean
