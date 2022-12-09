# HODLcube [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/U7U86QNA9)
Cryptocurrency ticker connected to Coinbase [Buy on Etsy](https://www.etsy.com/listing/1205181193/crypto-ticker-display-hodl-cube?click_key=10d35aab0e97a46f12506ed9d97eca05aedc7571%3A1205181193&click_sum=b9edafc7&ref=shop_home_active_1&pro=1&frs=1)

[Quick Start Guide](https://docs.google.com/document/d/1Tg4MM1jNnUUqZixxY0ZYTSoNqewVLtktd7ysAPqDA4k/edit?usp=sharing) can be found on Google Docs

<img src="https://user-images.githubusercontent.com/80581925/139483396-476c0094-bfa1-4d6b-baa0-acef892c5fc1.jpeg" width="40%">

### List of parts
- **Wemos D1 mini V3.0** ([link](https://www.aliexpress.com/item/32831353752.html)) or newer **Lolin D1 mini V3.1.0** ([link](https://www.aliexpress.com/item/32529101036.html)). Original version with big silver plate and FCC logo on it won't fit!

![Inkedd1mini_LI](https://user-images.githubusercontent.com/80581925/138943582-9db08b18-763e-4650-a285-195ba3918348.jpg)

- OLED 1.3" 128x64 display ([link](https://www.aliexpress.com/item/32683094040.html))
- active 5V buzzer ([link](https://www.aliexpress.com/item/4001112067716.html))
- 1 pc green LED 5 mm ([link](https://www.aliexpress.com/item/33015212696.html))
- 1 pc red LED 5 mm
- 1 pc 15R resistor 0.25 W ([link](https://www.aliexpress.com/item/32922643786.html)) for green LED; resistance may vary, see specifications of your LED
- 1 pc 51R resistor 0.25 W for red LED; resistance may vary, see specifications of your LED. You can use a calculator like [this](https://www.digikey.com/en/resources/conversion-calculators/conversion-calculator-led-series-resistor)

- female to female dupont cables ([link](https://www.aliexpress.com/item/1699285992.html))
- 28AWG wires ([link](https://www.aliexpress.com/item/4001178609999.html)) 24AWG - 28AWG should be fine, using colours depends on you. You can also reuse dupont cables with removed connectors
- self-tapping screws with (optional) collar
  - 2 pcs 2.3x4 mm ([link](https://www.aliexpress.com/item/1005001508219065.html))
  - 2 pcs 2x6 mm
- piece of PCB, with 6x14 holes - cut PCB size 20x80mm with 6x28 holes in half with shears or bench shear ([link](https://www.aliexpress.com/item/1005002011006453.html))

### You will also need
- 3D printer ([link](https://www.aliexpress.com/item/1005002378407440.html))
- PLA and/or PETG filaments ([link](https://www.aliexpress.com/item/32957585470.html))
- soldering iron ([link](https://www.aliexpress.com/item/32903921345.html))
- solder
- patience

### 3D printing
Parts are free to download on [thingiverse](https://www.thingiverse.com/thing:5031142)
1. Print the top part of the HODLcube from white translucent filament. [White PETG from Sunlu](https://www.aliexpress.com/item/32957585470.html) is recommended. It depends on your experience. This part has to be translucent but don't have to be transparent. LED light have to go through easily.
2. Print the rest five parts from PLA/PETG filament with colour of your choice
<img src="https://user-images.githubusercontent.com/80581925/140551939-ab6cee3a-f493-4de6-b735-c59fc113ee39.jpg" width="30%">


### Soldering
*see scheme for cable lenghts and colours*
1. Solder LEDs and resistors on top of cutted 6x14 PCB
<img src="https://user-images.githubusercontent.com/80581925/138932617-5946caba-80f2-4115-8bdf-ee23dc072721.jpg" width="30%">
     
3. Solder buzzer and all wires on the other side of PCB
<img src="https://user-images.githubusercontent.com/80581925/138932644-cbe0d278-c18f-4c3b-8ec5-2e0aa3b7c32a.jpg" width="30%">

5. All negatives (buzzer, resistors and also display GND dupont cable should lead to one spot with negative wire leading to GND of D1 mini)
6. Now solder all wires (positive green LED, positive red LED, positive buzzer, common negative wire) from this PCB to holes in D1 mini board
7. Solder 4 Dupont male cables for display
<img src="https://user-images.githubusercontent.com/80581925/138932764-0c4b71d2-4adf-4e2c-9bf3-c57892522d69.jpg" width="30%">

![schema_hodlCube_en_bb](https://github.com/sergejbubko/HODLcube/blob/main/wiring%20diagram/schema_hodlCube_en_bb.png)

### Test
1. Connect Dupont cables to display (see scheme)
2. Double check correct wiring
3. Connect D1 mini using micro USB cable to PC
4. Download
   - .bin file from [releases](https://github.com/sergejbubko/HODLcube/releases) and use esptool to flash it or visit this [page](https://nerdiy.de/en/howto-esp8266-mit-dem-esptool-bin-dateien-unter-windows-flashen/) for more information about esptool and its alternative with GUI called [ESPEasyFlasher](https://github.com/BattloXX/ESPEasyFlasher)
   - OR .zip file of this project and using [Arduino IDE](https://www.arduino.cc/en/software) compile it and upload to D1 mini, nice tutorial can be found [here](https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/). In Arduino IDE > Tools don't forget to check that you have selected:
     - correct board in Board > ESP8266 boards > **LOLIN(WEMOS) D1 R2 & mini**.
     - **115200** upload speed 
     
     You also need to include all libraries. You should find them in Library Manager in Arduino IDE or collect them manualy by downloading neccessary archives from github. Links are provided in sourcecode HODLcube.ino next to each included library.

### Assembly
1. Push the LEDs into top 3D printed part. For correct orientation see image below.
<img src="https://user-images.githubusercontent.com/80581925/138945501-86f0fe05-5246-4b36-ae59-ccf337954e6b.jpg" width="30%">

2. Put the D1 mini board onto bottom part and use two smaller screws to catch it on place
3. The display should snap on front part, use two bigger screws on upper part
<img src="https://user-images.githubusercontent.com/80581925/138945571-8786f4f3-8d94-48c7-be95-d5d9cf62ed10.jpg" width="30%">

4. Now put left, bottom, top and right part together. If it doesn't fit well use sharp knife or sandpaper. The reset button on D1 mini should face the small hole in left part.
5. Snap on the front part. If it is too hard to snap it in use drill bit and adjust holes for the pins.
6. Connect all 4 wires to display and snap in tha back part.

<img src="https://user-images.githubusercontent.com/80581925/138945622-ba44e183-e98a-4d2c-88a3-53cd99da2b25.jpg" width="30%">


