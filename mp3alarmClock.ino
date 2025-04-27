/******************************************************************************
 * Beispiel:      extended
 * 
 * Beschreibung:  	Dieses Beispiel spielt mehrere Dateien nacheinander ab.
 * 				Dateien werden dabei mit fortlaufender Nummer aufgerufen:
 * 				"001.mp3" bis "010.mp3"
 * 				Erweiterte Funktionen zur Erkennung und zum	Schalten der 
 * 				Versorgungsspannung der SD-Karte aktiviert.
 * 				Zur besseren Verständnis werden zusätzliche Ausgaben an 
 * 				den PC geschickt (können in Serial Monitor betrachtet werden).
 * 				
 ******************************************************************************/
#include <SD.h>
#include <SPI.h>
#include <AudioShieldMinimal.h>
#include "MCP23008.h"

/******************************************************************************
 * globale Defines
 ******************************************************************************/
#define SEGCLOCKpin 10
//#define RELOADvalFor20ms 25536
#define RELOADvalFor2ms 61535
#define BCDApin 1
#define BCDBpin 2
#define BCDCpin 9
#define BCDDpin 0
#define interimResetPin A0
#define DISPSTATECLK 12 //C
#define DISPSTATEALM 10 //A
#define DISPSTATEACT 15 //F
#define DISPSTATEVER 11 //b
#define DISPSTATE_TST2 13 //d
#define DISPSTATE_TST3 14 //e


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


/******************************************************************************
 * globale Variablen
 ******************************************************************************/
byte mk_month = 4;
byte mk_day = 26;

//Variable für Dateinamen anlegen
//Dateinamen entsprechend 8.3 Format
char filename[13] = "01.wav";

//Zähler für Dateien
unsigned char filenumber = 1;

//clock variables
byte currHr = 12;
byte currMin = 34;
//alarm variables
byte alarmHr = 0;
byte alarmMin = 0;
//display variables
byte dispHr = 0;
byte dispMin = 0;
byte dispState = 0;
//unsigned int twentyMSecs = 0;
unsigned int twoMSecs = 0;

//display variables
int digitToUpdate = 0;

//i2c-port-extender
MCP23008 MCP(0x20);
#define ALMBUTTON     0
#define SETTIMEBUTTON 1
#define HOURINCBUTTON 2
#define MININCBUTTON  3
#define ALMONBUTTON   4
byte buttonState = 0;

//state machine
byte BT_State = D_PowerOff_State;
byte BT_prevState;

/******************************************************************************
 * Funktionen
 ******************************************************************************/
void initSegmentCounter(void)
{
  digitalWrite(SEGCLOCKpin, LOW);
  digitalWrite(interimResetPin, HIGH);
  for (int i=1; i<=5; i++)
  {
    //Generiere 5 Pulse um alle D-flip-flops durchzutakten
    digitalWrite(SEGCLOCKpin, LOW);
    delay(50);
    digitalWrite(SEGCLOCKpin, HIGH);
    delay(50);
  }
  //Reset fuer Segment-Counter zuruecknehmen
  digitalWrite(interimResetPin, LOW);
}


void writeBcdToSegPins(byte givenNumber, byte upperNibble)
{
byte number;
  if (upperNibble == 1)
  {
    number = (byte)(givenNumber / 10);
  }
  else if (upperNibble == 2)
  {
    number = givenNumber;
  }
  else
  {
    number = (byte)(givenNumber % 10);
  }
  if ((number & 1) == 1)
  {
    digitalWrite(BCDApin, HIGH);
  }
  else
  {
    digitalWrite(BCDApin, LOW);
  }
  if ((number & 2) == 2)
  {
    digitalWrite(BCDBpin, HIGH);
  }
  else
  {
    digitalWrite(BCDBpin, LOW);
  }
  if ((number & 4) == 4)
  {
    digitalWrite(BCDCpin, HIGH);
  }
  else
  {
    digitalWrite(BCDCpin, LOW);
  }
  if ((number & 8) == 8)
  {
    digitalWrite(BCDDpin, HIGH);
  }
  else
  {
    digitalWrite(BCDDpin, LOW);
  }
}


void setup()
{
  //Clock pin fuer Segment-Counter als Ausgang und Low setzen
  pinMode(SEGCLOCKpin, OUTPUT);
  digitalWrite(SEGCLOCKpin, LOW);
  //BCD-pins fuer D346 als Ausgaenge setzen
  pinMode(BCDApin, OUTPUT);
  digitalWrite(BCDApin, LOW);
  pinMode(BCDBpin, OUTPUT);
  digitalWrite(BCDBpin, LOW);
  pinMode(BCDCpin, OUTPUT);
  digitalWrite(BCDCpin, LOW);
  pinMode(BCDDpin, OUTPUT);
  digitalWrite(BCDDpin, LOW);
  //Reset pin fuer Segment-Counter als Ausgang und High fuer Reset active setzen
  pinMode(interimResetPin, OUTPUT);
  //Segment-Counter ruecksetzen
  initSegmentCounter();
  //Alle Interrupts deaktivieren
  noInterrupts();
  //
  digitToUpdate = 0;
  //Bits von TCCR1A und TCCR1B loeschen
  TCCR1A = 0;
  TCCR1B = 0;
  //Laden des Zaehlerwertes
  TCNT1 = RELOADvalFor2ms;
  //Prescaler: 8
  TCCR1B |= (1 << CS11);
  //Enable timer overflow interrupt
  TIMSK1 |= (1 << TOIE1);
      
  //SD-Karte initialisieren
  //SD_CS als parameter übergeben, da hier ChipSelect anders belegt
  //if( SD.begin( SD_CS ) == false )
  //{
      // Programm beenden, da keine SD-Karte vorhanden
  //  return;
  //}
  
  //MP3-Decoder initialisieren
  //VS1011.begin();

  //Alle Interrupts aktivieren
  interrupts();

  Wire.begin();
  Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
  MCP.begin();

  while(!(MCP.isConnected()))
  {
    //wait to be connected
    //verbindet aber nicht
  }
  
  MCP.pinMode8(0xFF);
}


ISR(TIMER1_OVF_vect)
{
  //START EXT-PIN-COUNT-IRQ
  //1 minute passed, 500 occurrences of 2ms = 1sec and 60 of them means 1 min
  if (twoMSecs == (500 * 60))
  {
    twoMSecs = 0;
    if (currMin < 59)
    {
      currMin++;
    }
    else
    {
      currMin = 0;
      if (currHr < 23)
      {
        currHr++;
      }
      else
      {
        currHr = 0;
      }
    }
  }
  //END EXT-PIN-COUNT-IRQ
  //Pin-Wert an Portpin schreiben
  digitalWrite(SEGCLOCKpin, LOW);
  if (digitToUpdate == 0)
  {
    writeBcdToSegPins(dispHr, 1);
    digitToUpdate = 1;
  }
  else if (digitToUpdate == 1)
  {
    writeBcdToSegPins(dispHr, 0);
    digitToUpdate = 2;
  }
  else if (digitToUpdate == 2)
  {
    writeBcdToSegPins(dispMin, 1);
    digitToUpdate = 3;
  }
  else if (digitToUpdate == 3)
  {
    writeBcdToSegPins(dispMin, 0);
    digitToUpdate = 4;
  }
  else if (digitToUpdate == 4)
  {
    writeBcdToSegPins(dispState, 2);
    digitToUpdate = 0;
  }
  //Pin-Wert an Portpin schreiben
  digitalWrite(SEGCLOCKpin, HIGH);
  //
  twoMSecs++;
  //Laden des Zaehlerwertes
  TCNT1 = RELOADvalFor2ms;
}

void loop()
{
  //read in buttons
  if (MCP.read1(ALMBUTTON) == true)
  {
    buttonState = buttonState | (1 << ALMBUTTON);
  }
  else
  {
    buttonState = buttonState & (~(1 << ALMBUTTON));
  }
  if (MCP.read1(SETTIMEBUTTON) == true)
  {
    buttonState = buttonState | (1 << SETTIMEBUTTON);
  }
  else
  {
    buttonState = buttonState & (~(1 << SETTIMEBUTTON));
  }
  if (MCP.read1(HOURINCBUTTON) == true)
  {
    buttonState = buttonState | (1 << HOURINCBUTTON);
  }
  else
  {
    buttonState = buttonState & (~(1 << HOURINCBUTTON));
  }
  if (MCP.read1(MININCBUTTON) == true)
  {
    buttonState = buttonState | (1 << MININCBUTTON);
  }
  else
  {
    buttonState = buttonState & (~(1 << MININCBUTTON));
  }
  if (MCP.read1(ALMONBUTTON) == true)
  {
    buttonState = buttonState | (1 << ALMONBUTTON);
  }
  else
  {
    buttonState = buttonState & (~(1 << ALMONBUTTON));
  }
  //state machine
  switch (BT_State)
  {
    case D_PowerOff_State:
      BT_prevState = BT_State; //store previous state
      BT_State = D_ShowVersion_State;
      break; //D_PowerOff_State
    case D_ShowVersion_State:
      dispHr = mk_month;
      dispMin = mk_day;
      dispState = DISPSTATEVER;
      delay(3000); //condition to change to next state
      BT_prevState = BT_State; //store previous state
      BT_State = D_noTimeYet_State;
      break; //D_ShowVersion_State
    case D_noTimeYet_State:
      dispHr = 77;
      dispMin = 77;
      dispState = DISPSTATECLK;
      //any button pressed
      if (buttonState > 0)
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockNoAlarmSet_State;
      }
      break; //D_noTimeYet_State
    case D_RunClockNoAlarmSet_State:
      dispHr = currHr;
      dispMin = currMin;
      dispState = DISPSTATECLK;
      if ((buttonState & (1 << SETTIMEBUTTON)) == (1 << SETTIMEBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTime_State;
      }
      else if ((buttonState & (1 << ALMONBUTTON)) == (1 << ALMONBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockAlarmSet_State;
      }
      break; //D_RunClockNoAlarmSet_State
    case D_RunClockAlarmSet_State:
      dispHr = currHr;
      dispMin = currMin;
      dispState = DISPSTATEALM;
      if ((currHr == alarmHr) && (currMin == alarmMin))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockAlarmActive_State;
      }
      else if ((buttonState & (1 << ALMBUTTON)) == (1 << ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarm_State; 
      }
      else if (!((buttonState & (1 << ALMONBUTTON)) == (1 << ALMONBUTTON)))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockNoAlarmSet_State;
      }
      break; //D_RunClockAlarmSet_State
    case D_RunClockAlarmActive_State:
      dispHr = currHr;
      dispMin = currMin;
      dispState = DISPSTATEACT;
      //play mp3s
      if ((buttonState & (1 << ALMBUTTON)) == (1 << ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockAlarmSet_State;
      }
      break; //D_RunClockAlarmActive_State
    case D_AdjustTime_State:
      dispHr = 99;
      dispMin = 99;
      dispState = DISPSTATECLK;
      if ((buttonState & (1 << HOURINCBUTTON)) == (1 << HOURINCBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTimeHr_State;
      }
      else if ((buttonState & (1 << MININCBUTTON)) == (1 << MININCBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTimeMn_State;
      }
      else if ((buttonState & (1 << ALMBUTTON)) == (1 << ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockNoAlarmSet_State;
      }
      break; //D_AdjustTime_State
    case D_AdjustTimeHr_State:
      dispHr = currHr;
      dispMin = 99;
      dispState = DISPSTATECLK;
      if ((buttonState & (1 << HOURINCBUTTON)) == (1 << HOURINCBUTTON))
      {
        if (currHr < 23)
        {
          currHr++;
        }
        else
        {
          currHr = 0;
        }
      }
      else if ((buttonState & (1 << SETTIMEBUTTON)) == (1 << SETTIMEBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTime_State;
      }
      break; //AdjustTimeHr_State
    case D_AdjustTimeMn_State:
      dispHr = 99;
      dispMin = currMin;
      dispState = DISPSTATECLK;
      if ((buttonState & (1 << MININCBUTTON)) == (1 << MININCBUTTON))
      {
        if (currMin < 59)
        {
          currMin++;
        }
        else
        {
          currMin = 0;
        }
      }
      else if ((buttonState & (1 << SETTIMEBUTTON)) == (1 << SETTIMEBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustTime_State;
      }
      break; //D_AdjustTimeMn_State
    case D_AdjustAlarm_State:
      dispHr = 99;
      dispMin = 99;
      dispState = DISPSTATEALM;
      if ((buttonState & (1 << HOURINCBUTTON)) == (1 << HOURINCBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarmHr_State;
      }
      else if ((buttonState & (1 << MININCBUTTON)) == (1 << MININCBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarmMn_State;
      }
      else if ((buttonState & (1 << SETTIMEBUTTON)) == (1 << SETTIMEBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_RunClockAlarmSet_State;
      }
      break; //D_AdjustAlarm_State
    case D_AdjustAlarmHr_State:
      dispHr = alarmHr;
      dispMin = 99;
      dispState = DISPSTATEALM;
      if ((buttonState & (1 << HOURINCBUTTON)) == (1 << HOURINCBUTTON))
      {
        if (alarmHr < 23)
        {
          alarmHr++;
        }
        else
        {
          alarmHr = 0;
        }
      }
      else if ((buttonState & (1 << ALMBUTTON)) == (1 << ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarm_State;
      }
      break; //D_AdjustAlarmHr_State
    case D_AdjustAlarmMn_State:
      dispHr = 99;
      dispMin = alarmMin;
      dispState = DISPSTATEALM;
      if ((buttonState & (1 << MININCBUTTON)) == (1 << MININCBUTTON))
      {
        if (alarmMin < 59)
        {
          alarmMin++;
        }
        else
        {
          alarmMin = 0;
        }
      }
      else if ((buttonState & (1 << ALMBUTTON)) == (1 << ALMBUTTON))
      {
        BT_prevState = BT_State; //store previous state
        BT_State = D_AdjustAlarm_State;
      }
      break; //D_AdjustAlarmMn_State
    default:
      break;
  }
  //write global actions
  delay(100);
}
