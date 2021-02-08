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

// Maple Mini
//#define BOARD_LED_PIN PB1
// Blue pill
#define BOARD_LED_PIN PC13

// That's a large number! Serial strings shouldn't really be that long.
#define MAXLENGTH 50

// If it takes any device more than 5000s to construct a string, something went wrong
#define SERIAL_TIMEOUT 5000

// EZ Added from the STM32F103 datasheet https://www.st.com/resource/en/reference_manual/CD00171190-.pdf in table 195
#define SERIAL_7E1	0B00000010
#define SERIAL_7O1	0B00000011

USBMultiSerial<3> ms;

// Some strings we'll be using for display stuff. These are stored in PROGMEM
#define N_SERMODES 10
const uint8_t serialModes[] PROGMEM = {SERIAL_8N1, SERIAL_8N2, SERIAL_9N1, SERIAL_9N2, SERIAL_8E1, SERIAL_8E2, SERIAL_8O1, SERIAL_8O2, SERIAL_7E1, SERIAL_7O1};
const char *serialModeNames[] PROGMEM = {"8N1", "8N2", "9N1", "9N2", "8E1", "8E2", "8O1", "8O2", "7E1", "7O1"};
uint8_t smIndex = 0;

#define N_BAUDRATES 8
const uint32_t baudRates[] PROGMEM = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
uint8_t brIndex = 0;

const char delimMsg[] PROGMEM = "delim set to ";
const char baudMsg[] PROGMEM = "Baud rate between RS232 devices set to ";
const char smMsg[] PROGMEM = "Serial mode set to ";
const char welcomeMsg[] PROGMEM = "C> Interactive console mode\r\nC> Logging is now disabled\r\nC> Type H for help, [ENTER] to exit";
const char helpMsg[] PROGMEM = "Use W to locate all four consoles\r\n" \
    "C> d for delimiter (\\r, \\n and \\xHH notation understood)\r\n" \
    "C> b for baudrate (b? for a list of modes)\r\n" \
    "C> m for serial mode (m? to display available modes)" \
    "C> Start string with > to send to device or < to controllers";

// These are used for parsing the serial input (A B and C are USB, 1 and 2 are RS232).
String rxMsgA = "";
String rxMsgB = "";
String rxMsgC = "";
String rxMsg1 = "";
String rxMsg2 = "";

uint32_t elapsed_tA;
uint32_t elapsed_tB;
uint32_t elapsed_tC;
uint32_t elapsed_t1;
uint32_t elapsed_t2;

// Console mode entered by pressing ENTER on an empty line
bool isConsole = false;

// Comment mode is when starting typing, will change after timeout or when ENTER was pressed
bool isTyping = false;

// isFaked when faking a response.
bool isFaked = false;

// Delimiter is at EEPROM address 2, stored as a string
//    '\n' or '0x0A' (10 in decimal) -> This character is called "Line Feed" (LF).
//    '\r' or '0x0D' (13 in decimal) -> This one is called "Carriage return" (CR).
String delim = "\r";

// https://circuits4you.com/2018/10/16/arduino-reading-and-writing-string-to-eeprom/
// Store are retrieve strings from the EEPROM
void writeString(uint8_t add,String data)
{
  uint8_t _size = data.length();

  for(uint8_t i=0; i<_size; i++) {
      EEPROM.write(add+i,data[i]);
  }
  EEPROM.write(add+_size,'\0');   //Add termination null character for String Data
}
 
String readString(uint8_t add)
{
    String temp = "";
    for (uint8_t i=0; i< 10; i++) {
        char k = (char)EEPROM.read(add+i);
        if (k == 0)
            break;
        temp += k;
    }
    return temp;
}

// https://forum.arduino.cc/index.php?topic=123486.0
// Used to convert a string to a hex number
int x2i(String s) {
    int x = 0;
    for (uint8_t i=0; i < s.length(); i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            x *= 16;
            x += c - '0';
        }
        else if (c >= 'A' && c <= 'F') {
            x *= 16;
            x += (c - 'A') + 10;
        }
        else if (c >= 'a' && c <= 'f') {
            x *= 16;
            x += (c - 'a') + 10;
        }
        else
            break;
    }
    return x;
}

String escapeString(String rxMsg) {
    String retString = "";
    for (uint8_t i=0; i<rxMsg.length(); i++) {
        char c = rxMsg.charAt(i);
        if (c >=32 && c<128)
            retString+=c;
        else {
            retString += "\\x";
            if (c < 16) retString += "0";
            retString += String(c, HEX);
        }
    }
    return retString;
}

// Convert 7 to 8 by stripping the MSB (parity bit) but only if smIndex is for 7 bit mode
String conv78(String rxMsg) {
    if (smIndex == 8 || smIndex == 9) {
        String retString = "";
        for (uint8_t i=0; i<rxMsg.length(); i++) {
            char c = rxMsg.charAt(i);
            c = c & 127;
            retString += c;
        }
        return retString;
    } else
        return rxMsg;
}

// Fake a response from a Hamilton MVP device (global var rxMsg2 for a command)
void fakeResponse(String rxMsg) {
/*
   //Uncomment this to fake a Hamilton MVP device compatible with the HamiltonMVP device adapter
    static uint8_t valvePos=0;

    isFaked = true;
    if (rxMsg == "1a" || rxMsg.substring(1,4) == "LXP") {
        //Do nothing, we're good?
        rxMsg2 = "";
    } else if (rxMsg.charAt(1) == 'F') {
        rxMsg2 = "Y";
    } else if (rxMsg.charAt(1) == 'U') {
        //send the firmware
        rxMsg2 = "M01.02.03";
    } else if (rxMsg.substring(1,3) == "E2") {
        //Query E2 to clear errors
        rxMsg2 = "\x50\x40\x50\x50";
    } else if (rxMsg.substring(1,4) == "LQT") {
        //Query the valve type (8 port distribution)
        rxMsg2 = "2";
    } else if (rxMsg.substring(1,4) == "LQP") {
        //Query the valve position
        rxMsg2 = "0"+String(valvePos+1);
    } else if (rxMsg.substring(1,4) == "LQA") {
        //Query the valve angle
        rxMsg2 = "000";
    } else if (rxMsg.substring(1,4) == "LQF") {
        //Query Valve Speed Request/Response
        rxMsg2 = "0";
    } else if (rxMsg.substring(1,3) == "LP") {
        valvePos = rxMsg.substring(4,5).toInt()-1;
        if (valvePos < 0) valvePos = 0;
        else if (valvePos > 7) valvePos = 7;
        rxMsg2 = "";
    } else {
        isFaked = false;
    }

    // The first response should be an exact echo
    // The second response ACK or NAK; ACK may also be followed by query
    if (isFaked) {
        rxMsg2 = rxMsg+delim+"\x06"+rxMsg2;
    }
*/
}

void printSerialModes(void) {
    ms.ports[2].write("Serial modes are: ");
    for (uint8_t i=0; i < N_SERMODES; i++) {
        ms.ports[2].write(serialModeNames[i]);
        ms.ports[2].write(" ");
    }
    ms.ports[2].write("\r\n");
}

void printBaudRates(void) {
    ms.ports[2].write("Baudrates are: ");
    for (uint8_t i=0; i < N_BAUDRATES; i++) {
        ms.ports[2].print(baudRates[i]);
        ms.ports[2].write(" ");
    }
    ms.ports[2].write("\r\n");
}

int8_t getSmIndex(String rxMsg) {
    int8_t ret = -1;
    for (int8_t i=0; i < N_SERMODES; i++) {
        if (rxMsg == serialModeNames[i]) {
            ret = i;
            break;
        }
    }
    return ret;
}

void setup() {
    // initialize the digital pin as an output:
    pinMode(BOARD_LED_PIN, OUTPUT);

    ms.begin();
    while (!USBComposite);

    // initialise the baudrate
    uint8_t ee_index = EEPROM.read(0);
    if (ee_index >= N_BAUDRATES) ee_index = N_BAUDRATES-1;
    brIndex = ee_index;

    // initialise the baudrate
    ee_index = EEPROM.read(1);
    if (ee_index >= N_SERMODES) ee_index = 0;
    smIndex = ee_index;

    // initialising the delimiter
    delim = readString(2);
    if (delim.length() == 0)
        delim = "\r";

    Serial1.begin(baudRates[brIndex], serialModes[smIndex]);
    Serial2.begin(baudRates[brIndex], serialModes[smIndex]);
}

void loop() {
    char s;
    uint32_t now = millis();
    digitalWrite(BOARD_LED_PIN, !digitalRead(BOARD_LED_PIN));

    // USB debug port (port1)
    while (ms.ports[2].available() > 0) {
        elapsed_tC = now;
        s=(char)ms.ports[2].read();
        uint8_t tl = rxMsgC.length();
        bool isError = false;
        
        if (s == '\n' || s == '\r') {
            isTyping = false;

            // This is just a comment, just go to the next line when you press enter...
            if (!isConsole && tl > 0) {
                ms.ports[2].write("\r\n");
                rxMsgC = "";
                continue;
            }

            // But if ENTER was pressed on an empty line, we go to console mode...
            if (tl == 0) {
                if (!isConsole) {
                    isConsole = true;
                    ms.ports[2].println(welcomeMsg);
                    ms.ports[2].print("C> ");
                    ms.ports[2].print(baudMsg);
                    ms.ports[2].println(baudRates[brIndex]);
                    ms.ports[2].print("C> ");
                    ms.ports[2].print(smMsg);
                    ms.ports[2].println(serialModeNames[smIndex]);
                    ms.ports[2].print("C> ");
                    ms.ports[2].print(delimMsg);
                    ms.ports[2].print(escapeString(delim));
                } else {
                    isConsole = false;
                    ms.ports[2].write("bye\r\n");
                }
            }
            if (isConsole) ms.ports[2].write("\r\nC> ");

            if (rxMsgC=="H")  { 
                // The help message
                ms.ports[2].println(helpMsg);
            } else if (rxMsgC=="W") {
                // Who does what?
                ms.ports[0].println("USB1");
                ms.ports[1].println("USB2");
                ms.ports[2].println("CONSOLE");
                Serial1.println("Ser1");
                Serial2.println("Ser2");
            } else if (rxMsgC.startsWith(">") && tl > 1) {
                // Here we send to the device
                Serial2.print(rxMsgC.substring(1) + delim);
            } else if (rxMsgC.startsWith("<") && tl > 1) {
                // Here we send to all listening devices (serial and USB)
                rxMsgC = rxMsgC.substring(1) + delim;
                Serial1.print(rxMsgC);
                ms.ports[0].println(rxMsgC);
                ms.ports[1].println(rxMsgC);
            } else if (rxMsgC.startsWith("m")) {
                if (tl > 1) {
                    if (rxMsgC.charAt(1) == '?') {
                        printSerialModes();
                        ms.ports[2].write("C> ");
                    }
                    else {
                        int8_t i = getSmIndex(rxMsgC.substring(1));
                        if (i == -1)
                            isError = true;
                        else if (i != smIndex) {
                            smIndex = i;
                            Serial1.begin(baudRates[brIndex], serialModes[smIndex]);
                            Serial2.begin(baudRates[brIndex], serialModes[smIndex]);
                            EEPROM.update(1, smIndex);
                        }
                    }
                }
                if (!isError) {
                    ms.ports[2].print(smMsg);
                    ms.ports[2].println(serialModeNames[smIndex]);
                }
            } else if (rxMsgC.startsWith("b")) {
                if (tl > 1) {
                    if (rxMsgC.charAt(1) == '?') {
                        printBaudRates();
                        ms.ports[2].write("C> ");
                    }
                    else {
                        uint32_t br = rxMsgC.substring(1).toInt();
                        isError = true;
                        for (uint8_t i=0; i<N_BAUDRATES; i++) {
                            if (br == baudRates[i]) {
                                isError = false;
                                if (i != brIndex) {
                                    Serial1.begin(br, serialModes[smIndex]);
                                    Serial2.begin(br, serialModes[smIndex]);

                                    // Here we write the baudrate index to the EEPROM
                                    brIndex = i;
                                    EEPROM.update(0, brIndex);
                                }
                                break;
                            }
                        }
                    }
                }
                if (!isError) {
                    ms.ports[2].print(baudMsg);
                    ms.ports[2].println(baudRates[brIndex]);
                }
            } else if (rxMsgC.startsWith("d")) {
                if (tl > 1) {
                    // Exotic delimiters (max 10 characters stored in the EEPROM) are handled here. They are appended to commands sent to the device.
                    // However, for commands returned by the device, only the first character is checked.
                    rxMsgC = rxMsgC.substring(1);
                    rxMsgC.replace("\\n","\n");
                    rxMsgC.replace("\\r","\r");

                    uint32_t s_from=0;
                    while (1) {
                        s_from = rxMsgC.indexOf("\\x",s_from);
                        if (s_from == -1)
                            break;

                        char c = (char)(x2i(rxMsgC.substring(s_from+2,s_from+4)));
                        rxMsgC = rxMsgC.substring(0,s_from)+c+rxMsgC.substring(s_from+4);
                        s_from += 4;
                        if (s_from >= rxMsgC.length())
                            break;
                    }

                    if (rxMsgC != delim) {
                        delim = rxMsgC;
                        writeString(2,delim);
                    }
                }

                //Display the escaped delimiter:
                ms.ports[2].print(delimMsg);
                ms.ports[2].println(escapeString(delim));
            } else {
                // An error only if we didn't just press ENTER
                if (tl > 0) isError = true;
            }

            if (isError)
                ms.ports[2].println("Huh?");

            // A command was issued. Clear the input buffer
            if (tl > 0) {
                rxMsgC = "";
                if (isConsole) ms.ports[2].write("C> ");
            }

        } else {  
            // Here we have a chance to display a comment but we don't have a prompt
            if (!isConsole && !isTyping && tl == 0) {
                isTyping = true;
                ms.ports[2].print("C> ");
            }

            bool isDisplay = true;

            // delete is 127, backspace is 8
            if (s == 127 || s == 8) {
                if (tl > 0)
                    rxMsgC = rxMsgC.substring(0, tl-1);
                else
                    isDisplay = false;
            } else {
                // Here we've reached the end of the line, so we make a new one
                if (tl >= MAXLENGTH) {
                    rxMsgC = "";
                    ms.ports[2].print("\r\nC> ");
                    if (isConsole)
                        ms.ports[2].print("Huh?\r\nC> ");
                }
                rxMsgC += s; 
            }
            if (isDisplay) ms.ports[2].write(s);
        }
    }

    // USB controller to device (port0)
    while (ms.ports[0].available() > 0) {
        elapsed_tA = now;
        s=(char)ms.ports[0].read();
        uint8_t tl = rxMsgA.length();

        if (s == '\n' || s == '\r' || s == delim[0]) {
            if (tl == 0)
                continue;

            // Quiet if in console mode
            if (!isConsole) {
                // Here we were typing something so cut it short!
                if (isTyping) {
                    isTyping = false;
                    rxMsgC = "";
                    ms.ports[2].print("\r\n");
                }

                ms.ports[2].write("U> ");
                ms.ports[2].println(escapeString(rxMsgA));

                // These will fake a response from the device
                fakeResponse(rxMsgA);
            }

            // Goes to device...
            Serial2.print(rxMsgA + delim);

            // Clear the input buffer
            rxMsgA = "";
        } else {  
            // Add the character or clear the string if too long
            rxMsgA = (tl < MAXLENGTH) ? rxMsgA+s : "";
        }
    }

    // 2nd USB controller to device (port2)
    while (ms.ports[1].available() > 0) {
        elapsed_tB = now;
        s=(char)ms.ports[1].read();
        uint8_t tl = rxMsgB.length();

        if (s == '\n' || s == '\r' || s == delim[0]) {
            if (tl == 0)
                continue;

            // Quiet if in console mode
            if (!isConsole) {
                // Here we were typing something so cut it short!
                if (isTyping) {
                    isTyping = false;
                    rxMsgC = "";
                    ms.ports[2].print("\r\n");
                }

                ms.ports[2].write("u> ");
                ms.ports[2].println(escapeString(rxMsgB));

                // These will fake a response from the device
                fakeResponse(rxMsgB);
            }

            // Goes to device...
            Serial2.print(rxMsgB + delim);

            // Clear the input buffer
            rxMsgB = "";
        } else {  
            // Add the character or clear the string if too long
            rxMsgB = (tl < MAXLENGTH) ? rxMsgB+s : "";
        }
    }

    // Serial1 is Serial controller to device
    while (Serial1.available() > 0) {
        elapsed_t1 = now;
        s=(char)Serial1.read();
        uint8_t tl = rxMsg1.length();

        if (s == '\n' || s == '\r' || s == delim[0]) {
            if (rxMsg1.length() == 0)
                continue;

            // XXX Replace the following substring in the query
            // rxMsg1.replace("BD_EP","AAAS");

            // Quiet if in console mode
            if (!isConsole) {
                // Here we were typing something so cut it short!
                if (isTyping) {
                    isTyping = false;
                    rxMsgC = "";
                    ms.ports[2].print("\r\n");
                }

                ms.ports[2].write("S> ");
                rxMsgA = conv78(rxMsgA);
                ms.ports[2].println(escapeString(rxMsg1));

                // These will fake a response from the device
                fakeResponse(rxMsg1);
            }

            // Goes to device...
            Serial2.print(rxMsg1 + delim);

            // Clear the input buffer
            rxMsg1 = "";
        } else {  
            // Add the character or clear the string if too long
            rxMsg1 = (tl < MAXLENGTH) ? rxMsg1+s : "";
        }
    }

    // Serial device to both controllers
    while (Serial2.available() > 0  || isFaked) {
        elapsed_t2 = now;

        // For a fake response, we fake the incoming serial2 character. rxMsg2 itself is already faked
        s = (isFaked)?'\n':(char)Serial2.read();

        uint8_t tl = rxMsg2.length();

        if (s == '\n' || s == '\r' || s == delim[0]) {
            if (rxMsg2.length() == 0)
                continue;

            // XXX Replace the following substring in the reply
            // rxMsg2.replace("BD_EP","AAAS");

            // XXX Here we strip the parity bit
            rxMsg2 = conv78(rxMsg2);

            // Here (if we have to) we process a large faked string in delimited chunks 
            uint32_t s_from=0, s_to;
            while (1) {
                s_to = rxMsg2.indexOf(delim,s_from);
                if (s_to == -1)
                    s_to = tl;

                // Goes to USB controller...
                ms.ports[0].print(rxMsg2.substring(s_from,s_to) + delim);

                // Goes to second USB controller...
                ms.ports[1].print(rxMsg2.substring(s_from,s_to) + delim);

                // Goes to serial controller...
                Serial1.print(rxMsg2.substring(s_from,s_to) + delim);

                // Quiet if in console mode
                if (!isConsole) {
                    // Here we were typing something so cut it short!
                    if (isTyping) {
                        isTyping = false;
                        rxMsgC = "";
                        ms.ports[2].print("\r\n");
                    }

                    // Debug info
                    //if (isFaked) ms.ports[2].println("C> Faking:");
                    ms.ports[2].write("D> ");
                    ms.ports[2].println(escapeString(rxMsg2.substring(s_from,s_to)));
                }

                // We got to the end of the string, nothing more to send
                if (s_to >= tl)
                    break;

                s_from = s_to + delim.length();
            }


            // Clear the input buffer
            rxMsg2 = "";
            isFaked = false;
        } else {  
            // Add the character or clear the string if too long
            rxMsg2 = (tl < MAXLENGTH) ? rxMsg2+s : "";
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
    if (now - elapsed_tC >= SERIAL_TIMEOUT) {
        // Here we decide we waited long enough and clear the string.
        // We're telling the user
        if (rxMsgC.length() > 0) {
            if (isConsole)
                ms.ports[2].print("\r\nC> Huh?\r\nC> ");
            else {
                isTyping = false;
                ms.ports[2].print("\r\n");
            }
        }

        rxMsgC = ""; elapsed_tC = now;
    }

    // Wait some
    delay(50);
}
