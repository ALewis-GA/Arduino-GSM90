#Arduino-GSM90

Arduino Uno R3 sketches to handle simple serial comms
to a GEM GSM90 magnetometer for absolute observations with either LCD screen + buttons or TFT touch screen

Hardware Requirements:

Arduino Uno R3 (or compatible)  
DFRobot RS232 Shield (Core electronics DFR0258)  

GSM90Serial (LCD + button):  Duinotech 2 x 16 LCD 6 button shield (JayCar XC-4454) or  
GSM90-Touch: 2.8" 240x 320 TFT LCD Touch screen display (JayCar DuinoTech XC-4630; TFT ID 0x9595)  

The TFT Touch Screen is less magnetic than the 2 x 16 LCD, so Touch screen is preferred hardware.

Software built on Arduino Development System V 1.8.19 using  

GSM90-Touch:  
Adafruit_GFX; TouchScreen library; MCUFriend-kbv; EEPROM libraries  

GSM90Serial:  
LiquidCrystal and EEPROM libraries  

Andrew Lewis  
2023-04


