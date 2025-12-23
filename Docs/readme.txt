#
# STC DIY Clock Kit firmware based on Jens J.(https://github.com/zerog2k/stc_diyclock)
# and Anatoli Klassen's work (https://github.com/dev26th/stc_diyclock_gps)
#
# This version was reworked by Wierzbowsky [RBSC] in December 2025 for the older DIY clock kits
# There are 2 different versions of the firmware: for normal and inverted logic LED displays
#
# Version 1.00
#

This is the firmware replacement for the older STC15F mcu-based DIY Clock Kit from AliExpress around 2015-2023.
Use [SDCC](http://sdcc.sf.net) to build the new firmware file or flash the pre-compiled firmware with the official
stc-isp tool into the STC15F204EA series microcontroller.


## Features

Basic functionality:

* time display/set 24 hour mode
* display seconds
* date display/set in DD/MM (adjusted by Wierzbowsky)
* year display/set in YYYY (added by Wierzbowsky)
* display auto-dim (adjusted by Wierzbowsky)
* display weekday (number 1-7) (adjusted by Wierzbowsky)
* temperature display/set in °C (adjusted by Wierzbowsky)
* user-defined alarm with on/off switch
* hourly chime for the selected hour range with on/off switch
* 20-second beeping alarm can be stopped by pressing any switch


## Hardware

* DIY LED Clock kit, based on STC15F204EA and DS1302
* Connection to PC via any USB-UART adapter, e.g. CP2102, CH340G (USB-to-TTL)
* There are different firmwares for normal and inverted LED displays (see below)


## Build Requirements
* SDCC installed and added into the PATH variable

The installer can be found in the Software folder of this repository.


## Using Clock with new Firmware

By default, when no switches are pressed, the indication will automatically cycle between:
time -> temperature -> date -> year -> day of the week (number from 1 to 7)

If compiled with default options, pressing of S1 (the upper switch) on the start screen will cycle between:
set hour -> set minute -> set alarm hour -> set alarm minute -> alarm on/off -> chime start hour -> chime stop hour -> chime on/off

Use S2 (the lower switch) to change corresponding values, the current value blinks. If all values blink, the S2 switch will
enable or disable the chime or alarm. An enabled chime or alarm will be indicated by the dot near the rightmost LED indicator.
On the start screen the last dot indicates whether the alarm is on or off.

Pressing S2 switch on the start/main screen will cycle between:
temperature -> date -> year -> weekday -> seconds

To change values, press S1 switch on the corresponding setup screen. The temperature can be adjusted with +/- 10 degrees
range. The year can be adjusted from 2025 to 2050. When pressing the S1 on the seconds screen, the seconds value becomes
zero.

When the alarm sounds, it can be suppressed by pressing any of the 2 switches. The alarm will not be snoozed, it will only
repeat in 24 hours unless it is disabled by a user.


## How to Flash the Firmware

IMPORTANT! First of all you need to identify that your DIY kit is suitable for this firmware. Modern DIY kits use different
pinouts and different LED panels, so the provided firmwares are not suitable for them! There are 2 images in the Docs folder:
board.jpg and board_back.jpg. See if the images match your kit's circuit board. If not, do not attempt to program the firmware
or you may ruin your DIY kit!

The second important thing is the type of the LED panel. There are 2 types: 8041A and 8041B. The A version uses normal logic,
the B version uses the inverted logic. See the LED_panel.jpg image to see where to find the markings on the LED panel. There
are 2 main.hex files in separate folders named Normal and Inverted. Use the main.hex from the Normal folder for A version and
main.hex from the Inverted folder for B version. These are different firmwares and they will not work if flashed into improper
version of the DIY kit.

To program the firmware into the chip you need to have an UART USB dongle. Connect the power cable to the computer or a USB hub
and then connect the UART into the nearby USB port. Your UART will be visible in the system as one of COM ports, for example
COM4. Then you need to start the firmware programming utility - it can be found in the Software folder. The file name of the
utility is stc-isp-v6.91K.exe but you can use any similar or later version of this utility.

In order to program the custom firmware for the first time, you need to enable certain features in the utility, otherwise
programming will fail. See the program.jpg and erase.jpg images to see what options need to be set. Use the Open Code File
button to load the suitable main.hex file. You are now ready.

Connect the USB UART to your DIY kit's board as shown on the UART_cable.jpg image. Use 2-pin header and press it firmly against
the board. The blue wire is RXD signal, the green wire is TXD signal. Do not connect the power yet. Click on Download/Program
button and only then connect the power to the board. The software will flash the firmware into the STC chip, see the progress.jpg
image for reference. When the programming is finished, disconnect the UART from your DIY kit board. Done!

If you wish to reprogram the firmware after you flashed it into the microcontroller, you can deselect the Program OR checkbox,
see the reprogram.jpg for reference.

Finally, set the correct time, date, month and year, adjust the temperature to match your current room's temperature and set
the houly beep or the alarm to the desired time. The new firmware is way more powerful than the original one that came with
the oder DIY kits. The older firmware could only show time and temperature.


## Clock Assumptions

Some of the code assumes 11.0592 MHz internal RC system clock (set by stc-isp or stcgal).


## Disclaimer

This code is provided as-is, with NO guarantees or liabilities.
As the original firmware loaded on an STC MCU cannot be downloaded or backed up, it cannot be restored.
If you are not comfortable with experimenting, I suggest obtaining another blank STC MCU and using this to test, so that you can
move back to original firmware, if desired.


## References
STC15f204ea English datasheet:
http://www.stcmcu.com/datasheet/stc/stc-ad-pdf/stc15f204ea-series-english.pdf

Wierzbowsky's private Github repository:
https://github.com/wierzbowsky

The kit's schematics can be found in the Docs folder: see the schamatics.jpg file.
