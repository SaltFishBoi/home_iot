/*
  RADIO.h - Library for Radio transceiver function.
  Created by Kevin Z. Yu, March 23, 2021.
  Released into the public domain.
*/

//Last edit 5/17/2021
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "RADIO.h"

/*
RADIO::RADIO(int pin)
{
  pinMode(pin, OUTPUT);
  _pin = pin;
}
*/

// initialize transceiver
void RADIO::initialize_trans(void) {
    if (ELECHOUSE_cc1101.getCC1101()) {      // Check the CC1101 Spi connection.
        Serial.println("Connection OK");
    }
    else {
        Serial.println("Connection Error");
    }

    ELECHOUSE_cc1101.Init();                // must be set to initialize the cc1101!
    ELECHOUSE_cc1101.setCCMode(1);          // set config for internal transmission mode.
    ELECHOUSE_cc1101.setModulation(0);      // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
    ELECHOUSE_cc1101.setMHZ(433.92);        // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
    ELECHOUSE_cc1101.setDeviation(47.60);   // Set the Frequency deviation in kHz. Value from 1.58 to 380.85. Default is 47.60 kHz.
    ELECHOUSE_cc1101.setChannel(0);         // Set the Channelnumber from 0 to 255. Default is cahnnel 0.
    ELECHOUSE_cc1101.setChsp(199.95);       // The channel spacing is multiplied by the channel number CHAN and added to the base frequency in kHz. Value from 25.39 to 405.45. Default is 199.95 kHz.
    ELECHOUSE_cc1101.setRxBW(812.50);       // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
    ELECHOUSE_cc1101.setDRate(199.94);      // Set the Data Rate in kBaud. Value from 0.02 to 1621.83. Default is 99.97 kBaud!
    ELECHOUSE_cc1101.setPA(10);             // Set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
    ELECHOUSE_cc1101.setSyncMode(2);        // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
    ELECHOUSE_cc1101.setSyncWord(211, 145); // Set sync word. Must be the same for the transmitter and receiver. (Syncword high, Syncword low)
    ELECHOUSE_cc1101.setAdrChk(0);          // Controls address check configuration of received packages. 0 = No address check. 1 = Address check, no broadcast. 2 = Address check and 0 (0x00) broadcast. 3 = Address check and 0 (0x00) and 255 (0xFF) broadcast.
    ELECHOUSE_cc1101.setAddr(0);            // Address used for packet filtration. Optional broadcast addresses are 0 (0x00) and 255 (0xFF).
    ELECHOUSE_cc1101.setWhiteData(0);       // Turn data whitening on / off. 0 = Whitening off. 1 = Whitening on.
    ELECHOUSE_cc1101.setPktFormat(0);       // Format of RX and TX data. 0 = Normal mode, use FIFOs for RX and TX. 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.
    ELECHOUSE_cc1101.setLengthConfig(1);    // 0 = Fixed packet length mode. 1 = Variable packet length mode. 2 = Infinite packet length mode. 3 = Reserved
    ELECHOUSE_cc1101.setPacketLength(0);    // Indicates the packet length when fixed packet length mode is enabled. If variable packet length mode is used, this value indicates the maximum packet length allowed.
    ELECHOUSE_cc1101.setCrc(1);             // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
    ELECHOUSE_cc1101.setCRC_AF(0);          // Enable automatic flush of RX FIFO when CRC is not OK. This requires that only one packet is in the RXIFIFO and that packet length is limited to the RX FIFO size.
    ELECHOUSE_cc1101.setDcFilterOff(0);     // Disable digital DC blocking filter before demodulator. Only for data rates ≤ 250 kBaud The recommended IF frequency changes when the DC blocking is disabled. 1 = Disable (current optimized). 0 = Enable (better sensitivity).
    ELECHOUSE_cc1101.setManchester(0);      // Enables Manchester encoding/decoding. 0 = Disable. 1 = Enable.
    ELECHOUSE_cc1101.setFEC(0);             // Enable Forward Error Correction (FEC) with interleaving for packet payload (Only supported for fixed packet length mode. 0 = Disable. 1 = Enable.
    ELECHOUSE_cc1101.setPQT(0);             // Preamble quality estimator threshold. The preamble quality estimator increases an internal counter by one each time a bit is received that is different from the previous bit, and decreases the counter by 8 each time a bit is received that is the same as the last bit. A threshold of 4∙PQT for this counter is used to gate sync word detection. When PQT=0 a sync word is always accepted.
    ELECHOUSE_cc1101.setAppendStatus(0);    // When enabled, two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK.

    Serial.println("XX Mode");
}

void RADIO::receiveMessage(int maxDuration, byte *message, byte type, byte id) {
    byte buffer[2] = {0};
    unsigned long startTime = millis();   // the time the delay started using current time

    while ((millis() - startTime) < maxDuration) {
        if (type == BS && ELECHOUSE_cc1101.CheckRxFifo(rx_time) && ELECHOUSE_cc1101.CheckCRC()) {
            ELECHOUSE_cc1101.ReceiveData(buffer);
            decode(buffer, message);
            if (message[3] == id && (message[0] == bsRespond || message[0] == bsRequest)) {
                break;
            }
        }
        else if (type == CPE && ELECHOUSE_cc1101.CheckRxFifo(rx_time) && ELECHOUSE_cc1101.CheckCRC()) {
            ELECHOUSE_cc1101.ReceiveData(buffer);
            decode(buffer, message);
            if (message[2] == id && message[0] == cpeRespond) {
                break;
            }
        }
        else if (type == LBU && ELECHOUSE_cc1101.CheckRxFifo(rx_time) && ELECHOUSE_cc1101.CheckCRC()) {
            ELECHOUSE_cc1101.ReceiveData(buffer);
            decode(buffer, message);
            if (message[0] == lbuInterrupt) {
                break;
            }
        }
        else if (ELECHOUSE_cc1101.CheckRxFifo(rx_time) && ELECHOUSE_cc1101.CheckCRC()) {
            //Serial.println("hello");
            //CRC Check. If "setCrc(false)" crc returns always OK


            //Rssi Level in dBm
            //Serial.print("Rssi: ");
            //Serial.println(ELECHOUSE_cc1101.getRssi());

            //Link Quality Indicator
            //Serial.print("LQI: ");
            //Serial.println(ELECHOUSE_cc1101.getLqi());

            //Get received Data and calculate length
            // int len = ELECHOUSE_cc1101.ReceiveData(buffer);
            // buffer[len] = '\0';
            ELECHOUSE_cc1101.ReceiveData(buffer);
            decode(buffer, message);
            break;
        }
    }
}

void RADIO::sendMessage(int duration, byte *message) {
    byte buffer[2] = {0};
    unsigned long startTime = millis();   // the time the delay started using current time

    encode(message, buffer);

    while ((millis() - startTime) < duration) {
        ELECHOUSE_cc1101.SendData(buffer, 2, tx_time);
    }
}

// ---------------------------------------------
// |  opCode  | payload  |   src    |   des    |
// ---------------------------------------------
// |7  6  5  4|3  2  1  0|7  6  5  4|3  2  1  0|
// ---------------------------------------------
// |        byte1        |        byte2        |
// ---------------------------------------------

void RADIO::decode(byte *buffer, byte *message) {
    message[0] = buffer[0] >> 4;
    message[1] = buffer[0] & 0x0f; 
    message[2] = buffer[1] >> 4;
    message[3] = buffer[1] & 0x0f;
}

void RADIO::encode(byte *message, byte *buffer) {
    buffer[0] = message[0] << 4;
    buffer[0] = buffer[0] + message[1];
    buffer[1] = message[2] << 4;
    buffer[1] = buffer[1] + message[3];
}

void RADIO::switchChannel(byte ch) {
    ELECHOUSE_cc1101.setChannel(ch*2);
}