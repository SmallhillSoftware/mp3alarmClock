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
#define DISPSTATECLK 12 //C
#define DISPSTATEALM 10 //A
#define DISPSTATE_TST1 11 //b
#define DISPSTATE_TST2 13 //d
#define DISPSTATE_TST3 14 //e
#define DISPSTATE_TST4 15 //f


/******************************************************************************
 * globale Variablen
 ******************************************************************************/
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

/******************************************************************************
 * Funktionen
 ******************************************************************************/
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
  //Clock pin fuer Johnson-Counter als Ausgang setzen
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

  //
  Serial.end();

  //Alle Interrupts aktivieren
  interrupts();
  
  Wire.begin();
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
    digitToUpdate = 5;
  }
  else if (digitToUpdate == 5)
  {
    //
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
  /*
  if (MCP.read1(ALMBUTTON))
  {
    buttonState = buttonState | (1 << ALMBUTTON);
    dispState = DISPSTATECLK;
  }
  else
  {
    buttonState = buttonState & B11111110;
  }
  if (MCP.read1(SETTIMEBUTTON))
  {
    buttonState = buttonState | (1 << SETTIMEBUTTON);
    dispState = DISPSTATEALM;
  }
  else
  {
    buttonState = buttonState & B11111101;
  }
  if (MCP.read1(HOURINCBUTTON))
  {
    buttonState = buttonState | (1 << HOURINCBUTTON);
  }
  else
  {
    buttonState = buttonState & B11111011;
  }
  if (MCP.read1(MININCBUTTON))
  {
    buttonState = buttonState | (1 << MININCBUTTON);
  }
  else
  {
    buttonState = buttonState & B11110111;
  }
  */
  if (MCP.read1(ALMONBUTTON) == false)
  {
    dispHr = currHr;
    dispMin = currMin;
    dispState = DISPSTATEALM;
  }
  else
  {
    dispHr = currHr;
    dispMin = currMin;
    dispState = DISPSTATECLK;
  }
  delay(100);
}
