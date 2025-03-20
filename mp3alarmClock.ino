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
#define SEGCLOCKpin 0
#define RELOADvalFor20ms 25536
#define BCDApin 1
#define BCDBpin 2
#define BCDCpin 9
#define BCDDpin 10


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
  switch (digitToUpdate)
  {
    case 0:
      //write upper nibble of hour
      writeBcdToSegPins(currHr, 1);
      digitToUpdate = 1;
      break;
    case 1:
      //write lower nibble of hour
      writeBcdToSegPins(currHr, 0);
      digitToUpdate = 2;
      break;
    case 2:
      //write upper nibble of minute
      writeBcdToSegPins(currMin, 1);
      digitToUpdate = 3;
      break;
    case 3:
      //write lower nibble of minute
      writeBcdToSegPins(currMin, 0);
      digitToUpdate = 4;
      break;
    case 4:
      //write symbols a,c
      writeBcdToSegPins(12, 2);
      digitToUpdate = 0;
      break;
    default:
      break;
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
