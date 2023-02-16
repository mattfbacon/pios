#!/bin/sh
picocom /dev/ttyUSB0 --baud 115200 --flow n --parity n --databits 8 --stopbits 1 --echo
