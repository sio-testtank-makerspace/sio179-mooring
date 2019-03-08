#include "application.h"
#line 1 "c:/Users/pingu/Documents/SIO179/StorageSD/src/StorageSD.ino"
/*
 * Project StorageSD
 * Description: Code testing error logging, data storage on MicroSD
 * Author: Padraig MacGabann
 * Date: 2/25/2019
 */

#include "SdFat.h"

// Prep for turning off cellular (for testing), getting battery details, and incrementing/toggling
void setup();
void loop();
#line 11 "c:/Users/pingu/Documents/SIO179/StorageSD/src/StorageSD.ino"
SYSTEM_MODE(MANUAL);
FuelGauge batteryMonitor;
int count = 0;
int led1 = D0;
bool led_state = LOW;

// Pick an SPI configuration
#define SPI_CONFIGURATION 0
//----
// Setup SPI configuration.
#if SPI_CONFIGURATION == 0
SdFat sd;
const uint8_t chipSelect = SS;
#endif //SPI_CONFIGURATION
//----

File myFile;

void setup() {
  //TODO: generate a new filename each time this runs
  // How about, fileName = User_Appended_File_Name & "_" & DateTime.Now.ToString("yyyyMMdd_HH_mm_ss")
  // this seems good: LOG_2019_etc....
  // TODO: ensure that Time. is related to realtime and is not damaged during clock drift, time reset, etc.

  Cellular.off();
  pinMode(led1, OUTPUT);
  delay(5000);
  // Wait for USB Serial
  while(!Serial){
    SysCall::yield();
  }

  Serial.println("Type any character to start");
  while(Serial.read() <= 0){
    SysCall::yield();
  }

  // Initialize SdFat or print a detailed error message
  // and halt. Use half speed like the native library.
  // Change to SPI_FULL_SPEED for more performance.
  if(!sd.begin(chipSelect, SPI_HALF_SPEED)){
    sd.initErrorHalt();
  }

  //String fileName = String( "LOG_" + Time.year() + Time.month() + Time.day() + Time.hour() + Time.minute() + Time.second() );
  //Serial.println(fileName);
  // Open the file for write at end like the "Native SD library"
  if(!myFile.open("e1.txt", O_RDWR | O_CREAT | O_AT_END)){
    sd.errorHalt("opening e1.txt for write failed (1)");
  }
  // If the file opened okay, write to it:
  Serial.print("Writing to e1.txt...");
  myFile.println("testing 1, 2, 3.");
  myFile.printf("fileSize: %d\n", myFile.fileSize());

  // Close the file:
  myFile.close();
  Serial.println("done.");

  // Re-open the file for reading: 
  if(!myFile.open("e1.txt", O_READ)){
    sd.errorHalt("opening e1.txt for read failed (2)");
  }
  Serial.println("e1.txt content:");

  // Read from the file until there's nothing else in it
  // Show data from file over serial. NOTE THAT IT CAN TAKE A FAIRLY LONG TIME 
  // TO WRITE AND PUSH TO SERIAL THIS DATA (i.e. 1-3 min) - PM 3/5/19 10:29am
  int data;
  while((data = myFile.read()) >= 0){
    Serial.write(data);
  }

  // Close the file
  myFile.close();

  // Shut off serial port in hopes that it will prevent
  // breaking stuff.
  delay(5000); // Give Serial.write(---) enough time to print out everything

  // ^ delay(5000) is, in other words, for enough time after flashing 
  // to connect over serial via tera term, etc. - PM 3/5/19 10:31am
  
  Serial.end();
}

void loop() {
  // Create vars for battery params and buffer for SD card
  float cellVoltage = batteryMonitor.getVCell(); 
  float stateOfCharge = batteryMonitor.getSoC();
  char buf[100];

  /* 
  The battery params are just an example for something that could
  be written to the buffer. In the final seapHOx implementation,
  we will push the parameters of the instrument & particle that are
  relevant to us - PM 3/5/19 10:34am 
  */ 

 // Open the file for write at end like the "Native SD Library"
 if(!myFile.open("e1.txt", O_RDWR | O_CREAT | O_AT_END)){
   sd.errorHalt("opening e1.txt for write failed (3)");
 }

 // Write into buffer the increment and battery params
 snprintf(buf, sizeof(buf), "%d, %.02f. %.02f", count, cellVoltage, stateOfCharge);
 myFile.println(buf);

 myFile.close();

 // Toggle LED, increment counter, delay 100ms
  digitalWrite(led1, !led_state);
  count++;
  delay(100);
}