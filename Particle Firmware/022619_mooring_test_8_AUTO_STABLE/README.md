# 031419_MOORING_TEST_AUTO_STABLE 

Device OS is updated to V1.0.1, firmware is written for the new OS, but should be backwards compatible.

### Some Changes: 

- particle.connect() previously had no breakout condition, potentially leading to infinite loops upon deterioration of cell signal. This has been fixed now.

- millis() overflow was fixed in the new device OS. However, for backwards compatiblity, the firmware now uses Time.local() instead of millis()

- Added expanded serial monitoring capability for determining where errors occur

- USB comms now turn on before PUBLISH_STATE is called. This improves monitoring ability.

- Added additional comments to the code

### Normal Behavior

- Wakes up and prints out a line
- Runs through various setup protocols
- Connects to the cloud
- Parses SeapHOx and publishes the data
- Sets a sleep time, and goes to deep sleep

### Issues:

- At the moment, if the device cannot connect to the internet, it occasionally goes into an infinite loop, although it usually times out within 5 minutes. However, it is unclear as to how to fix this as this is a background process issue. 
- There appears to be a parsing issue with the SeapHOx that is causing a hang to occur intermittely without a breakout. Either the serial is not ending and restarting properly, or parsing is occurring when the SeapHOx is not ready.
- There exists no timeout condition for SeapHOx right now if a data connection has been established, but the serial is corrupted.
- If the parsing hangs, the Particle cannot obtain data and publish. If manually reset, the board is able to publish again, but then runs into the hang issue. 

### Note that:

This code stands alone. It may still require the user to create a /src/ file, project.properties, and other support files on their own; for instance thru the CLI. 
