#!/bin/bash
IP_ADDR=$(adb shell ip addr show wlan0 | grep "inet " | awk '{print $2}' | cut -d '/' -f 1-1)

adb tcpip 5555
echo
echo "Press enter."
read YORNO
echo "Will connect to $IP_ADDR"
adb connect $IP_ADDR
echo
echo "Press enter."
read YORNO
echo
adb devices
