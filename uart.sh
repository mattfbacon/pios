#!/bin/sh
while true; do
	picocom /dev/ttyUSB0 --baud 115200 --flow n --parity n --databits 8 --stopbits 1 --echo && break
	# continue if error, after waiting a bit
	sleep 1
done
