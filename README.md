<h1> BEStie: bridge between Bosch Electric System (BES) bikes and fitness tracking gear.</h1>

<div align="center">
  <img width="500" alt="BEStie in action" src="https://github.com/user-attachments/assets/7b57be8b-d07e-4baf-ab5b-b62df10c717c" />
</div>

Stream cycling performance metrics straight to your watch or cycling computer in real time. If your existing fitness gear can work with smart trainer over BLE it will work with BEStie.

## A bit of context (the problem)

Bosch ebikes are able to measure rider power and cadence.

That's enough to track one's fitness progress without investing into power metering pedals (expensive) or crank mounted sensors (impossible on most Octalink cranks).

On 4th May 2026 Bosch issued an update for bikes equipped with Performance CX Gen 4/5 and Performance SX motors allowing telemetry access over Bluetooth Low Energy via **Live Data Interface (LDI)** protocol.

However the way LDI peer device must be implemented makes **adoption into existing devices difficult**. At the time of writing only a handful of Garmin Edge computers adopted the spec.

## Solution - use a translator

This is where BEStie comes in. A protocol translator firmware based on cheap and widely available boards with on Nordic nRF5x MCUs.

- On one end **spec compliant Live Data Interface peer**. Just open Flow app and click 'add component'. New 'bike display' named 'BEStie' will appear. Connect. Done.

- On the other end 'smart trainer' (BLE FTMS Indoor Bike) that **streams power, speed and cadence straight to your existing fitness gear** (cycling computer, smartwatch, anything really).

    If it understands Zwift compatible trainers it will just work with BEStie. Add a sensor, go for a ride. That's it.

Afterwards **every ride is seamless** - power on your bike, start activity tracking on your fitness device. Enjoy data being there when you need it.

## Why BEStie?

Compared to other projects using LDI and similar platform BEStie gives you:

- **highly accurate data** - going with FTMS format means no data conversion. Power and cadence values match Kiox display and Flow app recordings exactly.

    For comparison using Cycling Power Service means converting exact values in RPM, Watts and kph into abstract magnet pulses of virtual bicycle wheel. This is an imperfect process and result never is an exact match to the source data.

- **vendor agnostic compatibility** - in the age of Zwift smart trainer support is common. Instead of waiting for your device to be updated to support Bosch LDI spend an evening once and focus on riding.

- **ultra low power operation** - current version draws 60 µA in idle and 140 µA while riding. That's months of operation on coin cell battery and way over a year on 1000mA single cell LiPo. Moreover it translates to **peace of mind - just throw a board running BEStie into your saddle bag and forget about it for the whole riding season**.

- ability to connect BEStie to **multiple fitness devices at once** (currently 2). Recording activity on a watch and seeing immediate values and alerts on a head unit? No problem.

- **seamless switching between bikes** - you can add single BEStie board to up to 10 bikes at once. As long as only one is turned on at a time everything will just work. I'm constantly switching between my Bosch CX MTB rig and SX eGravel without need to touch anything.

## AI generated content disclosure

_**Lovingly handcrafted with years of Nordic nRF5 and Bluetooth Low Energy expertise.**_

This project **does not contain any AI/LLM generated code** and I'd like to keep it that way. LLMs were occasionally used as lint tools.

## One more thing

I hate to ask but if you like the project please consider leaving a GitHub star ⭐. It will help more than you think.


<h1>Table of Contents</h1>

- [Supported hardware](#supported-hardware)
	- [Nordic nRF52 boards](#nordic-nrf52-boards)
		- [What to choose](#what-to-choose)
	- [Bosch Smart System bikes](#bosch-smart-system-bikes)
	- [Fitness tracking gear](#fitness-tracking-gear)
- [Getting started](#getting-started)
	- [Installation](#installation)
		- [Method for devices equipped with Adafruit bootloader (the easy way)](#method-for-devices-equipped-with-adafruit-bootloader-the-easy-way)
			- [Put your device into bootloader mode.](#put-your-device-into-bootloader-mode)
			- [Update the bootloader (first BEStie install on given board)](#update-the-bootloader-first-bestie-install-on-given-board)
			- [Install BEStie firmware .hex (also works for bootloader update)](#install-bestie-firmware-hex-also-works-for-bootloader-update)
			- [Verifying that firmware is installed correctly](#verifying-that-firmware-is-installed-correctly)
	- [Connecting BEStie to Bosch bike](#connecting-bestie-to-bosch-bike)
		- [Add BEStie to Bosch bike as accessory](#add-bestie-to-bosch-bike-as-accessory)
	- [Connecting BEStie to cycling computer / smart watch](#connecting-bestie-to-cycling-computer--smart-watch)
- [Troubleshooting / Frequently asked questions](#troubleshooting--frequently-asked-questions)

# Supported hardware

## Nordic nRF52 boards

There's no hard platform dependencies. Any board equipped with Nordic nRF52832 or nRF52840 will work. Adding other nRF52 family chips is also possible.

Specific supported board will be needed if you want to track BEStie's battery levels (coming soon, stay tuned). Generic version that works on any nRF52832 or nRF52840 will be always available alongside it.

So far following boards were tested:

| Board | Notes |
|-------|-------|
| Nordic Dev Kits nRF52832-DK ([PCA10040](https://www.nordicsemi.com/Products/Development-hardware/nRF52-DK)) / nRF52840-DK ([PCA10056](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-DK)) - for developers | <ul><li>Reference development platform</li><li>Best radio performance (no problems with smartwatches)</li><li>Coin cell battery holder</li><li>Easy enough to flash once you've set up Nordic tools</li></ul> |
| [Adafruit Feather nRF52840 Express](https://www.adafruit.com/product/4062) - **recommended** for beginners | <ul><li>Easy firmware updates thanks to integrated USB bootloader</li><li>Easy single cell LiPo battery integration with on board JST-PH connector</li><li>Acceptable radio performance</li></ul> |
| [Seeed Studio XIAO nRF52840](https://www.seeedstudio.com/Seeed-XIAO-BLE-nRF52840-p-5201.html) - cheapest but problematic | <ul><li>Easy firmware updates thanks to integrated USB bootloader</li><li>Adding battery requires soldering tiny wires</li><li>**Bad RF performance resulting in unreliable connections with smartwatches**</li></ul> |

### What to choose

For best possible first time experience, I'd highly recommend picking Adafruit Feather Express and single cell 500-1000mAh LiPo battery with JST PH connector. This way easy installation method is available and no soldering is required. <br> Make sure to check battery connector polarity against markings on Adafruit Feather board and swap pins around if necessary.

## Bosch Smart System bikes

Supported motor list:
- Performance line CX gen 4, motor model BDU374x
- Performance line CX gen 5, motor model BDU384x
- Performance line SX, motor model BDU314x
- other Smart System bikes that work with Flow app and got Live Data Interface update (4th May 2026)

Bike must be updated to the latest firmware and paired with the up to date Flow app.

## Fitness tracking gear

Your fitness tracking device must support smart cycling trainer over BLE (BLE FTMS profile). If your gear works with Zwift compatible trainer verify that it connects over Bluetooth Low Energy and if yes it will work with BEStie.

# Getting started

## Installation

### Method for devices equipped with Adafruit bootloader (the easy way) 
- [Adafruit Feather nRF52840 Express](https://www.adafruit.com/product/4062) 
- [Seeed Studio XIAO nRF52840](https://www.seeedstudio.com/Seeed-XIAO-BLE-nRF52840-p-5201.html) 
- anything from [this list](https://github.com/bestie-org/Adafruit_nRF52_Bootloader/blob/master/supported_boards.md) that has nRF52840 MCU

#### Put your device into bootloader mode. 

<ul>
<li>Adafruit Feather <br> Plug in USB cable. Push and hold both buttons. Let go of reset button and keep holding the other one. Wait for new USB drive to appear. </li>
<li>Seeed Studio XIAO <br> Press and hold tiny SMD button next to USB port. Plug in USB cable. Wait for USB drive to appear.</li>
</ul>

_In case of using other board look up button sequence to enter bootloader in the board documentation._

Afterwards verify that you have new USB drive with following files:
- `CURRENT.UF2`
- `INDEX.HTM`
- `INFO_UF2.TXT`


If not try again, sometimes it takes a couple of attempts.


#### Update the bootloader (first BEStie install on given board)

Check `INFO_UF2.TXT` contents on USB drive. It should look similar to this:

```
UF2 Bootloader <version>
Model: Seeed XIAO nRF52840
Board-ID: nRF52840-SeeedXiao-v1
Date: Jun 22 2026
SoftDevice: S140 X.Y.Z
```

If `Softdevice` line says `SoftDevice: S140 7.3.0` bootloader is up to date and you can proceed to the next step - installing BEStie firmware.

Otherwise time to update the bootloader:
1. take note of `Model:` line in `INFO_UF2.TXT`
2. find it in [this list](https://github.com/bestie-org/Adafruit_nRF52_Bootloader/blob/master/supported_boards.md) and note corresponding name in `Board` column.
3. Go to [BEStie bootloader release](https://github.com/bestie-org/Adafruit_nRF52_Bootloader/releases/latest/) and find .hex file named according to `Board` column contents and ending in `_s140_7.3.0.hex`

   * For Adafruit Feather Express `Board` is `feather_nrf52840_express` and bootloader is named `feather_nrf52840_express_bootloader-vX.Y.Z_s140_7.3.0.hex`
   * For Seeed Studio XIAO `Board` is `xiao_nrf52840_ble` and bootloader is named `xiao_nrf52840_ble_bootloader-vX.Y.Z_s140_7.3.0.hex `

   	**Warning:** Be very careful about picking correct bootloader variant. 

   	For example there's Feather and Feather Express, Xiao and Xiao Sense. 

   	**Flashing wrong variant may result in unbootable board**. Not updating bootloader to 7.3.0 softdevice version will result in BEStie firmware crashing on boot.

4. Once you have a correct file go to the next step. 

	Flash bootloader file first using the same instructions as for the main firmware. Then proceed with flashing BEStie firmware file.

#### Install BEStie firmware .hex (also works for bootloader update)

**Make sure Softdevice 7.3.0 bootloader is installed** by following the steps above. Otherwise BEStie firmware will fail in unpredictable manner or outright crash.

In case of doing bootloader update you can proceed regardless.

<div align="center">
	<video src="https://github.com/user-attachments/assets/885ea862-84bc-48cb-891e-cf43fd68cbda" width="640" controls></video>
</div>

1. Get BEStie firmware .hex from [release page](https://github.com/bestie-org/BEStie/releases/latest). File should be named `bestie_nrf52840.hex`
2. Verify that the board is still in bootloader mode (there's a USB drive with `INFO_UF2.TXT` file as described above). Bootloader access times out after few minutes of inactivity
3. Go to [BEStie firmware uploader](https://bestie-org.github.io/adafruit-uploader/) page. You'll need a browser that supports WebSerial API (Chromium or recent release of Firefox)
4. Click 'Connect Serial' button. 
	- On first run there may be prompts asking you to allow this website to access USB devices or similar. Allow that and remember for the future.
	- There should be exactly one port on the list (by default there's filter for known bootloader devices)
5. Click 'Single .hex (auto detect)' and pick your .hex file
	- in case of doing bootloader update pick bootloader .hex and proceed. After update is done go back and do the steps with BEStie firmware .hex
	- click 'Upload' and wait. For bootloader update wait additional 30s after upload is done for flash write to happen.

#### Verifying that firmware is installed correctly
1. after unplugging USB and plugging it back board should boot to application. No USB drive. 

	If USB drive appears instead, bootloader is there and fails to boot the application code.
2. Get Nordic nRF Connect app 
	* [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en)
	* [iOS](https://apps.apple.com/us/app/nrf-connect-for-mobile/id1054362403)
3. Scan for devices. There should be one named 'BEStie'. You can filter by name. Tap the icon next to the name so red star appears and you can find it later by selecting 'show preferred devices' in filter menu.
4. try connecting to it if you like. 

	There will be 'Fitness Machine' service inside. 
	
	If you're doing this at later point with bike connected the app will decode data stream with power, speed and cadence. <br> Updates will start after enabling notifications (triple down arrow button next to FTMS characteristic inside the service). 

## Connecting BEStie to Bosch bike

### Add BEStie to Bosch bike as accessory
On fresh boot BEStie enters enhanced pairing mode that boosts likelihood of successful pairing with the bike. This mode ends on first successful ebike connection (just after pairing). 

It's best to power cycle BEStie board before adding new bike. All old pairings will be retained.

Pairing mode draws 90 µA vs 60 µA in normal advertising. After a power cycle, BEStie stays in pairing mode until it reconnects to a paired bike. To switch back to normal advertising, turn on a paired bike for at least 60 seconds.

<div align="center">
  <video src="https://github.com/user-attachments/assets/10be3567-08a9-4f4b-857e-d38123ba41cc" width="640" controls></video>
</div>

1. make sure BEStie firmware is installed and can be found by nRF Connect scan
2. bring BEStie, bike and phone with Flow app close together, about arms reach would be best. 
3. Optimally do the pairing away from other fitness devices like HR straps or cycling sensors. Flow app scans for and tries to connect to each device in random order. Often it gives up just after single try/device.
4. Make sure that only the bike you intend pair with BEStie is on
5. Select the correct bike in Flow app
6. Tap the gear icon -> components -> blue 'Add component' link
7. Select 'Accessories'
8. Scanning process will start
	- Flow 'add component' process is not the best. It's true even with officially supported Garmin devices. We can't do anything about it. Be persistent, good luck.
	- phone model plays a large role in how fast and reliable this is. 
	Worst case I've seen so far was 20 failed attempts with officially supported Garmin Edge 1050. Rebooting the phone and the bike multiple times eventually helped.
	- single scan can take up to 2 minutes. Be patient and let it run till Flow app gives up. Repeat scanning 2-3 times before rebooting anything.
	- If it fails check if you see BEStie on nRF Connect scan. If yes the firmware is working. Keep trying, phone reboot may help.
9. Eventually new device named 'BEStie' will appear looking like a cycling computer. Click 'Connect'.
10. Go to component list again, tap BEStie. Wait for connection dot to go green. It confirms that the bike is fully bonded now. That can take up to 90s on the first try. Once devices exchange security keys connections will happen much quicker and everything will automatically reconnect in the background.
	<div align="center">
    	<img width="500" alt="this is how connected device looks like" src="https://github.com/user-attachments/assets/010bc9d5-3270-4eed-ad33-50a3c00382ee" />
	</div>

## Connecting BEStie to cycling computer / smart watch

<div align="center">
  <video src="https://github.com/user-attachments/assets/35f18573-002e-4d13-82c6-0c13134a58eb" width="640" controls></video>
</div>

The exact process will vary depending on your device. On Garmin Edge Explore 2 and Enduro 3 the process looks as follows:
1. It's best to pair with the bike first and connect it with BEStie. Once ebike connection is established BLE advertising switches to focused FTMS advertising. <br> If you can't do it in this order scanning time will be longer and some devices might need more than one attempt. Keep trying.
2. Find 'Sensors' item in drop down shade or watch settings menu under 'Connectivity'
3. Tap 'Add new' -> 'Search all'
4. After up to 90s new smart trainer device named 'BEStie' will appear on the list. Tap '+' button next to it. 
	* Some firmware versions may show an empty list with 'scan for Bluetooth devices' button. Tap the button and wait.
5. 'Connecting to new device' screen will appear. This can take a long while on older devices like Edge Explore 2. Go grab a coffee and let it complete.
6. Check list of sensors. BEStie should be there as a smart trainer device.
7. Do a test 'ride'

	<div align="center">
		<video src="https://github.com/user-attachments/assets/7eea4aee-39db-48ad-ab9e-7a87e6a05aef" width="640" controls></video>
	</div>
	  
	- make sure you have instantaneous speed and cadence values added to your data screens
	- turn off your fitness device and the bike to force everything to disconnect. Keep BEStie on.
	- turn on your fitness device, start an activity. Look at speed and cadence fields. They should show '--' and after 30-60s 'trainer connected' message should appear followed by a beep.
	- if not go to the sensor tab and see what it says next to BEStie. It should be 'searching', followed by 'connected'.
	- if BEStie is connected turn on the bike watching the data screen. Values should go from '--' to '0.0' indicating that bike telemetry stream is up. <br>This can take up to a minute but usually is faster. Initial connection takes longer than subsequent reconnects for specific bike and power cycle.
	- if it's safe to do so spin the cranks at steady pace for about 10s. Speed should go from 0 to some value and after a moment of steady cranking bike will have enough data to indicate cadence. <br> Power can't be checked in the work stand. But if the other values work it will work as well on a real ride.

# Troubleshooting / Frequently asked questions

1. __No matter what I do my fitness device can't find BEStie. Bike connection works.__
   
	Verify that you see BEStie in nRF Connect app scan as described above. If that's the case and you still can't pair it with your fitness gear it may be lacking support for BLE FTMS protocol or in layman's terms Bluetooth connected Zwift-compatible Smart Trainers. In such case I'm sorry for wasting your time. 
	
	This is a fundamental design choice. I could implement Cycling Power Service for better compatibility. I did a lot of work to make that happen. But no matter what data quality was very bad. 
	
	Bosch bike sends exact values of power in Watts, cadence in RPM and speed in kph. FTMS protocol allows BEStie to relay the data exactly as they come. That results in clean, reliable ride recordings with 1:1 sample matching.
	
	Bluetooth Cycling Power Service (CPMS) and Cycling Speed and Cadence Service (CSC) work with assumption that your data source is a rim magnet and no processing on sensor side is possible. <br> They expect BEStie to send clicks of magnet passing hall effect sensor on each wheel rotation. That turns into a case of simulating rotation of virtual bicycle wheel. Add relatively low LDI data rate into the mix and BEStie suddenly needs to do some serious data extrapolation to keep Garmin from going into 'no data' between samples. <br> As a result values are either responsive but filled with noise that's not in the source or slow to respond and overly smoothed cutting away sudden hard pushes on the cranks. In either case not good enough for tracking rider performance.

2. __Everything works at home during test 'ride' but connection drops frequently outside.__ <br> *alternatively:* __It works with my bike computer but fails with smart watch__ 
	
	This may be related to radio performance problem. If devices work in calm environment while being in close physical proximity firmware installation, pairing and connection setup is done correctly.

	Bluetooth operates in 2.4GHz band. Human body is a perfect barrier for this frequency. If you put BEStie board in your saddle bag or backpack radio waves need to go trough your body. This requires board with good antenna hardware. Smaller boards may struggle. Xiao BLE was giving me trouble in this regard. 
	
	It does not help that smartwatches use lower performance antennas due to size constraints.<br> I've spent weeks chasing what seemed to be a bug in BEStie's code. It was working perfectly with Edge bike computer but failed to stay connected every time I went outside with Enduro 3 watch. Finally I've discovered that Xiao BLE does not have enough 'oompf' to get the signal trough. Swapping for Adafruit Feather solved the problem instantly.

3. __I'm seeing different average power/cadence trough BEStie compared to Kiox display__

	Bosch has chosen to calculate displayed averages in 'ignore zero values' mode. By default Garmin devices include zero values in their averages. This is configurable and can be set either way in fitness device settings. 
	
	Cyclists have different opinions which is the right setting. It seems MTB crowd likes to exclude zero values while roadies tend to want all samples including zeros.

	In any case BEStie does the right thing and achieves transparency forwarding all data values exactly as sent by the bike.

4. __I've set my zero exclusion for power and cadence correctly and averages work now. But immediate power still does not match between Kiox and BEStie__

	Different fitness devices do different things with displayed data. 
	
	For example Garmin Edge 1050 shows matching, quickly updating power values in numeric 'Power' field but applies post processing in numeric display on 'Power Graph'. What you're looking for to verify is data field showing truly instantaneous power, speed and cadence without any smoothing. 

5. __When I suddenly push hard power and cadence match Kiox with slight 1-2s delay. Can you make it update faster?__

	Sadly not faster than where it is now. I've squeezed every bit of latency on BEStie's side that was possible to eliminate while keeping power usage low.

    <div align="center">
      <video src="https://github.com/user-attachments/assets/d3b7a0fc-1c93-461f-851e-9b81835ae51c" width="640" controls></video>
    </div>

   Latency in this case is complicated:
	* BLE protocol works in discrete time quants. 
  		
		That means there are specific time windows when each device is allowed to send/receive over radio. BEStie is a BLE peripheral meaning it has no say how many radio windows per second it gets. BLE central devices - bike, head unit, smart watch dictate connection parameters and send 'data transfer possible now' packets. <br> BEStie provides a set of parameters that can be ignored by the central. If BEStie's configuration is respected communication happens every 100-200ms

		Moreover two time quants are needed per data update. First one is LDI packet coming from the bike. Then BEStie works hard to catch the next possible window and reflect data over FTMS to bike computer or a watch. Overall BLE latency is around 200-400ms or 0.4s.
	
	* Bosch LDI updates happen relatively infrequently
		
		* Speed every 1-2s 
		* Cadence every 2-4s
		* Power every 1-2s

		This is controlled by code running on bike's side.
	
	* Kiox display updates are much more responsive making overall impression worse

		Relevant data is updated on internal CANbus (wired network inside the bike) multiple times per second. I don't have any measurements but it looks like Kiox gets new set of data multiple times per second and apparent response is instant. LDI data are sent on much lower cadence.

	* Garmin updates and records data at fixed 1Hz interval

		This puts lower latency limit at 1.4s (1s for data field update + 2 radio windows).

7. __I've compared recordings from Flow app against BEStie in tool like [Compare the watts](https://compare-the-watts.com/) or [Quantified Self](https://quantified-self.io) and it shows bad data correlation/quality. Why do you claim BEStie gives good quality data?__

	Existing tools assume single radio hop between sensor and recording device. This fundamentally impacts results. In single hop scenario samples are in sync relatively to each other. This means that once you find a match on small portion of the data there's a constant time offset between recording devices.

	In two radio hop scenario (Bike --> BEStie --> Recording device) we have random jitter because BLE works in discrete and relatively long radio communication intervals. <br> Every sample may 'miss the train' and have to wait going from bike to BEStie and then miss another one going from BEStie to recording device. Or hop on closest one and wait for the other. Or hit best possible connection window twice. This is random and sample timings seen by multiple recording devices can shift in relation to each other in both directions at any time. 
	
	Rest assured, sample order stays the same. **Power, cadence, speed curve shapes match exactly within local jitter value. Averages match within 1-2W/RPM over whole rides between BEStie and Kiox.**
	
	`*.fit` files have timestamp granularity of 1s. It partially masks jitter and static offset may work for multiple samples before shifting to another value. This confuses existing tools even more. 

	As a result method of static sample shifting won't work and will indicate bad data correlations (about 80%). **When you account for jitter correlation suddenly jumps to >99%**. Remaining 1% is caused by multiple samples arriving in single 1s data recording window. Those tend to be averaged somehow.

	If you still don't trust it I've created [a set of scripts](https://github.com/bestie-org/fit-align) that you can feed with your `*.fit` files and see the matched data, graphs and correlation values yourself.

8. __Can you add battery range tracking, assist mode indicator, < insert a feature here > ?__

	Not today. BEStie is a Bosch Live Data Interface (LDI) peer. Spec is [here](proto/20260501_LiveDataInterface_V1_28042026.pdf). If by some miracle you can pass feedback about missing features to relevant team at Bosch that would be greatly appreciated by the whole community. And it would help everyone implementing LDI spec.
	
	Bosch exposes following values over LDI:
	* Speed
	* Battery State of Charge (SOC)
	* Rider Power
	* Cadence
	* Odometer
	* Unix Time
	* Light Status
	* Ambient Brightness
	* Light Reserve mode
	* System Lock
	* Bike Not Driving
	* Charger Connected
	* Diagnostic probe Connected

	Also Bluetooth Low Energy lacks standardized way of exposing ebike related data. Remember that we don't control fitness tracking gear firmware. This limits BEStie to using standard BLE profiles and services or the other side won't know how to parse the data.
