# General Programming

Don't forget to call `standard_init`.

# UART debug cable

- Red wire -> don't connect!
- Black wire -> ground
- White wire -> TXD
- Green wire -> RXD

Command to connect: `picocom /dev/ttyUSB0 --parity n --baud 115200 --stopbits 1 --databits 8 --flow n --echo`

# AHT20 Sensor

- Datasheet: <https://cdn-shop.adafruit.com/product-files/5183/5193_DHT20.pdf>
- I2C Address: 0x38

# IRQs

Check out the armstub approach used by rockytriton/LLD. Might be the solution to getting interrupts working without having to disable the GIC.
