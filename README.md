# PixelMatrixFun-ESP32
 PixelMatrixFun ESP32 module code
 
![All pieces connected and running](PixelMatrixFun.png?raw=true?raw=true "The blue round thing is just an object to capture to show everything working")

PixelMatrixFun is a wearable RGB LED matrix just for fun. And it's really cheap and easy to make!

I made it with ESP-32 because it's BLE (Bluetooth Low Energy) capabilities.

It's just a prototype. A lot of things should be fixed and improved.

It receive images wirelessly with BLE. I made an Android app that right now is just a prototype too.
https://github.com/aalku/PixelMatrixFun-Android

The current code is only ready for this led matrix but it's easy to adapt it to any other. I'll help you make the adaptation if you need it and I will accept any reasonable pull request.

If you want to build it you just need the LED matrix, an ESP-32 and a resistor.
I am using a 100 Ohm resistor but I think it should be arround 300 Ohm. Not sure.
Connect the battery to the ESP-32, the power to the matrix, and the data pin (13) to the input data wire through the resistor.

Do not rise the brightness much (it's currently 10 over 255) because the LEDs are too bright for this project and will drain a lot more of current than the ESP-32 can provide so you would need a different power circuit.

Pieces:
* 16x16 LED matrix:
  * https://s.click.aliexpress.com/e/_ANO8Zd
* ESP-32
  * Mine is a "WIFI KIT-32". You can use any ESP-32 board with 5V output pin. You don't need the OLED screen but it has LiPo battery support that is a nice alternative to power bank power supply. I didn't use it yet.
  * https://s.click.aliexpress.com/e/_A2VVlh
* Power Bank 
  *	https://s.click.aliexpress.com/e/_Al3NjN
  * The linked power bank is not the same as mine.
* 300 Ohm Resistor
  *	https://s.click.aliexpress.com/e/_AmBntv
  
I said this project is a prototype and I don't think I will continue it much further for now. But here it is for the case you find it useful or nice. :)
