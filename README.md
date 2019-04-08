# ftdiflash

SPI flash programmer for use with FTDI USB devices

The interfacing to the SPI bus has been optimized to achieve better performance.

Written and tested in Linux running on Virtual Box. No Windows testing has been done.

16MB Flash Memory has been used.

In order to build, the following are required:

libusb
libftdi

It is based on the following code:

See the guide here: https://learn.adafruit.com/programming-spi-flash-prom-with-an-ft232h-breakout/overview

This is a modified version of the iceprog tool from the excellent Icestorm FPGA toolchain by Clifford Wolf
https://github.com/cliffordwolf/icestorm


