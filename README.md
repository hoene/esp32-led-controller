<div align="right">
<a href="https://symonics.com/">
	<img alt="Symonics MySofa" width="320px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/media-1.png"/>
</a>
</div

#

<div align="center">
<a href="https://travis-ci.com/github/hoene/esp32-led-controller">
<img alt="Travis CI Status" src="https://travis-ci.org/hoene/esp32-led-controller.svg?branch=master"/>
</a>
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=GUN8R6NUQCS3C&source=url">
<img src="https://www.paypalobjects.com/en_US/i/btn/btn_donate_SM.gif" alt="Donate with PayPal button" />
</a>
</div>

# LED Controller


This a software for the ESP32 chip, which can control multiple LED matrix displays. The LED displays must be based on smart LEDs such WS2813, GS8218, neopixels, or others. Up to eight serial data interfaces are supported.

The LED controller provides a web interface, available via ethernet, a wifi access point, or via wifi client. Via the web interface, the displays, colors format, and display order can be controlled.

A video streaming interface is available via UDP port 1616. The image format must be RTP/MJPEG.

Alternative, different testing patterns can be started on the web page of the LED controller.

## Required Hardware

Please use one of the following ESP32 hardware modules:
* Olimex ESP32-Gateway Version A to E [https://www.olimex.com/Products/IoT/ESP32/ESP32-GATEWAY/open-source-hardware], 2 LED lines are supported, which are GPIO16 and GPIO32
* Olimex ESP32-Gateway Version F to  [https://www.olimex.com/Products/IoT/ESP32/ESP32-GATEWAY/open-source-hardware], 4 LED lines are supported, which are GPIO4, GPIO12, GPIO13, and GPIO32

However, it should not be too difficult to add support other modules.

If you use 12V LEDs, you might need a TTL level converter to convert the output from a 3.3V level to higher, e.g. 5 or 12V.
* Sparkfun BOB-12009 [https://www.sparkfun.com/products/12009]
* Amazon 8 Channel Logic Level Converter [https://amzn.to/2KgCOyV] or [https://amzn.to/3mJUct2]

For example, you may setup your test hardware such as the following

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/setup.jpg"/>
</a>
</div


## Required Software

The ESP32 development environment is required. Download and install it from [https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/].

## Programming

To set up your ESP32 controller with this software, please the github repository with

> git clone https://github.com/hoene/esp32-led-controller

You may want to reconfigure the hardware. Please either copy an existing '''sdkconfig''' file, e.g.

> cd esp32-led-controller

> cp sdkconfig.OLIMEX_ESP32_EVB_B sdkconfig
    
or configure your particular hardware via

> idf.py menuconfig 
    
to the setting you need. In the menu CONTROLLER you find the settings needed.

You compile the LED controller software with

> idf.py build
    
Connect your ESP32 via USB to get a serial programming interface, which you need to flash the ESP32. To flash the ESP32 call

> idf.py flash

# Usage

Connect your PC the LED controller via Ethernet or Wifi. The default Wifi AP password is "controller".
Then, go to the web page, e.g. http://192.168.4.130/. You should see something like:

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/overview.png"/>
</a>
</div

In the LED Panel card, you can select the size and type of your LED displays.

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/ledpanel.png"/>
</a>
</div

The Color Control card allow to change brightness or color of all displays.

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/colorcontrol.png"/>
</a>
</div

With the Network card, you can change the wifi settings.

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/network.png"/>
</a>
</div

The administration is for maintainces:

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/administration.png"/>
</a>
</div

And finally, the Status card give you an overview on the entire system. Its is only for testing and maintaince. It provides you information about the ESP32 system.

<div align="center">
<a href="https://symonics.com/">
	<img width="640px" src="https://raw.githubusercontent.com/hoene/esp32-led-controller/master/doc/status.png"/>
</a>
</div


## Video Streaming

On Linux, to stream from your desktop to the LED controller call - after adapting the parameter - the command:

> ffmpeg -f x11grab -framerate 2 -i :0.0+0,0 -vf scale=32:32 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.130:1616/

Or if you want to play a movie, call

> ffmpeg filme.mp4 -framerate 2 -vf scale=16:8 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.130:1616/

## Disclaimer

The source code is by <christian.hoene@symonics.com>, <a href="https://symonics.com/">Symonics GmbH</a>, and available under AGPL V3 license. The inital work has been funded by German <a href="https://www.bmbf.de">Federal Ministry of Education and Research</a> within the Fastmusic project. 