# HODLcube
Cryptocurrency ticker connected to Coinbase Pro

### List of parts
- copy of **Wemos D1 mini V3.0** ([link](https://www.aliexpress.com/item/32831353752.html)) or newer **Lolin D1 mini V3.1.0** ([link](https://www.aliexpress.com/item/32529101036.html)). Buying original version with big silver plate and FCC logo on it won't fit
- OLED 1.3" 128x64 display ([link](https://www.aliexpress.com/item/32683094040.html))
- active 5V buzzer ([link](https://www.aliexpress.com/item/4001112067716.html))
- 1 pc green LED ([link](https://www.aliexpress.com/item/33015212696.html))
- 1 pc red LED
- 1 pc 15R resistor ([link](https://www.aliexpress.com/item/32922643786.html)) for green LED; resistance may vary, see specifications of your LED
- 1 pc 51R resistor for red LED; resistance may vary, see specifications of your LED
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
1. Print the top part of the HODLcube from white translucent filament. [White PETG from Sunlu](https://www.aliexpress.com/item/32957585470.html)) is recommended. It depends on your your experience. This part has to be translucent but don't have to be transparent. LED light have to go through easily.
2. Print the rest five parts from PLA/PETG filament with colour of your choice

### Soldering, see scheme for cable lengths and colours
1. Solder LEDs and resistors on top of cutted 6x14 PCB
2. Solder buzzer and all wires on the other side of PCB
3. All negatives (buzzer, resistors should lead to one negative wire)
4. Now solder all wires (positive green LED, positive red LED, positive buzzer, common negative wire) from this PCB to holes in D1 mini board
5. Solder 4 Dupont male cables for display

![schema_hodlCube_en_bb](https://user-images.githubusercontent.com/80581925/138918036-4ffadb0c-3e09-42b0-924e-afa4061b0a62.png)

### Test
1. Connect Dupont cables to display 
2. Double check correct wiring
3. Connect D1 mini using micro USB cable to PC
4. Download
   - .bin file from [releases](https://github.com/sergejbubko/HODLcube/releases) and use esptool to flash it or visit this [page](https://nerdiy.de/en/howto-esp8266-mit-dem-esptool-bin-dateien-unter-windows-flashen/) for more information about esptool and its alternative with GUI called [ESPEasyFlasher](https://github.com/BattloXX/ESPEasyFlasher)
   - OR .zip file of this project and using [Arduino IDE](https://www.arduino.cc/en/software) compile it and upload to D1 mini, nice tutorial can be found [here](https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/) with one difference - select correct board in Board Manager called **LOLIN(WEMOS) D1 R2 & mini**. You also need to include all libraries. You should find them in Library Manager in Arduino IDE or collect them manualy by downloading neccessary archives from github. Links are provided in sourcecode HODLcube.ini next to each included library.

### Assembly
1. Push the LEDs into top 3D printed part the way it is not overlap the part itself
2. Put the D1 mini board onto bottom part and use two smaller screws to catch it on place
3. The display should snap on front part, use two bigger screws on upper part
4. Now put left, bottom, top and right part together. If it doesn't fit well use sharp knife or sandpaper. The reset button on D1 mini should face the small hole in left part.
5. Snap on the front part. If it is too hard to snap it in use drill bit and adjust holes for the pins.
6. Connect all 4 wires to display and snap in tha back part.

