/*
 * Copyright (C) 2020 Egor Zindy ezindy@gmail.com
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <EEPROM.h>
#include <USBComposite.h>

#define BOARD_LED_PIN PB1

// That's a large number! Serial strings shouldn't really be that long.
#define MAXLENGTH 50

// If it takes any device more than 5000s to construct a string, something went wrong
#define SERIAL_TIMEOUT 5000

USBMultiSerial<2> ms;

// Some strings we'll be using for display stuff. These are stored in PROGMEM
const uint32_t baudRates[] PROGMEM = {9600, 19200, 38400, 57600, 115200};
const char delimMsg[] PROGMEM = "C> delim set to \\";
const char baudMsg[] PROGMEM = "C> Baud rate between RS232 devices set to ";
const char welcomeMsg[] PROGMEM = "C> This is the console, type H for help (C> console, U> USB, S> serial, D> device)";
const char helpMsg[] PROGMEM = "C> Use W to locate all four consoles\r\n" \
    "C> d for delimiter (dr for \\r, dn for \\n, drn for \\r\\n\\r\\n)\r\n" \
    "C> b for baudrate (b9600, b19200, b38400, b57600,  b115200)";

// These are used for parsing the serial input (a and B are USB, 1 and 2 are RS232).
String rxMsgA = "";
String rxMsgB = "";
String rxMsg1 = "";
String rxMsg2 = "";

uint32_t elapsed_tA;
uint32_t elapsed_tB;
uint32_t elapsed_t1;
uint32_t elapsed_t2;
uint32_t baud_rate;

// Delimiter is at EEPROM address 0
//    '\n' or '0x0A' (10 in decimal) -> This character is called "Line Feed" (LF).
//    '\r' or '0x0D' (13 in decimal) -> This one is called "Carriage return" (CR).
//     0 is '\r' 1 is '\n' 2 is '\r\n'

uint8_t delim_index = 0;

// This is not a char to account for \r\n
String delim = "\r";

void printDelim(uint8_t index) {
    ms.ports[1].print(delimMsg);
    switch(index) {
        case 1:
            ms.ports[1].println("n");
            break;
        case 2:
            ms.ports[1].println("r\\n");
            break;
        default:
            ms.ports[1].println("r");
    }
}

void setup() {
    // initialize the digital pin as an output:
    pinMode(BOARD_LED_PIN, OUTPUT);

    ms.begin();

    while (!USBComposite);
    ms.ports[1].println(helpMsg);

    // initialising the delimiter
    delim_index = EEPROM.read(1);
    switch(delim_index) {
        case 1:
            delim = "\n";
            break;
        case 2:
            delim = "\r\n";
            break;
        default:
            delim = "\r";
            delim_index = 0;
    }
    printDelim(delim_index);

    // initialise the baudrate
    uint8_t ee_index = EEPROM.read(0);
    if (ee_index > 4) ee_index = 4;

    baud_rate = baudRates[ee_index];
    Serial1.begin(baud_rate); Serial2.begin(baud_rate);
    ms.ports[1].print(baudMsg); ms.ports[1].println(baud_rate);
}

void loop() {
    char s;
    uint32_t now = millis();

    digitalWrite(BOARD_LED_PIN, !digitalRead(BOARD_LED_PIN));

    // USB debug port (port1)
    while (ms.ports[1].available() > 0) {
        elapsed_tB = now;
        s=(char)ms.ports[1].read();
        if (s == '\n' || s == '\r') {
            uint8_t tl = rxMsgB.length();
            if (tl == 0) continue;

            if(rxMsgB=="H")  { 
                // The help message
                ms.ports[1].println(helpMsg);
            } else if(rxMsgB=="W") {
                // Who does what?
                ms.ports[0].println("USB");
                ms.ports[1].println("CONSOLE");
                Serial1.println("Ser1");
                Serial2.println("Ser2");
            } else if(rxMsgB.startsWith("b")) {
                if (tl > 1) {
                    uint32_t br = rxMsgB.substring(1).toInt();
                    for (uint8_t i=0; i < 5; i++) {
                        if (br == baudRates[i]) {
                            Serial1.begin(baud_rate);
                            Serial2.begin(baud_rate);
                            baud_rate = br;

                            // Here we write the baudrate index to the EEPROM
                            EEPROM.update(0, i);

                            break;
                        }
                    }
                }
                ms.ports[1].print(baudMsg);
                ms.ports[1].println(baud_rate);
            } else if(rxMsgB.startsWith("d")) {
                uint8_t index = delim_index;
                if (tl > 1) {
                    if(rxMsgB=="dr") {
                        // delim change
                        delim = "\r";
                        index = 0;
                    } else if(rxMsgB=="dn") {
                        // delim change
                        delim = "\n";
                        index = 1;
                    } else if(rxMsgB=="drn") {
                        // delim change
                        delim = "\r\n";
                        index = 2;
                    }
                }
                printDelim(index);
                if (index != delim_index) {
                    delim_index = index;
                    EEPROM.update(1, delim_index);
                }
            }

            // A command was issued. Clear the input buffer
            rxMsgB = "";
        } else {  
            rxMsgB += s; 
            if (rxMsgB.length() >= MAXLENGTH) {
                rxMsgB = "";
            }
        }
    }

    // USB controller to device (port0)
    while (ms.ports[0].available() > 0) {
        elapsed_tA = now;
        s=(char)ms.ports[0].read();
        if (s == '\n' || s == '\r') {
            if (rxMsgA.length() == 0)
                continue;

            // Goes to device...
            Serial2.print(rxMsgA + delim);

            // Debug means we want to know what's going on...
            ms.ports[1].write("U> ");
            ms.ports[1].println(rxMsgA);

            // Clear the input buffer
            rxMsgA = "";
        } else {  
            rxMsgA += s;
            if (rxMsgA.length() >= MAXLENGTH) {
                rxMsgA = "";
            }
        }
    }

    // Serial1 is Serial controller to device
    while (Serial1.available() > 0) {
        elapsed_t1 = now;
        s=(char)Serial1.read();

        if (s == '\n' || s == '\r') {
            if (rxMsg1.length() == 0)
                continue;

            // Goes to device...
            Serial2.print(rxMsg1 + delim);

            // Debug means we want to know what's going on...
            ms.ports[1].write("S> ");
            ms.ports[1].println(rxMsg1);

            // Clear the input buffer
            rxMsg1 = "";
        } else {  
            rxMsg1 += s;
            if (rxMsg1.length() >= MAXLENGTH) {
                rxMsg1 = "";
            }
        }
    }

    // Serial device to both controllers
    while (Serial2.available() > 0) {
        elapsed_t2 = now;
        s=(char)Serial2.read();

        if (s == '\n' || s == '\r') {
            if (rxMsg2.length() == 0)
                continue;

            // XXX Replace the following substring in the reply
            // rxMsg2.replace("BD_EP","AAAS");

            // Goes to USB controller...
            ms.ports[0].print(rxMsg2 + delim);

            // Goes to serial controller...
            Serial1.print(rxMsg2 + delim);

            // Debug means we want to know what's going on...
            ms.ports[1].write("D> ");
            ms.ports[1].println(rxMsg2);

            // Clear the input buffer
            rxMsg2 = "";
        } else {  
            rxMsg2 += s;
            if (rxMsg2.length() >= MAXLENGTH) {
                rxMsg2 = "";
            }
        }
    }

    if (now - elapsed_t1 >= SERIAL_TIMEOUT) {
        rxMsg1 = ""; elapsed_t1 = now;
    }
    if (now - elapsed_t2 >= SERIAL_TIMEOUT) {
        rxMsg2 = ""; elapsed_t2 = now;
    }
    if (now - elapsed_tA >= SERIAL_TIMEOUT) {
        rxMsgA = ""; elapsed_tA = now;
    }
    if (now - elapsed_tB >= SERIAL_TIMEOUT) {
        rxMsgB = ""; elapsed_tB = now;
    }

    // Wait some
    delay(50);
}
