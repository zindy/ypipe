# The Y-Pipe project
A hardware device (STM32duino based) for spying on RS232 communications.

## Introduction
The Y-Pipe is a device that replaces the RS232 link between the *original controller* and the *device*, and can be used for injecting commands from a
second *substitute controller* while simultaneously logging traffic between all three machines.

![Y-Pipe principle](docs/ypipe_principal.gif)

## Hardware
So far, the Y-Pipe has been tested on the [Blue Pill](https://stm32duinoforum.com/forum/wiki_subdomain/index_title_Blue_Pill.html) and on the [Maple Mini](https://stm32duinoforum.com/forum/wiki_subdomain/index_title_Maple_Mini.html). Both of these are STM32F103 based devices.

![Blue Pill and Maple Mini pinout](docs/bluepill_vs_maplemini.png)

The reason for using a STM32F103 based device is the fabulous [USBComposite_stm32f1 library](https://github.com/arpruss/USBComposite_stm32f1) which is part of the [Arduino_STM32](https://github.com/rogerclarkmelbourne/Arduino_STM32) ) library and allows the microcontroller
board to be seen as two separate USB serial devices on the substitute controller, one to inject commands and one for logging all the traffic.

RS232 communication from the device and to the original controller is handled through any kind of [MAX3232 clone module](https://www.sparkfun.com/products/11189) as long as the sockets and plugs match. 
Ultimately, it doesn't matter how this is wired as long as (most likely case), DCE goes to the PC and DTE goes to the device (see https://components101.com/connectors/rs232-connector).

![DCE and DTE pinouts](https://components101.com/sites/default/files/component_pin/RS232-Connector-Pinout.png)

[Electrically isolated modules](https://www.aliexpress.com/wholesale?SearchText=RS232+232+to+TTL+power+isolation) do exist and should be used to prevent ground loops and protect the original equipment.

# Use case scenarios
## Logging traffic between original controller (RS232) and device (RS232)
This could be used to spy on the traffic being exchanged between the controller and device.
This is especially important when the original controller is an older machine or possibly something else entirely and all you can tap into is the original RS232 link.

## Logging USB to Serial traffic
On the Y-Pipe, the substitute controller link (USB) is completely symmetrical and can be used to simultaneously send commands to the device and on the same machine,
log the traffic that comes back. For example, you want to reverse engineer an illumination device with an RS232 port and the control software works fine on a modern
machine using a USB to RS232 adapter. In this case, the Y-Pipe *replaces* the adapter and gives you a second serial port to log the traffic.

## Command mismatch
This may require some programming, but it is possible to replace substrings on the fly.
This is useful when either the original controller or the device sends strings which make the original controller misbehave.
Another use is when the controller was designed with a different device from the one you have and maybe changing the identification string sent back by the device unlocks
the controller software.

## Comparing the original software traffic to that of an alternative stack
Through the Y-Pipe and **using a lot of caution**, it is possible to control the device using two software stacks simultaneously and compare the responses sent back by the device. Original, Substitute and Device traffic is labelled as such through the logging port.

## TODO: Two separate applications on the same computer controlling a single device
This would require an additional USB device (so a &Psi;-Pipe?) and would be interesting to use when the overall control software (Micro-Manager, Metamorph, ...) only provides some basic level of integration, but the manufacturer of the device has a much more compelling bit of software. Unfortunately in most cases (unless the device has a USB and RS232 port that can be used simultaneously), you can only use one at a time. Using the ideas here however, a USB to RS232 widget could be built to provide control from both applications independently, because two separate USB serial ports would be provided. A 3rd one, the serial console, would be provided to select RS232 baud-rate and delimiter (see below).

# Compilation
The Y-Pipe software relies on Roger Clarke's Arduino_STM32 core libraries and can easily be compiled and uploaded via the Arduino IDE. Very briefly, install the Arduino SAM boards (Cortex-M3) and clone the [Arduino_STM32 repository](https://github.com/rogerclarkmelbourne/Arduino_STM32) into your Arduino/hardware folder. Full instructions in the [Arduino_STM32 wiki](https://github.com/rogerclarkmelbourne/Arduino_STM32/wiki/Installation).

For the maple mini, the best way I found to upload via the bootloader is to first install a libusb-win32 device. To do this, put the maple mini in "perpetual bootloader mode" (press reset, press the other button when fast blinking, should continue to fast blink).

At this point, if the maple mini appears as an unknown device in other devices:

![Unknown device](docs/before_zadig.png)

You can use [Zadig](https://zadig.akeo.ie/) to install a generic libusb driver for when you need to talk to the maple mini in bootloader mode:

![Zadig install](docs/doing_zadig.png)

... after which this is what the Maple003 device looks like in the Device manager:

![Zadig install](docs/after_zadig.png)

Other than that, everything needed to compile Y-Pipe should be already installed and compilation and upload (via the Bootloader) can be done in the Arduino IDE.

# Usage
Console mode is entered by pressing ENTER. This stops the logging of traffic from the controllers and device until console mode is exited by typing 'x'.

Two things can be changed and saved in the Y-Pipe EEPROM memory:
* 'b' on its own in a terminal window displays the current baud rate between controller and device. b+BAUDRATE (e.g. 'b115200') changes the current baud rate.
Valid options are 9600, 19200, 38400, 57600 and 115200.
*  'd' on its own displays the current delimiter value: '\\r', '\\n', '\\r\\n'. To change the delimiter, issue 'dr', 'dn' or 'drn'.

Additional commands are:
* 'H' for help
* 'W' for who am I? which displays each role on the corresponding terminal window (if connected)
* 'x' to exit the console mode.

