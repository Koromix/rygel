#!/bin/sh -e

cd $(dirname $0)

echo "---------- Building RELAY ----------"
felix -pTeensyLC mael_relay
echo "---------- Building ROBOT ----------"
felix -pTeensy35 mael_robot

echo "---------- Uploading RELAY ----------"
tycmd upload -B 1126140 ../../bin/TeensyLC/mael_relay.hex
echo "---------- Uploading ROBOT ----------"
tycmd upload -B 8119060 ../../bin/Teensy35/mael_robot.hex
