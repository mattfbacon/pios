# UART debug cable

- Red wire -> don't connect!
- Black wire -> ground
- White wire -> TXD
- Green wire -> RXD

Command to connect: `picocom /dev/ttyUSB0 --parity n --baud 115200 --stopbits 1 --databits 8 --flow n --echo`

This, including automatic restart on errors, is provided as `uart.sh`.
