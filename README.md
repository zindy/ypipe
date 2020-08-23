# The Y-Pipe project
A hardware device (STM32duino based) for spying on RS232 communications.

## Introduction
The Y-Pipe is a device that replaces the RS232 link between the *original controller* and the *device*, and can be used for injecting commands from a
second *substitute controller* while simultaneously logging traffic between all three machines.

![Y-Pipe principle](https://raw.githubusercontent.com/zindy/ypipe/master/docs/ypipe_principal.gif)

## Hardware
So far, the Y-Pipe has been tested on the [Blue Pill](https://stm32duinoforum.com/forum/wiki_subdomain/index_title_Blue_Pill.html) and on the [Maple Mini](https://stm32duinoforum.com/forum/wiki_subdomain/index_title_Maple_Mini.html). Both of these are STM32F103 based devices.
RS232 communication from the device and to the original controller is handled through any kind of [MAX3232 clone module](https://www.sparkfun.com/products/11189) as long as the sockets and plugs match.
[Electrically isolated modules](https://www.aliexpress.com/wholesale?SearchText=RS232+232+to+TTL+power+isolation) do exist and should be used to prevent ground loops and protect the original equipment.

The reason for using a STM32F103 based device is the fabulous [USBComposite_stm32f1 library](https://github.com/arpruss/USBComposite_stm32f1) which allows the microcontroller
board to be seen as two separate USB serial devices on the substitute controller, one to inject commands and one for logging all the traffic.

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

# Compilation
**TODO** Expand this section.

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

