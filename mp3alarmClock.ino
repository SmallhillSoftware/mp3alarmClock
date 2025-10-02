/******************************************************************************
 * MP3ALARMCLOCK
 * Drives a 4-digit-7-Segment-LED-display, implements a quarz- or net-frequency
 * synced clock with possibility to set an alarm time
 * Plays MP3-files between 00.mp3 and 99.mp3 when the alarm time fires
 * www.smallhill.de 2025
 ******************************************************************************/
#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include "MCP23008.h"

/******************************************************************************
 * globale Defines
 ******************************************************************************/
// These are the pins used for the music maker shield
#define VS1053RESETpin -1      // VS1053 reset pin (unused!)
#define VS1053CSpin     7      // VS1053 chip select pin (output)
#define VS1053DCSpin    6      // VS1053 Data/command select pin (output)
// These are common pins between breakout and shield
#define SDCARDCSpin     4      // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define VS1053DREQpin   3      // VS1053 Data request, ideally an Interrupt pin
//#define D_RELOADvalFor20ms 25536
#define D_RELOADvalFor2ms 61537
#define D_mp3Clock 1
#if (D_mp3Clock)
  //pinning for for MP3-clock
  #define D_SEGCLOCKpin 0
  #define D_BCDApin 1
  #define D_BCDBpin 9
  #define D_BCDCpin 10
  #define D_BCDDpin A1
#else
  //pinning for Arduino Uno Adapter but not for MP3-clock
  #define D_SEGCLOCKpin 10
  #define D_BCDApin 1
  #define D_BCDBpin 2
  #define D_BCDCpin 9
  #define D_BCDDpin 0
#endif
#define D_DISPSTATECLK 12 //C
#define D_DISPSTATEALM 10 //A
#define D_DISPSTATEACT 15 //F
#define D_DISPSTATEVER 11 //b
#define D_DISPSTATE_TST2 13 //d
#define D_DISPSTATE_TST3 14 //e

#define D_PowerOff_State             0
#define D_ShowVersion_State          1
#define D_noTimeYet_State            2
#define D_RunClockNoAlarmSet_State   3
#define D_RunClockAlarmSet_State     4
#define D_RunClockAlarmActive_State  5
#define D_AdjustTime_State           6
#define D_AdjustTimeHr_State         7
#define D_AdjustTimeMn_State         8
#define D_AdjustAlarm_State          9
#define D_AdjustAlarmHr_State       10
#define D_AdjustAlarmMn_State       11
#define D_noAlarmTimeYet_State      12

//eeprom addresses
#define D_eepromAdrFileHandling      0
#define D_eepromAdrAlarmHr           1
#define D_eepromAdrAlarmMin          2


/******************************************************************************
 * globale Variablen
 ******************************************************************************/
Adafruit_VS1053_FilePlayer musicPlayer = 
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(VS1053RESETpin, VS1053CSpin, VS1053DCSpin, VS1053DREQpin, SDCARDCSpin);

byte BT_mk_month = 9;
byte BT_mk_day = 28;

bool BL_setupFinished = false;

//clock variables
byte BT_currHr = 12;
byte BT_currMin = 34;
//alarm variables
byte BT_alarmHr = 0;
byte BT_alarmMin = 0;
//display variables
byte BT_dispHr = 0;
byte BT_dispMin = 0;
byte BT_dispState = 0;
//unsigned int UI_twentyMSecs = 0;
unsigned int UI_twoMSecs = 0;
unsigned int UI_seconds = 0;

//display variables
int INT_digitToUpdate = 0;

//i2c-port-extender
MCP23008 MCP(0x20);

#define D_ALMBUTTON     0
#define D_SETTIMEBUTTON 1
#define D_HOURINCBUTTON 2
#define D_MININCBUTTON  3
#define D_ALMONBUTTON   4
#define D_MID_LEDS_OUT  5
#define D_D2_DP_OUT     6
#define D_OE_RESET_OUT  7
byte BT_buttonState = 0;
bool BL_midLedState = false;

//state machine
byte BT_State = D_PowerOff_State;
byte BT_prevState;

//get time from battery backed-up RTC
bool B_timeFromRtc;
unsigned char UC_hourFromRtc = 0;
unsigned char UC_minuteFromRtc = 0;

//eeprom variables
unsigned char UC_filenumber = 0;
bool B_fileNrInEeprom;
unsigned char UC_storedAlarmHr  = 0;
bool B_alarmHrInEeprom;
unsigned char UC_storedAlarmMin = 0;
bool B_alarmMinInEeprom;

//file name for MP3-files to play in 8.3-format
char C_filename[12];

//indicator if currently MP3s are played
bool B_playMusic = false;

/******************************************************************************
 * Funktionen
 ******************************************************************************/
void initSegmentCounter(void)
{
  digitalWrite(D_SEGCLOCKpin, LOW);
  MCP.write1(D_OE_RESET_OUT, HIGH);
  for (int i=0; i<5; i++)
  {
    //Generiere 5 Pulse um alle D-flip-flops durchzutakten
    digitalWrite(D_SEGCLOCKpin, LOW);
    delay(50);
    digitalWrite(D_SEGCLOCKpin, HIGH);
    delay(50);
  }
  //Reset fuer Segment-Counter zuruecknehmen
  MCP.write1(D_OE_RESET_OUT, LOW);
}


void writeBcdToSegPins(byte BT_givenNumber, byte BT_upperNibble)
{
byte BT_number;
  if (BT_upperNibble == 1)
  {
    BT_number = (byte)(BT_givenNumber / 10);
  }
  else if (BT_upperNibble == 2)
  {
    BT_number = BT_givenNumber;
  }
  else
  {
    BT_number = (byte)(BT_givenNumber % 10);
  }
  if ((BT_number & 1) == 1)
  {
    digitalWrite(D_BCDApin, HIGH);
  }
  else
  {
    digitalWrite(D_BCDApin, LOW);
  }
  if ((BT_number & 2) == 2)
  {
    digitalWrite(D_BCDBpin, HIGH);
  }
  else
  {
    digitalWrite(D_BCDBpin, LOW);
  }
  if ((BT_number & 4) == 4)
  {
    digitalWrite(D_BCDCpin, HIGH);
  }
  else
  {
    digitalWrite(D_BCDCpin, LOW);
  }
  if ((BT_number & 8) == 8)
  {
    digitalWrite(D_BCDDpin, HIGH);
  }
  else
  {
    digitalWrite(D_BCDDpin, LOW);
  }
}


void setup()
{
byte bt_eepromData;
  //Clock pin fuer Segment-Counter als Ausgang und Low setzen
  pinMode(D_SEGCLOCKpin, OUTPUT);
  digitalWrite(D_SEGCLOCKpin, LOW);
  //BCD-pins fuer D346 als Ausgaenge setzen
  pinMode(D_BCDApin, OUTPUT);
  digitalWrite(D_BCDApin, LOW);
  pinMode(D_BCDBpin, OUTPUT);
  digitalWrite(D_BCDBpin, LOW);
  pinMode(D_BCDCpin, OUTPUT);
  digitalWrite(D_BCDCpin, LOW);
  pinMode(D_BCDDpin, OUTPUT);
  digitalWrite(D_BCDDpin, LOW);

  //get file number from eeprom
  bt_eepromData = EEPROM.read(D_eepromAdrFileHandling);
  if ((bt_eepromData & 0x80) == 0x80)
  {
    B_fileNrInEeprom = true;
    UC_filenumber = (bt_eepromData & 0x7F);
    if (UC_filenumber > 99)
    {
      UC_filenumber = 99;
    }
  }
  else
  {
    B_fileNrInEeprom = false;
    UC_filenumber = 0;
  }

  //get alarm hour from eeprom
  bt_eepromData = EEPROM.read(D_eepromAdrAlarmHr);
  if ((bt_eepromData & 0x80) == 0x80)
  {
    B_alarmHrInEeprom = true;
    UC_storedAlarmHr = (bt_eepromData & 0x7F);
    if (UC_storedAlarmHr > 23)
    {
      B_alarmHrInEeprom = false;
      UC_storedAlarmHr = 0;
    }
  }
  else
  {
    B_alarmHrInEeprom = false;
    UC_storedAlarmHr = 0;
  }

  //get alarm minute from eeprom
  bt_eepromData = EEPROM.read(D_eepromAdrAlarmMin);
  if ((bt_eepromData & 0x80) == 0x80)
  {
    B_alarmMinInEeprom = true;
    UC_storedAlarmMin = (bt_eepromData & 0x7F);
    if (UC_storedAlarmMin > 59)
    {
      B_alarmMinInEeprom = false;
      UC_storedAlarmMin = 0;
    }
  }
  else
  {
    B_alarmMinInEeprom = false;
    UC_storedAlarmMin = 0;
  }

  //get time from battery backed-up RTC
  B_timeFromRtc = false;
  
  //Alle Interrupts deaktivieren
  noInterrupts();
  //
  INT_digitToUpdate = 0;
  //Bits von TCCR1A und TCCR1B loeschen
  TCCR1A = 0;
  TCCR1B = 0;
  //Laden des Zaehlerwertes
  TCNT1 = D_RELOADvalFor2ms;
  //Prescaler: 8
  TCCR1B |= (1 << CS11);
  //Enable timer overflow interrupt
  TIMSK1 |= (1 << TOIE1);
  //Alle Interrupts aktivieren
  interrupts();
      
  //MP3-Decoder initialisieren
  musicPlayer.begin();
  
  //SD-Karte initialisieren
  //SD_CS als parameter übergeben, da hier ChipSelect anders belegt
  SD.begin(SDCARDCSpin);
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(30,30);

  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

  Wire.begin();
  Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
  MCP.begin(false);

  while(!(MCP.isConnected()))
  {
    //wait to be connected
    //verbindet aber nicht
  }
  
  //MCP.pinMode8(0x1F); //upper 3 bits are outputs
  MCP.pinMode1(D_ALMBUTTON, INPUT);
  MCP.pinMode1(D_SETTIMEBUTTON, INPUT);
  MCP.pinMode1(D_HOURINCBUTTON, INPUT);
  MCP.pinMode1(D_MININCBUTTON, INPUT);
  MCP.pinMode1(D_ALMONBUTTON, INPUT);
  MCP.pinMode1(D_MID_LEDS_OUT, OUTPUT);
  MCP.pinMode1(D_D2_DP_OUT, OUTPUT);
  MCP.pinMode1(D_OE_RESET_OUT, OUTPUT);
  
  MCP.setPolarity(D_ALMBUTTON, 0);
  MCP.setPolarity(D_SETTIMEBUTTON, 0);
  MCP.setPolarity(D_HOURINCBUTTON, 0);
  MCP.setPolarity(D_MININCBUTTON, 0);
  MCP.setPolarity(D_ALMONBUTTON, 0);
  MCP.setPolarity(D_MID_LEDS_OUT, 0);
  MCP.setPolarity(D_D2_DP_OUT, 0);
  MCP.setPolarity(D_OE_RESET_OUT, 0);

  MCP.setPullup(D_ALMBUTTON, 0);
  MCP.setPullup(D_SETTIMEBUTTON, 0);
  MCP.setPullup(D_HOURINCBUTTON, 0);
  MCP.setPullup(D_MININCBUTTON, 0);
  MCP.setPullup(D_ALMONBUTTON, 0);
  MCP.setPullup(D_MID_LEDS_OUT, 0);
  MCP.setPullup(D_D2_DP_OUT, 0);
  MCP.setPullup(D_OE_RESET_OUT, 0);
  
  initSegmentCounter(); //reset segment counter in GAL

  MCP.write1(D_MID_LEDS_OUT, HIGH); //klappt
  MCP.write1(D_D2_DP_OUT, HIGH);
  
  //signalize that the setup routine is finished
  BL_setupFinished = true;
}


ISR(TIMER1_OVF_vect)
{
  //START EXT-PIN-COUNT-IRQ
  //code to be executed when setup-routine is finished only
  if (BL_setupFinished)
  {
    //1sec passed
    if (UI_twoMSecs == 500)
    {
      UI_twoMSecs = 0;
      UI_seconds++;
      if (BL_midLedState == false)
      {
        BL_midLedState = true;
      }
      else
      {
        BL_midLedState = false;
      }
    }
    //1 minute passed, 500 occurrences of 2ms = 1sec and 60 of them means 1 min
    if (UI_seconds == 60)
    {
      UI_seconds = 0;
      if (BT_currMin < 59)
      {
        BT_currMin++;
      } //if (BT_currMin < 59)
      else
      {
        BT_currMin = 0;
        if (BT_currHr < 23)
        {
          BT_currHr++;
        } //if (BT_currHr < 23)
        else
        {
          BT_currHr = 0;
        } //else of if (BT_currHr < 23)
      } //else of if (BT_currMin < 59)
    } //if (UI_twoMSecs == (500 * 60))

    //write value to port pin
    digitalWrite(D_SEGCLOCKpin, LOW);
    if (INT_digitToUpdate == 0)
    {
      writeBcdToSegPins(BT_dispHr, 1);
      INT_digitToUpdate = 1;
    } //if (INT_digitToUpdate == 0)
    else if (INT_digitToUpdate == 1)
    {
      writeBcdToSegPins(BT_dispHr, 0);
      INT_digitToUpdate = 2;
    } //else if (INT_digitToUpdate == 1)
    else if (INT_digitToUpdate == 2)
    {
      writeBcdToSegPins(BT_dispMin, 1);
      INT_digitToUpdate = 3;
    } //else if (INT_digitToUpdate == 2)
    else if (INT_digitToUpdate == 3)
    {
      writeBcdToSegPins(BT_dispMin, 0);
      INT_digitToUpdate = 4;
    } //else if (INT_digitToUpdate == 3)
    else if (INT_digitToUpdate == 4)
    {
      writeBcdToSegPins(BT_dispState, 2);
      INT_digitToUpdate = 0;
    } //else if (INT_digitToUpdate == 4)
    //write value to port pin
    digitalWrite(D_SEGCLOCKpin, HIGH);
    //
    UI_twoMSecs++;
  } //if (BL_setupFinished)
  //reload counter value to make the IRQ firing again, always necessary independent from setup finished or not
  TCNT1 = D_RELOADvalFor2ms;
  //END EXT-PIN-COUNT-IRQ  
}

void loop()
{
unsigned int cntr = 0;
//build file name
sprintf(C_filename, "/%02d.mp3", UC_filenumber);
  
byte bt_eepromData;
  if (BL_midLedState == true)
  {
    MCP.write1(D_D2_DP_OUT, LOW);
  }
  else
  {
    MCP.write1(D_D2_DP_OUT, HIGH);
  }
  //read in buttons
  BT_buttonState = 0;
  if (MCP.read1(D_ALMBUTTON) == HIGH)
  {
    BT_buttonState = BT_buttonState | (byte)(((byte)(1 << D_ALMBUTTON)) & 0xFF);
  }
  if (MCP.read1(D_SETTIMEBUTTON) == HIGH)
  {
    BT_buttonState = BT_buttonState | (byte)(((byte)(1 << D_SETTIMEBUTTON)) & 0xFF);
  }
  if (MCP.read1(D_HOURINCBUTTON) == HIGH)
  {
    BT_buttonState = BT_buttonState | (byte)(((byte)(1 << D_HOURINCBUTTON)) & 0xFF);
  }
  if (MCP.read1(D_MININCBUTTON) == HIGH)
  {
    BT_buttonState = BT_buttonState | (byte)(((byte)(1 << D_MININCBUTTON)) & 0xFF);
  }
  if (MCP.read1(D_ALMONBUTTON) == HIGH)
  {
    BT_buttonState = BT_buttonState | (byte)(((byte)(1 << D_ALMONBUTTON)) & 0xFF);
  }
  
  //state machine
  switch (BT_State)
  {
    case D_PowerOff_State:
      BT_prevState = BT_State; //store previous state
      BT_State = D_ShowVersion_State;
      break; //D_PowerOff_State
    case D_ShowVersion_State:
      BT_dispHr = BT_mk_month;
      BT_dispMin = BT_mk_day;
      BT_dispState = D_DISPSTATEVER;
      delay(3000); //condition to change to next state
      if (B_timeFromRtc == true)
      {
        if ((B_alarmHrInEeprom == true) && (B_alarmMinInEeprom == true))
        {
          BT_State = D_RunClockNoAlarmSet_State;
        }
        else
        {
          BT_State = D_noAlarmTimeYet_State;
        }
      }
      else
      {
        BT_State = D_noTimeYet_State;
      }
      break; //D_ShowVersion_State
    case D_noTimeYet_State:
      BT_dispHr = 77;
      BT_dispMin = 77;
      BT_dispState = D_DISPSTATECLK;
      if ((BT_buttonState & (1 << D_SETTIMEBUTTON)) == (1 << D_SETTIMEBUTTON))
      {
        if ((B_alarmHrInEeprom == true) && (B_alarmMinInEeprom == true))
        {
          BT_State = D_RunClockNoAlarmSet_State;
        }
        else
        {
          BT_State = D_noAlarmTimeYet_State;
        }
      }
      break; //D_noTimeYet_State
    case D_noAlarmTimeYet_State:
      BT_dispHr = 99;
      BT_dispMin = 99;
      BT_dispState = D_DISPSTATEALM;
      if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        BT_State = D_RunClockNoAlarmSet_State;
      }
      break; //D_noAlarmTimeYet_State
    case D_RunClockNoAlarmSet_State:
      BT_dispHr = BT_currHr;
      BT_dispMin = BT_currMin;
      BT_dispState = D_DISPSTATECLK;
      //wenn ich diese Bedingung rausnehme dann wechselt er nicht mehr in den Einstellmodus
      //wenn ich dann aber den Alarmzustand aktiviere wechselt er in den Einstellmodus und
      //alles blinkt
      if ((BT_buttonState & (1 << D_SETTIMEBUTTON)) == (1 << D_SETTIMEBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTime_State;
      }
      else if ((BT_buttonState & (1 << D_ALMONBUTTON)) == (1 << D_ALMONBUTTON))
      {
        BT_State = D_RunClockAlarmSet_State;
      }
      break; //D_RunClockNoAlarmSet_State
    case D_RunClockAlarmSet_State:
      BT_dispHr = BT_currHr;
      BT_dispMin = BT_currMin;
      BT_dispState = D_DISPSTATEALM;
      if ((BT_currHr == BT_alarmHr) && (BT_currMin == BT_alarmMin))
      {
        BT_State = D_RunClockAlarmActive_State;
      }
      else if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarm_State; 
      }
      else if (!((BT_buttonState & (1 << D_ALMONBUTTON)) == (1 << D_ALMONBUTTON)))
      {
        BT_State = D_RunClockNoAlarmSet_State;
      }
      break; //D_RunClockAlarmSet_State
    case D_RunClockAlarmActive_State:
      BT_dispHr = BT_currHr;
      BT_dispMin = BT_currMin;
      BT_dispState = D_DISPSTATEACT;
      //play mp3s
      if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        bt_eepromData = UC_filenumber + 0x80;
        EEPROM.update(D_eepromAdrFileHandling, bt_eepromData);
        BT_State = D_RunClockAlarmSet_State;
      }
      break; //D_RunClockAlarmActive_State
    case D_AdjustTime_State:
      BT_dispHr = 77;
      BT_dispMin = 77;
      BT_dispState = D_DISPSTATECLK;
      if ((BT_buttonState & (1 << D_HOURINCBUTTON)) == (1 << D_HOURINCBUTTON))
      {
        BT_State = D_AdjustTimeHr_State;
      }
      else if ((BT_buttonState & (1 << D_MININCBUTTON)) == (1 << D_MININCBUTTON))
      {
        BT_State = D_AdjustTimeMn_State;
      }
      else if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        if (BT_prevState == D_RunClockAlarmSet_State)
        {
          BT_State = D_RunClockAlarmSet_State;
        }
        else
        {
          BT_State = D_RunClockNoAlarmSet_State;
        }
      }
      break; //D_AdjustTime_State
    case D_AdjustTimeHr_State:
      BT_dispHr = BT_currHr;
      BT_dispMin = 77;
      BT_dispState = D_DISPSTATECLK;
      if ((BT_buttonState & (1 << D_HOURINCBUTTON)) == (1 << D_HOURINCBUTTON))
      {
        if (BT_currHr < 23)
        {
          BT_currHr++;
        }
        else
        {
          BT_currHr = 0;
        }
      }
      else if ((BT_buttonState & (1 << D_SETTIMEBUTTON)) == (1 << D_SETTIMEBUTTON))
      {
        BT_State = D_AdjustTime_State;
      }
      break; //AdjustTimeHr_State
    case D_AdjustTimeMn_State:
      BT_dispHr = 77;
      BT_dispMin = BT_currMin;
      BT_dispState = D_DISPSTATECLK;
      if ((BT_buttonState & (1 << D_MININCBUTTON)) == (1 << D_MININCBUTTON))
      {
        if (BT_currMin < 59)
        {
          BT_currMin++;
        }
        else
        {
          BT_currMin = 0;
        }
      }
      else if ((BT_buttonState & (1 << D_SETTIMEBUTTON)) == (1 << D_SETTIMEBUTTON))
      {
        BT_State = D_AdjustTime_State;
      }
      break; //D_AdjustTimeMn_State
    case D_AdjustAlarm_State:
      BT_dispHr = 99;
      BT_dispMin = 99;
      BT_dispState = D_DISPSTATEALM;
      //ob ich diese Bedingung rauskommentiere oder drinlasse macht keinen Unterschied
      //er wechselt in den Einstell-Flacker-Modus
      if ((BT_buttonState & (1 << D_HOURINCBUTTON)) == (1 << D_HOURINCBUTTON))
      {
        BT_State = D_AdjustAlarmHr_State;
      }
      //ob ich diese Bedingung rauskommentiere oder drinlasse macht keinen Unterschied
      //er wechselt in den Einstell-Flacker-Modus
      else if ((BT_buttonState & (1 << D_MININCBUTTON)) == (1 << D_MININCBUTTON))
      {
        BT_State = D_AdjustAlarmMn_State;
      }
      //wenn ich diese Bedingungs auskommentiere wechselt er nicht mehr in den
      //Einstell-Flacker-Modus
      else if ((BT_buttonState & (1 << D_SETTIMEBUTTON)) == (1 << D_SETTIMEBUTTON))
      {
        //write Alarm Hour and Minute to EEPROM
        bt_eepromData = BT_alarmHr + 0x80;
        EEPROM.update(D_eepromAdrAlarmHr, bt_eepromData);
        bt_eepromData = BT_alarmMin + 0x80;
        EEPROM.update(D_eepromAdrAlarmMin, bt_eepromData);
        if (BT_prevState == D_RunClockAlarmSet_State)
        {
          BT_State = D_RunClockAlarmSet_State;
        }
        else
        {
          BT_State = D_RunClockNoAlarmSet_State;
        }
      }
      break; //D_AdjustAlarm_State
    case D_AdjustAlarmHr_State:
      BT_dispHr = BT_alarmHr;
      BT_dispMin = 99;
      BT_dispState = D_DISPSTATEALM;
      if ((BT_buttonState & (1 << D_HOURINCBUTTON)) == (1 << D_HOURINCBUTTON))
      {
        if (BT_alarmHr < 23)
        {
          BT_alarmHr++;
        }
        else
        {
          BT_alarmHr = 0;
        }
      }
      else if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        BT_State = D_AdjustAlarm_State;
      }
      break; //D_AdjustAlarmHr_State
    case D_AdjustAlarmMn_State:
      BT_dispHr = 99;
      BT_dispMin = BT_alarmMin;
      BT_dispState = D_DISPSTATEALM;
      if ((BT_buttonState & (1 << D_MININCBUTTON)) == (1 << D_MININCBUTTON))
      {
        if (BT_alarmMin < 59)
        {
          BT_alarmMin++;
        }
        else
        {
          BT_alarmMin = 0;
        }
      }
      else if ((BT_buttonState & (1 << D_ALMBUTTON)) == (1 << D_ALMBUTTON))
      {
        BT_State = D_AdjustAlarm_State;
      }
      break; //D_AdjustAlarmMn_State
    default:
      break;
  }
  if (BT_State == D_RunClockAlarmActive_State)
  {
    if (!B_playMusic)
    {
      musicPlayer.startPlayingFile(C_filename);
    } //if (!B_playMusic)
    B_playMusic = musicPlayer.playingMusic;
    if (!B_playMusic)
    {
      if (UC_filenumber < 99)
      {
        UC_filenumber++;
      }
    	else
    	{
        UC_filenumber = 0;
    	}
    } //if (!B_playMusic)
  } //if (BT_State == D_RunClockAlarmActive_State)
  else
  {
    if (musicPlayer.playingMusic)
    {
      musicPlayer.stopPlaying();
    }
  }
  delay(100);
}
