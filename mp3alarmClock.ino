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

/******************************************************************************
 * globale Defines
 ******************************************************************************/
#define SEGCLOCKpin 10
//#define RELOADvalFor20ms 25536
#define RELOADvalFor20ms 61535
#define BCDApin 1
#define BCDBpin 2
#define BCDCpin 9
#define BCDDpin 0


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
unsigned int twentyMSecs = 0;

//display variables
int digitToUpdate = 0;


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
  TCNT1 = RELOADvalFor20ms;
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
}


ISR(TIMER1_OVF_vect)
{
  //1 minute passed, 50 occurrences of 20ms = 1sec and 60 of them means 1 min
  if (twentyMSecs == (50 * 60))
  {
    twentyMSecs = 0;
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
  //Pin-Wert an Portpin schreiben
  digitalWrite(SEGCLOCKpin, LOW);
  if (digitToUpdate == 0)
  {
    writeBcdToSegPins(currHr, 1);
    digitToUpdate = 1;
  }
  else if (digitToUpdate == 1)
  {
    writeBcdToSegPins(currHr, 0);
    digitToUpdate = 2;
  }
  else if (digitToUpdate == 2)
  {
    writeBcdToSegPins(currMin, 1);
    digitToUpdate = 3;
  }
  else if (digitToUpdate == 3)
  {
    writeBcdToSegPins(currMin, 0);
    digitToUpdate = 4;
  }
  else if (digitToUpdate == 4)
  {
    writeBcdToSegPins(12, 2);
    digitToUpdate = 5;
  }
  else if (digitToUpdate == 5)
  {
    //
    writeBcdToSegPins(12, 2);
    digitToUpdate = 0;
  }
  //Pin-Wert an Portpin schreiben
  digitalWrite(SEGCLOCKpin, HIGH);
  //
  twentyMSecs++;
  //Laden des Zaehlerwertes
  TCNT1 = RELOADvalFor20ms;
}

void loop()
{
  delay( 1000 );
}
