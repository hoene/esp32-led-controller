# LED Controller

This a software for the ESP32 to control a multiple LED matrix display using smart LEDs such WS2813, GS8218, and others

## Required Hardware

Please use one of the following ESP32 hardware modules:
* Olimex ESP32-Gateway Version A to E [https://www.olimex.com/Products/IoT/ESP32/ESP32-GATEWAY/open-source-hardware], 2 LED lines are supported, which are GPIO16 and GPIO32
* Olimex ESP32-Gateway Version F to  [https://www.olimex.com/Products/IoT/ESP32/ESP32-GATEWAY/open-source-hardware], 4 LED lines are supported, which are GPIO4, GPIO12, GPIO13, and GPIO32

However, it should not be too difficult to add support other modules.

If you use 12V LEDs, you might need a TTL level converter to convert the output from a 3.3V level to higher, e.g. 5 or 12V.
* Sparkfun BOB-12009 [https://www.sparkfun.com/products/12009]
* Amazon 8 Channel Logic Level Converter [https://amzn.to/2KgCOyV] or [https://amzn.to/3mJUct2]

## Required Software

The ESP32 development environment is required. Download and install it from [https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/].

# Usage

## Testing

Invalid data:

    echo "This is my data" > /dev/udp/192.168.4.136/1616
    
Cude data:

    echo "(324324.234234,234234423.234324,3242334.4)" > /dev/udp/192.168.4.136/1616
    
RTP data:

    echo -n -e '\x02\x00\x02\x01TIMESSRC' > /dev/udp/192.168.4.136/1616

    I (563775) #rtp.c : pt 0 seq 258 ts 1162692948
     1014  git commit -m "Added ffmpeg from https://github.com/hoene/FFmpeg"

## Video Streaming

hoene@dev1:~/esp/controller$ ~/Downloads/ffmpeg-4.0.2-64bit-static/ffmpeg -f x11grab -framerate 2 -i :0.0+0,0 -vf scale=32:32 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.127:1616/
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=No Name
c=IN IP4 192.168.4.133
t=0 0
a=tool:libavformat 58.12.100
m=video 1616 RTP/AVP 26
b=AS:200

/home/hoene/Downloads/ffmpeg-4.0.2-64bit-static/ffmpeg -f x11grab -framerate 2 -i :0.0+0,0 -vf scale=16:16 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.133:1616/

/home/hoene/Downloads/ffmpeg-4.0.2-64bit-static/ffmpeg /home/hoene/Downloads/TV-20181013-2354-3701.h264.mp4 -framerate 2 -vf scale=16:8 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.133:1616/

/home/hoene/Downloads/ffmpeg-4.0.2-64bit-static/ffmpeg -re -i /home/hoene/Downloads/TV-20181013-2354-3701.h264.mp4 -map 0:0 -r 5 -vf scale=16:8 -vcodec mjpeg -huffman 0 -pix_fmt yuvj420p -f rtp rtp://192.168.4.133:1616/

## Secure flashing (first time)

cd controller

# erase everything
make erase_flash

# make the bootloader
make -j bootloader


# make the flash program
make -j flash

