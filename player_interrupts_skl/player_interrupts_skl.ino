/*************************************************** 
  This is an example for the Adafruit VS1053 Codec Breakout

  Designed specifically to work with the Adafruit VS1053 Codec Breakout 
  ----> https://www.adafruit.com/products/1381

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// include SPI, MP3 and SD libraries
#include <EEPROM.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);


////
char filename[11];

//Zähler für Dateien
unsigned char filenumber = 0;

bool bPlayMusic = false;
int  iEeepromAdrFileHandling = 0;
bool bFileNrInEeprom;
byte eepromData;

void setup()
{
  eepromData = EEPROM.read(iEeepromAdrFileHandling);
  if ((eepromData & 0x80) == 0x80)
  {
    bFileNrInEeprom = true;
    filenumber = (eepromData & 0x7F);
    if (filenumber > 99)
    {
      filenumber = 99;
    }
  }
  else
  {
    bFileNrInEeprom = false;
    filenumber = 0;
  }

  Serial.begin(9600);
  Serial.println("Adafruit VS1053 Library Test");

  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));

  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);

  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    Serial.println(F("DREQ pin is not an interrupt pin"));
}

void loop() {
  char outputstring[100];

  //build file name
  sprintf(filename, "/%02d.mp3", filenumber);

  if (!bPlayMusic)
  {
    // Start playing a file, then we can do stuff while waiting for it to finish
    if (! musicPlayer.startPlayingFile(filename))
    {
      sprintf(outputstring, "Could not open file %s", filename);
      Serial.println(outputstring);
      while(1);
    }
    else
    {
      sprintf(outputstring, "Started playing file:%s", filename);
      Serial.println(outputstring);
    }
    // file is now playing in the 'background' so now's a good time
    // to do something else like handling LEDs or buttons :)
  }
  
  bPlayMusic = musicPlayer.playingMusic;
  
  if (!bPlayMusic)
  {
    if (filenumber < 99)
    {
      filenumber++;
      eepromData = filenumber + 0x80;
      EEPROM.update(iEeepromAdrFileHandling, eepromData);
    }
    else
    {
      filenumber = 0;
    }
  }
}
