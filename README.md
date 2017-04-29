# nmon - Monitor for Arduino like development boards.

I always wanted a 2-way program that could monitor a Arduino like device on my USB ports that would not go away when the device is reprogrammed ore reset. I tried many different programs, like Putty and screen(1), but none of them quite got the balance of features right. One of the most important things is that the program shouldn't exit just because the device temporarily disappears. It should just go into a loop polling for it to come back.

## Function:
* Echo characters from a device to stdout.
* Take characters from the keyboard and echo to the device.
* Don't quit if the device temporarily goes away.
* Support both devices and network connections.

Usage: nmon /dev/tty.usbmodemXXX
or
Usage: nmon 192.168.0.25 23

## Motivation:

As much as I enjoy developing on Particle's Photon and Core with OTA programming, one thing I've always hated is the fact that "screen" or "particle monitor" quits when the device is reprogrammed. This happens on other devices too, like Teensy or Arduino, when they are reset. This occurs because the USB device disconnects and the /dev/tty.usbmodemXXX device disappears, and is recreated later when the part restarts.

So, I wrote this tiny program to be robust to that, and just keep retrying to open the device if it goes away. The effect is you can keep a terminal window open at all times watching the output without having to touch it when reprogramming.

I've only ever tested this on Mac OSX, but it would probably work on Linux with some minor modifications, if someone wants to try it out.
