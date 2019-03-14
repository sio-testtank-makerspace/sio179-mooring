/*
 * Project glast_StorageSD
 * Description: Integration of original commit Particle glast code with 
 *              SD Storage & logging capabilities
 * Author: Padraig MacGabann
 * Date: 3/12/2019
 */

#include "SdFat.h" //TODO: update includePath
#include "Particle.h"
// #include "Serial5/Serial5.h" // if we use Asset Tracker

SYSTEM_MODE(MANUAL); 
//TODO: see how this affects the original glast. 
//probably only in AUTOMATIC for testing purposes 

// Global objects
FuelGauge batteryMonitor;
PMIC pmic;

// Prep for  getting battery details, and incrementing/toggling
int count = 0;

// Boolean variables, important info for SD Log - P.M. 3/14/19 11:17am
bool isConnected = false; // default false
bool transmitSuccessful = false // default false

// Pick an SPI configuration
#define SPI_CONFIGURATION 0
//----
// Setup SPI configuration.
#if SPI_CONFIGURATION == 0
SdFat sd;
const uint8_t chipSelect = SS;
#endif //SPI_CONFIGURATION
//----

// Define SD Log file
File myFile;

// Forward declarations
void parseSeapHOx(); // parse response from SeapHOx of "ts" "gdata" or "glast" command from Electron

// This is the name of the Particle event to publish for battery or movement detection events
// It is a private event.
const char *eventName = "CpHOx1";

// Various timing constants
const unsigned long MAX_TIME_TO_PUBLISH_MS = 60000; // Only stay awake for 60 seconds trying to connect to the cloud and publish
const unsigned long TIME_AFTER_PUBLISH_MS = 4000; // After publish, wait 4 seconds for data to go out
const unsigned long TIME_AFTER_BOOT_MS = 5000; // At boot, wait 5 seconds before going to sleep again (after coming online)
const unsigned long SLEEP_TIME_SEC = 1800; // Deep sleep time
const unsigned long TIMEOUT_SEAPHOX_MS = 5000; // Max wait time for SeapHOx response

// SeapHOx struct for response variables
String s;
struct SeapHOx {
  int Sample_Number;     //0
  char *Board_Date;       //1
  char *Board_Time;         //2
  float Main_Batt_Volt;   //3 yes V
  float V_Therm;          //4
  float V_FET_INT;        //5
  float V_FET_EXT;        //6
  float isolated_power_Volt ;//7
  float Controller_Temp;//8
  float Durafet_Temp;   //9 yes C
  float V_Pressure;     //10
  float pHINT;          //11 yes pH
  float pHEXT;            //12 yes pH
  float Counter_Leak;   //13
  float Substrate_Leak;   //14
  float Optode_Model;   //15
  float Optode_SN;      //16
  float O2uM ;          //17 yes [µM]
  float O2_Saturation ; //18
  float Optode_Temp;    //19
  float Dphase;         //20
  float Bphase;         //21
  float Rphase;         //22
  float Bamp;           //23
  float Bpot;           //24
  float Ramp;           //25
  float Raw_Temp;       //26
  float SBE37_Temp;     //27 yes C
  float SBE37_Cond;     //28 yes cm^^-1
  float SBE37_Salinity; //29 yes s
  float SBE37_Date;     //30
  float SBE37_Time;     //31
};
SeapHOx SeapHOx_Cell;

// For the finite state machine
enum State { PUBLISH_STATE, SLEEP_STATE };
State state = PUBLISH_STATE;

unsigned long stateTime = 0;
unsigned long lastSerial = 0;

void setup() {
  // Set charging current to 1024mA (512 + 512 offset) (charge faster!):
  pmic.setChargeCurrent(0,0,1,0,0,0);

  // Start here when waking up out of SLEEP_MODE_DEEP
  state = PUBLISH_STATE;
  
  // Turn on USB comms
  Serial.begin(9600); //TODO: remove? PM 3/14/19 11:19am

  // Setup MyFile:
  // Initialize SdFat or print a detailed error message and halt. Use half speed like the native library.
  // Change to SPI_FULL_SPEED for more performance.
  if(!sd.begin(chipSelect, SPI_HALF_SPEED)){
    sd.initErrorHalt();
  }
  
  // Open the file for write at end like the "Native SD library"
  if(!myFile.open("log.txt", O_RDWR | O_CREAT | O_AT_END)){
    sd.errorHalt("opening log.txt for write failed (1)");
  }
 
  myFile.println(Time.now()%86400);   //myFile timestamp println
  myFile.println("Awake. Turn cell on."); // Remap Serial to myFile - PM 3/14/19 11:10am

  // Close file. Has to be closed and reopened in loop(){} - PM 3/14/19 12:23pm
  myFile.close();

  // SeapHOx serial; wait TIMEOUT_SEAPHOX_MS for a line to arrive
  Serial1.setTimeout(TIMEOUT_SEAPHOX_MS);
}

void loop() {
//////////////////////////////////////////////////////////////////////////////
  // Enter state machine
  switch(state) {

  //////////////////////////////////////////////////////////////////////////////
  /*** PUBLISH_STATE ***/
  /*** Get here from PUBLISH_STATE. Ensure that we're connected to Particle Cloud.
  If so, poll SeapHOx, parse response, and send that and GPS info to cloud then
  go to SLEEP_STATE
  If not connected, still poll SeapHOx and print out response over USB serial then go to SLEEP_STATE.
  ***/
  case PUBLISH_STATE: {
    Particle.connect();
    // Poll SeapHOx:
    // Clean out any residual junk in buffer and restart serial port
    Serial1.end();
    delay(1000);
    Serial1.begin(115200);
    delay(500);

    // Open the file for write at end like the "Native SD Library"
    if(!myFile.open("log.txt", O_RDWR | O_CREAT | O_AT_END)){
      sd.errorHalt("opening log.txt for write failed (2)");
    }

    // Get data in file after current file pointer
    Serial1.println("glast");
    myFile.println(Time.now()%86400);   //myFile timestamp println
    myFile.println("glast"); // Duplicate Serial1 in myFile - PM 3/14/19 11:12am

    // Read SeapHOx response
    s = Serial1.readString();			// read response
    String s2 = s.replace("Error.txt f_read error: FR_OK\r\n", "");
    const char* s_args = s2.c_str();
    char* each_var = strtok(strdup(s_args), "\t");

    myFile.println(s2); // Remap Serial to myFile - PM 3/14/19 11:10am

    // Parse SeapHOx response
    parseSeapHOx(each_var);

    // Put Electron, SeapHOx, and GPS data into data buffer and print to screen
    char data[120];
    float cellVoltage = batteryMonitor.getVCell();
    float stateOfCharge = batteryMonitor.getSoC();
    snprintf(data, sizeof(data), "%s,%s,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.02f,%.02f",
              SeapHOx_Cell.Board_Date, SeapHOx_Cell.Board_Time,
              SeapHOx_Cell.Main_Batt_Volt, SeapHOx_Cell.V_FET_INT,
              SeapHOx_Cell.V_FET_EXT, SeapHOx_Cell.V_Pressure,
              SeapHOx_Cell.pHINT, SeapHOx_Cell.O2uM,
              SeapHOx_Cell.SBE37_Temp, SeapHOx_Cell.SBE37_Salinity,
              cellVoltage, stateOfCharge
            );
    myFile.println(Time.now()%86400);   //myFile timestamp println
    myFile.println(data); // Remap Serial to myFile - PM 3/14/19 11:10am

    
    // Save state of Particle.connected at this instant - PM 3/14/19 11:40am
    isConnected = Particle.connected();

    // If connected, publish data buffer
    if (Particle.connected()) {
      // Save state of Particle.publish at this instant - PM 3/14/19 11:40am
      transmitSuccessful = Particle.publish(eventName, data, 60, PRIVATE); 

      // Wait for the publish to go out by spinning here till enough time has elapsed
      stateTime = millis();
      while (millis() - stateTime < TIME_AFTER_PUBLISH_MS) {
        delay(10);
      }

      /* In case that publish fails - PM 3/14/19
      // TODO: should this replace "took too long to publish @line.227 ?"
      
      if(!transmitSuccessful){
        //Code would go here
      }

      */

      // Save the booleans to myFile and close the file before Particle goes to sleep - P.M. 3/14/19 11:59am
      myFile.println(Time.now()%86400);   //myFile timestamp println
      myFile.printf("isConnected = %s",(isConnected)?"true":"false");
      myFile.printf("transmitSuccessful = %s",(transmitSuccessful)?"true":"false");
      myFile.println("Going to sleep");
      myFile.close();
  
      state = SLEEP_STATE;
    }

    // If not connected after certain amount of time, go to sleep to save battery
    else {

      // Took too long to publish, just go to sleep
      if (millis() - stateTime >= MAX_TIME_TO_PUBLISH_MS) {
        
        // Save the booleans to myFile and close the file before Particle goes to sleep - P.M. 3/14/19 11:59am
        myFile.println(Time.now()%86400);   //myFile timestamp println
        myFile.printf("isConnected = %s",(isConnected)?"true":"false");
        myFile.printf("transmitSuccessful = %s",(transmitSuccessful)?"true":"false");
        myFile.println("Going to sleep");
        myFile.close();

        state = SLEEP_STATE;
      }
    }

  }
  break;

  //////////////////////////////////////////////////////////////////////////////
  /*** SLEEP_STATE ***/
  /*** Get here from PUBLISH_STATE and go to GPS_WAIT_STATE (if code makes it that far)
  or SLEEP_MODE_DEEP after calculating a wakeup time based off of the current time from the cloud.
  ***/
  case SLEEP_STATE: {
    delay(500);

    // Calculate sleep time
  	int nextSampleMin = 5; // sample at 5 past the hour
  	int currentHour = Time.hour();
  	int currentSecond = Time.now()%86400; // in UTC

    // Open the file for write at end like the "Native SD Library"
    if(!myFile.open("log.txt", O_RDWR | O_CREAT | O_AT_END)){
      sd.errorHalt("opening log.txt for write failed (2)");
    }

  	// Calculate seconds since midnight of next sample
  	int nextSampleSec = (currentHour+1)*60*60+nextSampleMin*60; // sample at this time
   	int secondsToSleep = nextSampleSec - currentSecond;   	
    
    // Save length of sleep in SD log before System.sleep - PM 3/14/19 12:32pm
    myFile.printf("Sleep for %d seconds\n", secondsToSleep); // Remap Serial to myFile - PM 3/14/19 11:10am
    myFile.close();

    System.sleep(SLEEP_MODE_DEEP, secondsToSleep);

    // It'll only make it here if the sleep call doesn't work for some reason
    stateTime = millis();
    state = PUBLISH_STATE;
  }
  break;

  }
}

void parseSeapHOx(char* new_var){
 	// Parse SeapHOx response
 	int count = 0;
 	char* parsed[300];

  if(!myFile.open("log.txt", O_RDWR | O_CREAT | O_AT_END)){
    sd.errorHalt("opening log.txt for write failed (2)");
  }

 	while (new_var != NULL) {
 		parsed[count] = new_var;
 		count++;
 		myFile.println(new_var); // Remap Serial to myFile - PM 3/14/19 11:10am
 		new_var = strtok(NULL, " \t");
 	}

 	if (parsed[0][8] == '#') {
 		SeapHOx_Cell.Board_Date 			= parsed[1];
 		SeapHOx_Cell.Board_Time 			= parsed[2];
 		SeapHOx_Cell.Main_Batt_Volt   = strtof(parsed[3], NULL);
 		SeapHOx_Cell.V_Therm          = strtof(parsed[4], NULL);
 		SeapHOx_Cell.V_FET_INT        = strtof(parsed[5], NULL);
 		SeapHOx_Cell.V_FET_EXT        = strtof(parsed[6], NULL);
 		SeapHOx_Cell.Durafet_Temp     = strtof(parsed[9], NULL);
 		SeapHOx_Cell.V_Pressure       = strtof(parsed[10], NULL);
 		SeapHOx_Cell.pHINT            = strtof(parsed[11], NULL);
 		SeapHOx_Cell.pHEXT            = strtof(parsed[12], NULL);
 		SeapHOx_Cell.O2uM             = strtof(parsed[17], NULL);
 		SeapHOx_Cell.O2_Saturation    = strtof(parsed[18], NULL);
 		SeapHOx_Cell.Optode_Temp      = strtof(parsed[19], NULL);
 		SeapHOx_Cell.SBE37_Temp       = strtof(parsed[20], NULL);
 		SeapHOx_Cell.SBE37_Cond       = strtof(parsed[21], NULL);
 		SeapHOx_Cell.SBE37_Salinity   = strtof(parsed[22], NULL);

    myFile.printf("\nParsed SeapHOX \n Date-time %s-%s\n Main_Batt_Volt %.5f\n V_Therm %.5f\n V_FET_INT %.5f\n V_FET_EXT %.5f\n Durafet_Temp %2.5f\n V_Pressure %.5f\n pHINT %.5f\n pHEXT %.5f\n O2 %.5f\n O2_Saturation  %.5f\n Optode_Temp %.5f\n SBE37_Temp %.5f\n SBE37_Cond %.5f\n SBE37_Salinity %.5f\n",
    							SeapHOx_Cell.Board_Date, SeapHOx_Cell.Board_Time,
    							SeapHOx_Cell.Main_Batt_Volt, SeapHOx_Cell.V_Therm ,
                  SeapHOx_Cell.V_FET_INT, SeapHOx_Cell.V_FET_EXT ,
                  SeapHOx_Cell.Durafet_Temp, SeapHOx_Cell.V_Pressure,
                  SeapHOx_Cell.pHINT, SeapHOx_Cell.pHEXT,
                  SeapHOx_Cell.O2uM, SeapHOx_Cell.O2_Saturation,
                  SeapHOx_Cell.Optode_Temp, SeapHOx_Cell.SBE37_Temp,
                  SeapHOx_Cell.SBE37_Cond, SeapHOx_Cell.SBE37_Salinity
                );//Remap Serial to myFile - PM 3/14/19 11:10am
    myFile.close();
  }
}