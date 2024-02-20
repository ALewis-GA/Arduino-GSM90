/*****************************************************************
// Serial Comms to GSM90 magnetometer for std absolute observations
// assumes: Arduino Uno R3 or R4 (reference to serial port depends on board)
//        : LCD display 2 line, 16 character, 6 button;
//        : RS-232 shield (must be enabled with on-board switch)
// Without RS-232 shield (or disabled RS-232 shield) all serial comms are 
// directed to Arduino and can be viewed from the "serial monitor" in the 
// Arduino IDE for checking/debugging.
// Menu control via LCD buttons to adjust Baud_rate, magnetometer_tune_value
// (in microTesla) and number_of_obs_per_run. There is a hidden menu to adjust
// LCD backlight (left arrow button) The current value of these three 
// variables are stored to persistent EEPROM memory whenever
// they are updated and are used after reboot/power cycle.
// RS-232 parity, data_bits, stop_bits are hardwired into code as N81
// Magnetometer sampling interval is hardwired
// Some routines from https://github.com/harleo/arduino-lcd-menu
// Version 1.5 2022-08-22
// Version 1.6 2023-12-11  Update time outs for older model GSM90
//                         Include serial buffer flush
// ** When using UNO R4 must use "Serial1" object ( not "Serial" as for UNO R3) **
// Andrew Lewis
******************************************************************/
#include <LiquidCrystal.h>
#include <EEPROM.h>

// Set software version number
char Version[5] = "1.6";

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//LCD Keypad buttons analogue output level on analog pin 0
#define BTN_UP 200
#define BTN_DOWN 400
#define BTN_LEFT 600
#define BTN_RIGHT 60
#define BTN_ENTER 800

//Observatory specific parameters. 
char WelcomeString[16] = "GA Geomag";

// faster baud rates (eg 38400) has implications for signed "int" data type in this code
// Should change to unsigned int (and also LCD display function "printInt")
int baudSelect[5] = {300, 1200, 4800, 9600, 19200};

// If these default values are altered during run-time
// they are are stored in EEPROM and the updated values used
// for subsequent runs after reset/power cycle
int tune = 50;
int repeats = 8;
int baudIndex = 3;  // default baudrate = 9600

int i = 0;
int button_type;
int short_delay = 50;
int button_delay = 250;
int display_delay_short = 1000;
int display_delay_long = 4000;

// GSM90F_timeout must be at least 3.5 seconds; set the sum of the two GSM90* values to give desired sampling rate
// trial and error provided the values below for a 10 s sampling rate
int GSM90F_sampling_delay = 3000;
int GSM90F_timeout = 4000;
int LightLevel = 100;

//Define custom characters for LCD display
byte chr_arrowleft[8] = {
  0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00, 0x00
};
byte chr_arrowright[8] = {
  0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00
};
byte chr_arrowup[8] = {
  0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00, 0x00
};
byte chr_arrowdown[8] = {
  0x00, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00, 0x00
};
byte chr_enter[8] = {
  0x00, 0x01, 0x01, 0x05, 0x09, 0x1F, 0x08, 0x04
};

///////////////////////////////////////////////////////////////////
//  Clears a desired display line
//  int line: Line to clear
void clearLine(int line) {
  lcd.setCursor(0, line);
  lcd.print("                ");
}
//////////////////////////////////////////////////////////////////
//Initializes custom characters
void initChars() {
  lcd.createChar(0, chr_arrowleft);
  lcd.createChar(1, chr_arrowright);
  lcd.createChar(2, chr_arrowup);
  lcd.createChar(3, chr_arrowdown);
  lcd.createChar(4, chr_enter);
}
/////////////////////////////////////////////////////////////////////
//Initialize the display
void initLCD() {
  lcd.clear();
  initChars();
  lcd.begin(16, 2);
// Set the backlight level on analog pin 10; 0 = black; 250 = 100%
  analogWrite(10,LightLevel);
// switch off Arduino on-board LED  
  pinMode(13, OUTPUT);
  digitalWrite(13,LOW);  
  
  printStr(WelcomeString,0,0); printStr("V",11,0); printStr(Version,12,0);
// display start-up parameters  
  printStr("T",0,1); printInt(tune,1,1);
  printStr("#",4,1); printInt(repeats,5,1);
  printStr("B",8,1); printInt(baudSelect[baudIndex],9,1);
  delay(display_delay_long);
}
////////////////////////////////////////////////////////////////
// Read parameters from EEPROM if they are available
// address 0 = # (ascii 22)  indicates tune value in EEPROM(1)is valid data
//         1 = tune value (20 - 90)
//         2 = # indicates repeats value in EEPROM(3) is valid data
//         3 = repeats value (1 - 100)
//         4 = # indicates baud rate index in EEPROM(5) is valid data
//         5 = baud rate index (0 - 4) 
void readEEPROM() {
  byte eepromOK = 45; 
  byte eepromValue;
  eepromOK = EEPROM.read(0);
  if (eepromOK == 22) {
    eepromValue = EEPROM.read(1);
    tune = int(eepromValue);
    eepromOK = 45;
  }
  eepromOK = EEPROM.read(2);
  if (eepromOK == 22) {
    eepromValue = EEPROM.read(3);
    repeats =int(eepromValue);
    eepromOK = 45;
  }
  eepromOK = EEPROM.read(4);
  if (eepromOK == 22) {
    eepromValue = EEPROM.read(5);
    baudIndex = int(eepromValue);
    eepromOK = 45;
  }
}
////////////////////////////////////////////////////////////////
// Prints text to LCD display
// Will wrap to second display line
// if text is bigger than 16 chars.
// Doesn't work for text with more
// than 32 chars (lcd resets).
// string text: Text to print
// int index: Index at which to print text
// int line:  Line at which to print text
void printStr(String text, int index, int line) {
  if(text.length() > 16) {
    lcd.setCursor(0, 0);
    lcd.print(text);
    lcd.setCursor(0, 1);
    lcd.print(text.substring(17, text.length()));
  }
  else {
    lcd.setCursor(index, line);
    lcd.print(text);
  }
}
//////////////////////////////////////////////////////////////
//  Prints integer to LCD display
//  int value: integer to print
//  int index: LCD Index at which to print integer
//  int line:  LCD Line at which to print inteer
void printInt(int value, int index, int line) {
    lcd.setCursor(index, line);
    lcd.print(value);
}
///////////////////////////////////////////////////////////////
//  Prints custom character to LCD display
//  character: int, custom character code to print
//  int index: LCD Index at which to print character
//  int line:  LCD Line at which to print character
void printChr(int character, int index, int line) {
  lcd.setCursor(index, line);
  lcd.write(character);
}
//////////////////////////////////////////////////////////////
// Adjust serial comms baud rate from menu selection
void baudrate() {
  byte eepromValue;
  clearLine(0); clearLine(1);
  printStr("Baud   Up   Down",0,0);
  printChr(2,5,0); printChr(3,10,0); 
  
// Roll-over for baud-rate array requires different treatment than other roll-overs to prevent index over-run
    while (1) {
      clearLine(1); 
      printStr("Select",10,1); printChr(4,8,1);      
      printInt(baudSelect[baudIndex],0,1);
      delay(short_delay); 
      button_type = analogRead(0);      
 //Up Button    
      if ( (button_type < BTN_UP) && (button_type > BTN_RIGHT) ){
        baudIndex++;
        if ( baudIndex > 4) baudIndex = 0;
        delay(button_delay); 
     }
// down button
     else if ( (button_type < BTN_DOWN) && (button_type > BTN_UP) ){
        baudIndex--;
        if ( baudIndex  < 0)  baudIndex = 4;
        delay(button_delay); 
     }
//select button pushed  - save data to EEPROM
     else if ( ( button_type < BTN_ENTER ) && (button_type > BTN_LEFT) ){
        EEPROM.update(4,22);  
        eepromValue=byte(baudIndex);   
        EEPROM.update(5,eepromValue);     
        break;
     }
  }
}
//////////////////////////////////////////////////////////////////
// Adjust default GSM90 tuning parameter from menu selection
void tuneMag() {
  byte eepromValue;
  clearLine(0); clearLine(1);
  printStr("Tune   Up   Down",0,0);
  printChr(2,5,0); printChr(3,10,0); 
  printStr("Select",10,1);  printChr(4,8,1);  
  printInt(tune,0,1);
  delay(display_delay_short);
  
  while (1) {
     if ( tune < 20)  tune = 90;
     if ( tune > 90)  tune = 20;
     clearLine(1); 
     printStr("Select",10,1); printChr(4,8,1);  
     printInt(tune,0,1);
     delay(short_delay);
     
     button_type = analogRead(0);      
// Up button pushed      
     if ( (button_type < BTN_UP) && (button_type > BTN_RIGHT) ){
        tune++;
        delay(button_delay); 
     }
// down button pushed      
     else if ( (button_type < BTN_DOWN) && (button_type > BTN_UP) ){
        tune--;
        delay(button_delay); 
     }
//select button pushed: save to EEPROM
     else if ( ( button_type < BTN_ENTER ) && (button_type > BTN_LEFT) ){
        EEPROM.update(0,22);  
        eepromValue = byte(tune);   
        EEPROM.update(1,eepromValue);     
        break;
     }
  }
}
/////////////////////////////////////////////////////////////////////
// Adjust number of repeated obs from menu selection
void obs() {
  byte eepromValue;
  clearLine(0); clearLine(1);
  printStr("#Obs   Up   Down",0,0);
  printChr(2,5,0); printChr(3,10,0); 
  printStr("Select",10,1); printChr(4,8,1);  
  printInt(repeats,0,1);
  delay(display_delay_short);

  while (1) {
     if ( repeats < 1) repeats = 100;
     if ( repeats > 100) repeats = 1;
     clearLine(1); 
     printStr("Select",10,1); printChr(4,8,1);  
     printInt(repeats,0,1);
     delay(short_delay); 

     button_type = analogRead(0);      
 // Up button pushed   
     if ( (button_type < BTN_UP) && (button_type > BTN_RIGHT) ){
       repeats++;
       delay(button_delay); 
     }
// down button pushed      
     else if ( (button_type < BTN_DOWN) && (button_type > BTN_UP) ){
       repeats--;
       delay(button_delay); 
     }
//select button pushed  - save to EEPROM 
     else if ( ( button_type < BTN_ENTER ) && (button_type > BTN_LEFT) ){
       EEPROM.update(2,22);  
       eepromValue = byte(repeats);  
       EEPROM.update(3,eepromValue);     
       break;
     }
  }
}
/////////////////////////////////////////////////////////////////////
// Adjust the display backlight from menu selection: This is a hidden menu item
void backlight() {
  clearLine(0); clearLine(1);
  printStr("LCD    Up   Down",0,0);
  printChr(2,5,0); printChr(3,10,0); 
  printStr("Select",10,1);   printChr(4,8,1);  
  printInt(LightLevel,0,1);
  delay(button_delay);

  while (1) {
     if ( LightLevel < 10)  LightLevel = 245;
     if ( LightLevel > 245) LightLevel = 10;
     clearLine(1); 
     printStr("Select",10,1);   printChr(4,8,1);  
     printInt(LightLevel,0,1);
     delay(short_delay);
     analogWrite(10,LightLevel);

     button_type = analogRead(0);      
 // Up button pushed      
     if ( (button_type < BTN_UP) && (button_type > BTN_RIGHT) ){
        LightLevel+=5;
        analogWrite(10,LightLevel);
        delay(button_delay); 
     }
// down button pushed      
     else if ( (button_type < BTN_DOWN) && (button_type > BTN_UP) ){
        LightLevel-=5;
        analogWrite(10,LightLevel);
        delay(button_delay); 
     }
//select button pushed
     else if ( ( button_type < BTN_ENTER ) && (button_type > BTN_LEFT) ){
        break;
     }
  }
}
////////////////////////////////////////////////////////////////////
// Serial communications with GSM90 magnetometer - the main reason for this sketch
void comms() {
  int FLen,k;
  String TuneResponse, TotalField;
  clearLine(0); clearLine(1);
// serial port needs to be set up here in case baud rate has been changed from menu
// data bits, parity, stop bits cannot be changed interactively
  Serial1.begin(baudSelect[baudIndex],SERIAL_8N1);
  Serial1.setTimeout(1000);
// wait for serial port to connect.  
  while (!Serial1) {
    delay(short_delay); 
  }
  clearLine(0);
  printStr("T",0,0); printInt(tune,1,0);  

// flush the serial buffer before tune command
  while (Serial1.available() > 0) {
    k = Serial1.read();
  }
// Initiate Tune command to the magnetometer  
  Serial1.print("T"); Serial1.println(tune);
  TuneResponse = Serial1.readString();
  if ( TuneResponse.length() < 2) {
    clearLine(1);
    printStr("Tuning timed out",0,1);
    delay(display_delay_short);
  }
  else  printStr(TuneResponse,0,1);

// flush the serial buffer after tune command
  while (Serial1.available() > 0) {
    k = Serial1.read();
  }
  delay(display_delay_long);

// Initiate field readings
  Serial1.setTimeout(GSM90F_timeout);
  for (i = 0; i < repeats; i++) {
    clearLine(0);
    printInt(i+1,0,0);   printStr("F ",5,0);
    clearLine(1);

// flush the serial buffer
  while (Serial1.available() > 0) {
    k = Serial1.read();
  }
    Serial1.print("F");
    TotalField = Serial1.readString();
    FLen=TotalField.length();
// dont display EOL chars at end of valid field readings    
    if ( FLen < 2) printStr("Field timed out",0,1);
    else printStr(TotalField.substring(0,FLen-1),0,1);
 // flush the serial buffer after "F" command
    while (Serial1.available() > 0) {
      k = Serial1.read();
    }
    delay(GSM90F_sampling_delay);
  }
}
///////////////////////////////////////////////////////////////////
// Display the main menu:
// enter Run;  right-arrow Baud; 
// up-arrow Tune; down-arrow Obs  
// hidden left-arrow menu item not displayed 
void printMenu() {
 clearLine(0); clearLine(1);
// Enter
 printChr(4,0,0); printStr("Run",2,0); 
// Right arrow
 printChr(1,7,0); printStr("Baud",9,0);
// Up arrow 
 printChr(2,0,1); printStr("Tune",2,1);
// Down arrow 
 printChr(3,7,1); printStr("#Obs",9,1);
 delay(button_delay);
}
////////////////////////////////////////////////////////////////////////
// Display menu and jump to appropriate action defined by button selection
void menu() {
  printMenu();
  if (button_type < BTN_RIGHT) {
    baudrate();   //Select baud rate for serial comms
    delay(button_delay); 
  }
  else if (button_type < BTN_UP) {
    tuneMag();  //Select total field tune value
    delay(button_delay);
  }
  else if (button_type < BTN_DOWN) {
    obs();        //Select number of GSM90 readings to take
    delay(button_delay);
  }
  else if (button_type < BTN_LEFT) {
    backlight();  // adjust LCD screen backlight brightness level
    delay(button_delay);
  }
  else if (button_type < BTN_ENTER) {
    comms();  //  send/recieve data to GSM90 magnetometer via serial port; display output  on LCD
    delay(button_delay);
  }  
}
/////////////////////////////////////////////////////
void setup() {
  readEEPROM();
  initLCD();
 }
////////////////////////////////////////////////////
void loop() {
  button_type = analogRead(0);
  menu();
}
