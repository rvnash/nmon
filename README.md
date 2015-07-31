# nmon
Echo characters from a serial device to stdout. Don't quit if the device temporarily goes away.

Usage: nmon /dev/cu.usbmodemXXX

As much as I enjoy developing on Particle's Photon and Core with OTA programming, one thing I've always hated is the fact that "screen" or "particle monitor" quits when the device is reprogrammed. This happens on other devices too, like Teensy or Arduino, when they are reset. This occurs because the USB device disconnects and the /dev/cu.usbmodemXXX device disappears, and is recreated later when the part restarts.

So, I wrote this tiny program to be robust to that, and just keep retrying to open the device if it goes away. The effect is you can keep a terminal window open at all times watching the output without having to touch it when reprogramming.

I've only ever tested this on OSX, but it would probably work on Linux with some minor modifications, if someone wants to try it out.
