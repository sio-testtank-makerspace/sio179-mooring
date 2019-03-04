README

Device OS is updated to V1.0.1, firmware is written for the new OS, but should be backwards compatible.

Some Changes: 

-particle.connect() previously had no breakout condition, potentially leading to infinite loops upon deterioration of cell signal. This has been fixed now.

-millis() overflow was fixed in the new device OS. However, for backwards compatiblity, the firmware now uses Time.local() instead of millis()

-Added expanded serial monitoring capability for determining where errors occur

-The board has a new jumper cable grounding the WKP pin, so as to enable SLEEP_MODE_DEEP to work. Previously, the board was suffering from some issues where it would fall asleep and never wake up. 

-USB comms now turn on before PUBLISH_STATE is called

-At the moment, if the device cannot connect to the internet, it goes into an infinite loop. However, it is unclear as to how to fix this. 
