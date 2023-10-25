/*
  RADIO.h - Library for Radio transceiver function.
  Created by Kevin Z. Yu, March 23, 2021.
  Released into the public domain.
*/

//Last edit 5/17/2021
#ifndef RADIO_h
#define RADIO_h

#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// 0 reserved
#define cpeRequest 1
#define cpeRespond 2
#define cpeSend 3
#define cpeAcknowledge 4
#define bsRequest 5
#define bsRespond 6
#define cpeStart 7
#define cpeDone 9
#define cpeClose 10
#define lbuStart 11
#define bsStart 12
#define bsAcknowledge 13
#define lbuInterrupt 8


// FOR TRANSMIT
#define tx_time 3                // tx time in milli seconds
#define shortSendDuration 30
#define longSendDuration 200

// FOR RECEIVE
#define timeExpired 3
#define rx_time 5                // rx time in milli seconds
#define shortReceiveMaxDuration 50
#define longReceiveMaxDuration 500
#define cpeSenseDuration 50
#define CPE 1
#define LBU 2
#define BS 0
#define ANY 3

// WAIT TIME, PRIME NUMBER x10
const int waitTime[5] = {200,300,500,700,1100};

class RADIO
{
public:
    //RADIO(int pin);
    void initialize_trans(void);
    void receiveMessage(int maxDuration, byte *message, byte type, byte id);
    void sendMessage(int duration, byte *message);
    void switchChannel(byte ch);
private:
    int _pin;
    void decode(byte *buffer, byte *message);
    void encode(byte *message, byte *buffer);
};

extern RADIO Radio;

#endif