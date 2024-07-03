## DTSU PV emulator

I am running a 10kW home battery unit which is not directly connected to an inverter. My energy provider runs a smart algorithm that charges & discharges according to pricing, my use, available  PV energy.
But for that, the battery unit needs to have an extra meter for the PV data: amps, watts, voltage. which requires an extra CHint DTSU666 3-phase meter  
My  meter/utility closet is already quite packed and there is ample room for (yet) another 3-phase meter. 
So I decided to emulate the second CHINT DTSU666, make it a Modbus RTU slave (server) that gets it data from other sources, which is possible because all PV data is already present.

![DTSU emulator schematics](/doc/Schematics1.png)
![DTSU emulator box](/doc/20240701_110313.jpg)


The data source for the PV is a Growatt inverter, which I equipped with new firmware so that I can get at the PV data.  https://github.com/Alkhateb/Growatt_ShineWiFi-X works quite well
The stick is also an MQTT client which publishes its data to my local Mosquitto server which runs in a docker container on my Synology diskstation.

# software
I created an ESP8266 sketch to simply subscribe to MQTT and run a Modbus RTU server for the PV unit. The ESP is a wemos-mini D1, with a Max485 breakout. That is all.

For the emulation I wrote a DTSU666 class (see the header and cpp in lib) which mimics all registers of the DTSU666 meter, making it accurate replica.  The only data that is missing (data I don't have) is reactive power, but for PV that is obviously not a problem, zero is just fine.

Configuration is done with the excellent WiFimanager libray so that a zero config setup works well. Using wifimanager makes it also resliient : when wifi goes away of MQTT is no available, the unit keeps retrying.
I added a button with a builin led which flashes when new MQTT data is fed, and which can also act to enter (re)-confifuration mode e.g. when Wifi or the MQTT changes 

The solution works perfect. Code is here for grabs.

If you need to adapt the code for another PV source:  the only thing to do is change the MQTT callback depending on the data that you get. Alternatively, you could get data via HTTP requests but that is not something that I would recommend, reason: an HTTP request is blocking and can take several seconds, during that time the modbus server cannot serve data to the battery unit. MQTT data is immediate (msecs) since data is pushed over an esisting connection
I tried async TCP, but that turns out not to be very stable in combination with modbus RTU. So MQTT is perfect

The unit has OTA so that I can update whenever a change is required.

Happy emulating !
