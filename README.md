# MQTT LED stripe controller
## Overview
Instead of building a LED stripe controller by myself, which is not allways the cheapest and fastest solution, I decided to use a commercial controller. Focus was then more on the software than on the hardware design. The choice fell on the `Luminea ZX-2842` controller, which is cheap, easy to order, easy to open and the contained ESP8266 can easily reflashed with the available test pads on the PCB. Also getting rid of the original firmware, which makes use of a Chinese MQTT cloud, is a good feeling. Instead, my software connects to a local self maintained MQTT broker (see other project from me).

## Hardware
The hardware is the commercial product `Luminea ZX-2842`. It is sold by PEARL (https://www.pearl.de/a-ZX2842-3103.shtml) and Amazon (https://www.amazon.de/dp/B074SKRNLW/ref=cm_sw_em_r_mt_dp_ut0aGb4T5BBRB?_encoding=UTF8&psc=1) .
![ZX2842](/ZX2842.png)

There is an ESP8266 sitting on the backside. 2 PWM outputs are connected to drive the 2 output channels (Warm White + Cold White). Cold White is on GPIO12, Warm White is on GPIO0. The pushbutton is on GPIO13 and is assigned by my software to reset the Wifi settings.

## Serial connection
The serial header (3.3V, RXD, TXD, GND) as well as GPIO0, GPIO2 and RESET (IO0, IO2, RST) are populated as test pads on the frontside of the PCB. You can easily add some solder to fix the wires for the flash process. You need to connect to the serial programming interface of the ESP8266 chip. This is done by connecting any serial-to-USB converter (e.g. the FT232R) TX, RX and GND pins to the ESP8266 RX, TX and GND pins (cross connection!) and powering the ZX-2842. Recheck your serial-to-USB converter so to ensure that it supplies 3.3V voltage and NOT 5V. 5V will damage the ESP chip!

## Flash mode
To place the board into flashing mode, you will need to short IO0 (GPIO0) to GND. This can remain shorted while flashing is in progress, but you will need to remove the short in order to boot afterwards the flashed software.

## Installation
1. install Arduino IDE 1.8.1x
2. download and install the ESP8266 board supporting libraries with this URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
3. select the `Lolin(Wemos) D1 mini Lite` board
4. install the `Async MQTT client` library: https://github.com/marvinroger/async-mqtt-client/archive/master.zip
5. install the `Async TCP` library: https://github.com/me-no-dev/ESPAsyncTCP/archive/master.zip
6. compile and flash

## SW configuration
The configuration is completely done in the web frontend of the WifiManager. At first startup, the software boots up in access point mode. In this mode you can configure parameters like
* Wifi SSID
* Wifi password
* MQTT broker IP address
* MQTT user
* MQTT password

After these settings were saved, with the next startup, the software boots into normal operating mode and connects to your Wifi and MQTT broker. Entering again into the WifiManager configuration menu can be done be holding the push button pressed during the startup of the software.

## SW normal operation
The software subsribes to MQTT topics, over which the 2 PWM values (Warm White + Cold White) of the ZX-2842 can be changed. Each changed PWM value is stored to EEPROM to be able to restore it after the next startup (e.g. after a power loss). Also the software supports re-connection to Wifi and to the MQTT broker in case of power loss, Wifi loss or MQTT broker unavailability. The MQTT topics begin with the device specific MAC-address string (in the following "A020A600F73A" as an example). This is useful when having multiple controllers in your MQTT cloud to avoid collisions.

Subscribe topics (used to control the PWM channels):
* Topic: "/A020A600F73A/whtw"   Payload: "0" - "255"
* Topic: "/A020A600F73A/whtc"   Payload: "0" - "255"

Publish topics (used to give feedback to the MQTTbroker about the PWM channels at startup):
* Topic: "/A020A600F73A/whtw_fb"   Payload: "0" - "255"
* Topic: "/A020A600F73A/whtc_fb"   Payload: "0" - "255"
