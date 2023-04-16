/*****************************************************************
// Serial Comms to GSM90 magnetometer for std absolute observations with TFT touch screen display
// assumes: Arduino Uno R3 (JayCar  XC4410)
//        : TFT LCD TouchScreen 240 x 320  ID 0x9595 (Jaycar XC4630)
//        : RS-232 shield (must be enabled with on-board switch) (Core Electronics DFR0258)
//        : Adafruit_GFX library; TouchScreen library; EEPROM Library 
//        : MCUFriend-kbv library
//
// Without RS-232 shield (or disabled RS-232 shield) all serial comms are 
// directed to Arduino and can be viewed from the "serial monitor" in the 
// Arduino IDE for checking/debugging.
// Menu control via TouchScreem buttons to adjust Baud_rate, magnetometer_tune_value
// (in microTesla) and number_of_obs_per_run. The current value of these three 
// variables are stored to persistent EEPROM memory whenever
// they are updated and are used after reboot/power cycle.
// RS-232 parity, data_bits, stop_bits are hardwired into code as N81
// Magnetometer sampling interval is hardwired
// Version 1.1 2022-10-18 increase wait period at end of comms
// Version 1.3 2022-11-08 include serial buffer flush before/after "T" and after "F" command
// Version 1.4 2023-04-16 more info displayed to TFT screen, none displayed to serial monitor at start up
// Andrew Lewis
******************************************************************/


// Graphics library
#include <Adafruit_GFX.h>

// TFT touchscreen hardware library
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;

// touch screen library
#include <TouchScreen.h>

// Set software version number
#define _VERSION 1.4

// Touch screen pressure limits
#define MINPRESSURE 200
#define MAXPRESSURE 1000

// TFT screen colours
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Library to access EEPROM memory
#include <EEPROM.h>

// All Touch panels and wiring is DIFFERENT
// copy-paste results from MCUFriend/TouchScreen_Calibr_native.ino

//AML 2022-09-17 Screen fitted to Arduino UNO SN: 7513330333235150F071
    const int XP=8,XM=A2,YP=A3,YM=9; //240x320 ID=0x9595
    const int TS_LEFT=910,TS_RT=107,TS_TOP=84,TS_BOT=905;

// Define a TouchScreen object
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Touch buttons to display on the screen

// Main menu buttons
Adafruit_GFX_Button run_btn, baud_btn, tune_btn, obs_btn;
//Sub menu buttons
Adafruit_GFX_Button up_btn, down_btn, sel_btn;

// location of touch points
int pixel_x, pixel_y;     //Touch_getXY() updates global vars

//Observatory specific parameters 
int baudSelect[5] = {300, 1200, 4800, 9600, 19200};

// If these default values are altered during run-time
// they are are stored in EEPROM and the updated values used
// for subsequent runs after reset/power cycle
int tune = 50;
int repeats = 8;
int baudIndex = 3;  // default baudrate = 9600
int i = 0;

// GSM90F_timeout must be at least 3.5 seconds; set the sum of the two GSM90* values to give desired sampling rate
// trial and error provided the values below for a 10 s sampling rate
int GSM90F_sampling_delay = 3500, GSM90F_timeout = 3500, short_delay = 50;

//////////////////////////////////////////////////////////////////////////////////////////////
// get touch locations (AdaFruit)
bool Touch_getXY(void)
{
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT);      //restore shared pins
    pinMode(XM, OUTPUT);
    digitalWrite(YP, HIGH);   //because TFT control pins
    digitalWrite(XM, HIGH);
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed) {
        pixel_x = map(p.x, TS_LEFT, TS_RT, 0, tft.width()); //.kbv makes sense to me
        pixel_y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
    }
    return pressed;
}
////////////////////////////////////////////////////////////////
// Read obs parameters from EEPROM if they are available
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
//////////////////////////////////////////////////////////////
// Display the main menu selection buttons
void main_menu(void) {
  tft.fillScreen(BLUE);
  run_btn.drawButton(false);
  baud_btn.drawButton(false);
  tune_btn.drawButton(false);
  obs_btn.drawButton(false);
}
///////////////////////////////////////////////////////////////
// Display sub menu selection buttons  
void sub_menu(void) {
  tft.fillScreen(CYAN);
  sel_btn.drawButton(false);
  up_btn.drawButton(false);
  down_btn.drawButton(false);
  tft.drawLine(0,205,320,205,BLACK);
  tft.drawLine(0,207,320,207,BLACK);
}  
//////////////////////////////////////////////////////////////
// Adjust serial comms baud rate from menu selection
void baudrate() {
  byte eepromValue;
  sub_menu();
  
// Roll-over for baud-rate array requires different treatment than other roll-overs to prevent index over-run
  while (1) {
    tft.fillRect(50,220,150,100,CYAN);
    tft.setCursor( 80,250);
    tft.print(baudSelect[baudIndex]);
    tft.setCursor( 80, 300);
    tft.print("n-8-1");
    

// check for screen press
    bool down = Touch_getXY();
    up_btn.press(down && up_btn.contains(pixel_x, pixel_y));
    down_btn.press(down && down_btn.contains(pixel_x, pixel_y));
    sel_btn.press(down && sel_btn.contains(pixel_x, pixel_y));
    
    if (up_btn.justReleased())   up_btn.drawButton();
    if (down_btn.justReleased())   down_btn.drawButton();
    if (sel_btn.justReleased())   sel_btn.drawButton();
    
// Up Button pressed
    if ( up_btn.justPressed() ) {
        up_btn.drawButton(true);
        baudIndex++;
        if ( baudIndex > 4) baudIndex = 0;
    }      
// Down button pressed
     else if ( down_btn.justPressed() ) {
      down_btn.drawButton(true);
      baudIndex--;
      if ( baudIndex  < 0)  baudIndex = 4;
     }
//select button pushed  - save data to EEPROM
     else if ( sel_btn.justPressed() ) {
        sel_btn.drawButton(true);
        EEPROM.update(4,22);  
        eepromValue=byte(baudIndex);   
        EEPROM.update(5,eepromValue);   
// redraw the main menu
        main_menu();
        break;
     }
   delay(short_delay);
   }
}
//////////////////////////////////////////////////////////////
// Adjust the magnetometer tune value from menu selection
void tuneMag() {
  byte eepromValue;
  sub_menu();
  
  while (1) {
    if ( tune < 20)  tune = 90;
    if ( tune > 90)  tune = 20;
    tft.fillRect(50,220,150,100,CYAN);
    tft.setCursor( 100,250);
    tft.print(tune);

// check for screen press
    bool down = Touch_getXY();
    up_btn.press(down && up_btn.contains(pixel_x, pixel_y));
    down_btn.press(down && down_btn.contains(pixel_x, pixel_y));
    sel_btn.press(down && sel_btn.contains(pixel_x, pixel_y));
    
    if (up_btn.justReleased())   up_btn.drawButton();
    if (down_btn.justReleased())   down_btn.drawButton();
    if (sel_btn.justReleased())   sel_btn.drawButton();
    
// Up Button pressed
    if ( up_btn.justPressed() ) {
        up_btn.drawButton(true);
        tune++;
    }      
// Down button pressed
     else if ( down_btn.justPressed() ) {
      down_btn.drawButton(true);
      tune--;
     }
//select button pushed  - save data to EEPROM
     else if ( sel_btn.justPressed() ) {
        sel_btn.drawButton(true);
        EEPROM.update(0,22);  
        eepromValue=byte(tune);   
        EEPROM.update(1,eepromValue);   
// redraw the main menu
        main_menu();
        break;
     }
   delay(short_delay);
   }
}
//////////////////////////////////////////////////////////////
// Adjust the number of repeated observations from menu selection
void obs() {
  byte eepromValue;
  sub_menu();
  
  while (1) {
    if ( repeats  < 1)  repeats = 100;
    if ( repeats  > 100)  repeats = 1;
    tft.fillRect(50,220,150,100,CYAN);
    tft.setCursor( 100,250);
    tft.print(repeats);

// check for screen press
    bool down = Touch_getXY();
    up_btn.press(down && up_btn.contains(pixel_x, pixel_y));
    down_btn.press(down && down_btn.contains(pixel_x, pixel_y));
    sel_btn.press(down && sel_btn.contains(pixel_x, pixel_y));
    
    if (up_btn.justReleased())   up_btn.drawButton();
    if (down_btn.justReleased())   down_btn.drawButton();
    if (sel_btn.justReleased())   sel_btn.drawButton();
    
// Up Button pressed
    if ( up_btn.justPressed() ) {
        up_btn.drawButton(true);
        repeats++;
    }      
// Down button pressed
     else if ( down_btn.justPressed() ) {
      down_btn.drawButton(true);
      repeats--;
     }
//select button pushed  - save data to EEPROM
     else if ( sel_btn.justPressed() ) {
        sel_btn.drawButton(true);
        EEPROM.update(2,22);  
        eepromValue=byte(repeats);   
        EEPROM.update(3,eepromValue);   
// redraw the main menu
        main_menu();
        break;
     }
   delay(short_delay);
   }
}
//////////////////////////////////////////////////////////////////////////////
// Serial communications with GSM90 magnetometer - the main reason for this sketch
void comms() {
  int FLen,yloc,j,k;
  String TuneResponse, TotalField;
// serial port needs to be set up here in case baud rate has been changed from menu
// data bits, parity, stop bits cannot be changed interactively
  Serial.begin(baudSelect[baudIndex],SERIAL_8N1);
  Serial.setTimeout(1000);
// wait for serial port to connect.  
  while (!Serial) {
    delay(short_delay); 
  }
  
  tft.fillScreen(CYAN);
  tft.setTextSize(2);
  tft.setCursor(5,10);  tft.print("T");
  tft.setCursor(20,10);  tft.print(tune);
  
// Initiate Tune command to the magnetometer 

// Added 2022-11-08
// flush the serial buffer before tune command
  while (Serial.available() > 0) {
    k = Serial.read();
  }
 
  Serial.print("T"); Serial.println(tune,DEC);
  TuneResponse = Serial.readString();
  tft.setCursor(5,40);
  if ( TuneResponse.length() < 2) tft.print("Tuning timed out");
  else  tft.print(TuneResponse);

// added 2022-11-08
// flush the serial buffer after tune command
  while (Serial.available() > 0) {
    k = Serial.read();
    tft.print("X");
  }
  delay(4000);

// Initiate field readings
  Serial.setTimeout(GSM90F_timeout);
  for (i = 0; i < repeats; i++) {
// if there are more than 10 readings then clear the screen and re-start display from the top
    j = i % 10;
    if ( j == 0 ) tft.fillScreen(CYAN);
    yloc= 20 + j *30;
    tft.setTextSize(2);
    tft.setCursor(5,yloc);
    tft.print(i+1);  
    tft.setCursor(30,yloc);
    tft.print("F ");
    
// Added 2022-11-08
// flush the serial buffer
  while (Serial.available() > 0) {
    k = Serial.read();
  }
// Send the command    
    Serial.print("F");

    TotalField = Serial.readString();
    FLen=TotalField.length();
    
    tft.setCursor(70,yloc);
    if ( FLen < 2) {
      tft.setTextSize(1);
      tft.print("Field timed out");
    }
    else {
      tft.setTextSize(2);
// dont display EOL chars at end of valid field readings    
      tft.print(TotalField.substring(0,FLen-1));
    }
    delay(GSM90F_sampling_delay);

// added 2022-11-08   
// flush the serial buffer after "F" command
    while (Serial.available() > 0) {
      k = Serial.read();
    }
  }
  tft.drawLine(0,yloc+15,320,yloc+15,BLACK);
  tft.drawLine(0,yloc+17,320,yloc+17,BLACK);
  delay(GSM90F_sampling_delay);
  delay(GSM90F_sampling_delay);
  main_menu();
}
////////////////////////////////////////////////////////////////////////////////
void setup(void)
{
    Serial.begin(9600);
    uint16_t ID = tft.readID();
// Dont write to serial port incase GSM90 is already connected   
//  Serial.print("TFT Screen ID = 0x"); Serial.println(ID, HEX);

// initialise the TFT screen object    
    tft.begin(ID);
    tft.setRotation(0);            // PORTRAIT
    
//  position(X,Y) , shape(X,Y) , colour(EDGE, BODY, FONT),  text, text size 
// "position" is the CENTRE of the button rectangle
     run_btn.initButton(&tft, 120, 80 , 150, 60, WHITE, GREEN, BLACK, "Run", 3);
    baud_btn.initButton(&tft,  60, 180, 100, 40, WHITE, CYAN, BLACK, "Baud",3);
    tune_btn.initButton(&tft, 180, 180, 100, 40, WHITE, CYAN, BLACK, "Tune",3);
     obs_btn.initButton(&tft, 120, 280, 100, 40, WHITE, CYAN, BLACK, "# Obs", 3);

      up_btn.initButton(&tft, 60, 180, 100, 40, WHITE, YELLOW, BLACK, "Up",3);
    down_btn.initButton(&tft,180, 180, 100, 40, WHITE, YELLOW, BLACK, "Down",3);
     sel_btn.initButton(&tft,120,  80, 150, 60, WHITE, GREEN, BLACK, "Select", 3);

// Read obs data from EEPROM
  readEEPROM();


//Dont write to serial port in-case instrument is already connected
//  Serial.println("Observatory Parameters:");
//  Serial.print("Tune: ");
//  Serial.println(tune);
//  Serial.print("BaudIndex ");
//  Serial.print(baudIndex);  Serial.print(": "); Serial.println(baudSelect[baudIndex]);
//  Serial.print("#Obs: ");
//  Serial.println(repeats);
//  Serial.print("Version "); Serial.println(_VERSION);    
  
  tft.fillScreen(WHITE);   
  tft.setTextColor(BLACK);  
  tft.setTextSize(2);
  tft.setCursor(30, 30);   tft.print("Screen ID 0x"); tft.println(ID, HEX);
  tft.setTextSize(3);
  tft.setCursor(30,50 );
  tft.print("GA GEOMAG");
  tft.setTextSize(2);
  tft.setCursor(30, 90);
  tft.print("GSM90 Interface");
  tft.setCursor(30, 120);
  tft.print("Version ");    tft.println(_VERSION);    
  tft.setCursor(30, 150);   tft.print("Tune: "); tft.println(tune);
  tft.setCursor(30, 180);   tft.print("Baud: "); tft.println(baudSelect[baudIndex]);
  tft.setCursor(30, 210);   tft.print("# Obs: "); tft.println(repeats);
  delay(5000);
  main_menu();
}
//////////////////////////////////////////////////////////////////////////////////////////////////
void loop(void)
{
// down = true if touch screen is presssed and returns the X, Y co-ordinates into global variables pixel_x, pixel_y 
    bool down = Touch_getXY();
    run_btn.press(down && run_btn.contains(pixel_x, pixel_y));
    baud_btn.press(down && baud_btn.contains(pixel_x, pixel_y));
    tune_btn.press(down && tune_btn.contains(pixel_x, pixel_y));
    obs_btn.press(down && obs_btn.contains(pixel_x, pixel_y));    
    if (run_btn.justReleased())   run_btn.drawButton();
    if (baud_btn.justReleased())  baud_btn.drawButton();
    if (tune_btn.justReleased())  tune_btn.drawButton();
    if (obs_btn.justReleased())   obs_btn.drawButton();

    if (run_btn.justPressed())   comms();        
    if (baud_btn.justPressed())  baudrate();
    if (tune_btn.justPressed())  tuneMag();
    if (obs_btn.justPressed())   obs();
}
