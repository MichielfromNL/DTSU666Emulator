## DTSU PV emulator

I am running a 10kW home battery unit which is not directly connected to an inverter. My energy provider runs a smart algorithm that charges & discharges according to pricing, my use, available  PV energy.
But for that, the battery unit needs to have an extra meter for the PV data: amps, watts, voltage. which requires an extra CHint DTSU666 3-phase meter  
My  meter/utility closet is already quite packed and there is ample room for (yet) another 3-phase meter. 
So I decided to emulate the second CHINT DTSU666, make it a Modbus RTU slave (server) that gets it data from other sources, which is possible because the  PV data I need is already available from the inverter.

![DTSU emulator schematics](/doc/Schematics1.png)
![DTSU emulator box](/doc/20240701_110313.jpg)


The data source that I have is is a modified Growatt dongle, which I equipped with new "open" firmware whoch publishes its data to MQTT.  https://github.com/Alkhateb/Growatt_ShineWiFi-X - this works quite well
The stick is configured to publish to my local Mosquitto server which runs in a docker container on my Synology diskstation.

# Software
I created an ESP8266 sketch which simply subscribes to MQTT PV topic from Growatt, and makes it available for a a Modbus RTU server on the ESP: a wemos-mini D1, with a Max485 breakout providing RS485. That is all.

For the emulation I wrote a DTSU666 class (see the header and cpp in lib) which mimics all registers of the DTSU666 meter, making it accurate replica.  The only data that is missing (data I don't have) is reactive power, but for PV that is obviously not a problem, zero is just fine.

Configuration is done with the excellent WiFimanager libray so that a zero config setup works well. Using wifimanager makes it resilient : when wifi goes away, power is cycled, or MQTT is no available, the unit keeps retrying.
I added a button with a builin led which flashes when new MQTT data is fed, and which can also act to enter (re)-confifuration mode e.g. when Wifi or the MQTT have to be changed. 

The solution is quite simple but works perfect. Code is here for grabs.

If you need to adapt the code for another PV source:  the only thing to do is change the MQTT callback depending on the data that you get. Alternatively, you could get data via HTTP requests but that is not something that I would recommend, reason: an HTTP request is blocking and can take several seconds, during that time the modbus server cannot serve data to the battery unit. MQTT data is immediate (msecs) since data is pushed over an esisting connection
I tried async TCP, but that turns out not to be very stable in combination with modbus RTU. So MQTT over TCP/IP is perfect

The unit has OTA so that I can update whenever a change is required.

Happy emulating !
