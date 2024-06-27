## DTSU PV emulator

I am running a 10kW home battery unit which is not directly connected to an inverter. My energy provider runs a smart algorithm that charges & discharges according to pricing, my use, available  PV energy.
But for that, the battery unit need s to have neter, and also PV data. 
However, my  meter closted is already packed and there is ample room for  (yet) anothe 3-phase meter. SO I decided to emulate the second CHINT DTSU666 and feed it to the battery uit, since all PV data is already present.

The data source for PV is a Growatt inverter, which I equipped with  ne w firmware so that I can get the PV data.  https://github.com/Alkhateb/Growatt_ShineWiFi-X
The stick is also an MQTT client, I push that to my local Mosquitto server , running in a docker container on my Synology diskstation

Next, I created an ESP8266 sketch to subscribe to MQTT and run a Modbus RTU server for the PV unit. The ESP is a wemos-misi D1, with a Max485 breakout. That is all.

I also created a DTSU666 class (see the header and cpp in lib) to mimic all registers of the  DTSU666 meter so that it is 100% emulation. 
The only thing missing (data I don't have) is reactive power, but for  PV that is obviously not a problem, zero is just fine.

Configuration is done with the excellent wifimanager so that a zero config setup works well. THat makes it also resliient : when wifi goes away of MQTT is no available, the unit keeps retrying.
Todo: add a button for on-demand configuration

The solution works perfect. Code is here for grabs.

If you need to adapt the code for another PV:  the only thing to do is change the MQTT callback depending on the data that you get. Alternatively, you could get data via HTTP requests, but that is not somethin that I would recommend. 
Reason: an HTTP request is blocking and can take several seconds. MQTT data is immediate since data is pushed over an esisting connection
I tried async TCP, but that turns out not to be very stable in combination with modbus RTU

Happy emulating !
