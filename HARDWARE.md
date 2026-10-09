These docs are work in progress! Currently writing out the simple aspects here, will need to do a further pass clarifying things.

# Making your own transmitter
The main important part of your own transmitter is an ESP32 that supports native USB. You only need to get one of these if you are either running an event yourself, or want to test your own receiver implementations

### Recommended: ESP32-S3
Has a external ESP-NOW antenna, and supports native USB. Those are the only two requirements really
https://a.co/d/020YqeTL

## Running an event
If you are planning on running an event, things get more complicated. Currently not covering the whole process here (but plan to at some point! reach out if you are curious!), but along with having a transmitter ESP32, you also will want a mini computer. a fanless POS type computer works well enough, as long as it has a USB port and a mic jack, and can run regular x86 linux (not arm!), then it will work well. Currently used is a Qotom Q305P

## Creating an AL Tablet
This also requires additional setup on the transmitter PC, but its possible to have the audiolink tablet as a event runner. Currently, a Microsoft Surface Go 1st Gen tablet is used for the display.

# Making a receiver
There is three main aspects to a receiver device. Those being battery, ESP32, and lights
## ESP32
Almost any ESP32 will work, as long as it can receive ESP-NOW broadcast packets (which are most modern ESP32 boards). There are two types:
### On-Board Antenna
These are the most common, and feature a on-board antenna. While these are cheaper and more compact, they receive signals significantly worse and are highly not recommended except for testing or if you are on a big budget. An example of these is this https://a.co/d/07FVEDM8
### Off-Board Antenna
These are the ones you should be looking to purchase. These feature an off-board antenna. which has significantly more strength to receive data and will cause significantly less packet loss, especially at further distances or through bodies/walls. An example of this is these https://a.co/d/09AUEV2m

## Batteries
Currently all I have tested with is using USB batteries. These are actually fairly effective, you can pickup cheap portable phone charger batteries from say five below that are 5000mAH, and due to the amperage draw will last 7 ish hours, just using the USB cable. Just be careful with amperage draw, you will need to create a more complicated setup if you start drawing too many amps through the ESP32 itself.

## LEDs
Virtually any neopixel-like / single-wire LED strip will work currently. You can even set them up in a serpentine matrix for 2D displays. Bear in mind, cheaper addressable strips have worse shift register timing, which can lead to severe color flickering happening, especially on longer runs.

HUB75 Matrices are also supported. One note, is that currently due to DMA costs, the current maximum that has been acheived is a 128x64 matrix arrangement, and with severe downsides such as reduced update rate of the matrix itself and a data rate of roughly 4 FPS, as the DMA process of updating the screen causes WiFi to drop packets. A longer term solution to this would be to have one ESP32 receive and process the audiolink data, while a seperate ESP32 sent data over serial handled the actual matrix rendering
