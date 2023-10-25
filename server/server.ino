//File Name: Base Station
//Project Name: Cogntiive Radio Network Enviroment Deployment
//Engineer: Kevin Yu
//Date: 3-22-2021
//Description: 
//New transmitting and receiving method applied. (method checks the Rx Fifo for any data it contains)
//It allows you to do several things in a loop.
//In addition, the gdo0 and gdo2 pin are not required.
//https://github.com/LSatan/SmartRC-CC1101-Driver-Lib
//by Little_S@tan

//Last edit 5/17/2021
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RADIO.h>
#include <TEST.h>

#define LED1 3
#define LED2 4

// channel status
#define FREE 0
#define OP 0xFF

// client status
#define RFS 0
#define WFC 1
#define DONE 0xFF

// TIME
unsigned long startTime = 0;              // starting reference time

// STATE
int state = 0;                  // 0 = receive request, 1 respond, 2 debug
int operation = 0;              // 0 = experiment, 1 = NONE

// SELECTION TABLE
// limit frequency to 128 (int 2 bytes), so max endInDay is "64 day"
// example: { {0xFF00, 0x0301, 0x0202 ...},       => at 0 time phase, weight of CH0 = -1, CH1 = 3, CH2 = 2
//            {0xFF00, 0x0A01, 0x0B02 ...},       => at 1 time phase, weight of CH0 = -1, CH1 = 10, CH2 = 11
//            ...}
int selectionTable[scheduleSize][maxChannel];

// CHANNEL LIST
// channel zero reserved
// status 0 = FREE, status 0xFF Occupied by Priority user
// record channel occupation
// example: {  0  , 0x12, 0xFF, 0x34, ...}  => CH1 has cpe 1 and cpe 2, CH2 has lbu occupation
//          | CH0 | CH1 | CH2 | CH3 ...
byte channelList[maxChannel] = { FREE };

// CLIENT LIST
// client zero reserved
// status 0 = Requesting For Session, status 1 = Waiting For Connection, status 0xFF = DONE
// record client channel status
// example: {    0    ,    1    ,    2    ,    3     ,...}
//          | reserve | client1 | client2 | client3   ...
byte clientList[numClient] = { 0, WFC, WFC, WFC, WFC, WFC};

// LICENSED LIST
// example: {     0     ,     1     ,     2     ,     3      ,...}
//          | licenced1 | licenced2 | licenced3 | licenced4   ...
byte licensedList[numLicensed] = { 0 };

// -----------------------------------------------------
// |   opCode   |  payload   |     src    |     des    |
// -----------------------------------------------------
// | message[0] | message[1] | message[2] | message[3] |
// -----------------------------------------------------
byte outMessage[4] = {0};           // placeholder for message to be transmit 
byte inMessage[4] = {0};            // placeholder for receiving message


// useful simple get and set function for all lists
byte getClientStatus(byte clientID) {
    return clientList[clientID];
}

byte getChannelOccupation(byte ch) {
    return channelList[ch];
}

void occupationUpdate(byte client1, byte client2, byte ch) {
    channelList[ch] = (client1 << 4) + client2;
    clientList[client1] = ch;
    clientList[client2] = ch;
}

// release when detect cpe user not in the channel for following reasons
// 1. license band user interrupt
// 2. session expired
// 3. session not successfully setup
void releaseUpdate(byte client1, byte client2, byte ch, byte st) {
    channelList[ch] = st;
    clientList[client1] = RFS;
    clientList[client2] = RFS;
}

// Sync functions
void connectClients() {
    bool connectionFlag = false;
    Radio.switchChannel(1);         // got to CH1 stand by channel before cpe start

    while (!connectionFlag) {
        // setup single connection
        Radio.receiveMessage(shortReceiveMaxDuration, inMessage, BS, 0);  // (... 0) dummy parameter
        if (inMessage[0] == cpeStart) {
            inMessage[0] = 0; // clear msg
            outMessage[0] = bsAcknowledge;
            outMessage[1] = 0;
            outMessage[2] = inMessage[2];
            outMessage[3] = inMessage[2];
            Radio.sendMessage(shortSendDuration, outMessage);
            releaseUpdate(inMessage[2], inMessage[2], 0, 0); // (... 0, 0) dummy parameters
            Serial.println("Connect a client");
        }

        // check if all connection success
        connectionFlag = true;
        for (byte i = 1; i < numClient; i++) {
            if (clientList[i] == WFC) {
                connectionFlag = false;
                break;
            }
        }
    }
    Serial.println("Connect all clients");
}

void synLicensedUsers() {
    bool synFlag = false;
    Radio.switchChannel(2);             // go to CH2 stand by channel before lbu start

    while (!synFlag) {
        // setup single connection
        Radio.receiveMessage(shortReceiveMaxDuration, inMessage, BS, 0);  // (... 0) dummy parameter
        if (inMessage[0] == lbuStart) {
            inMessage[0] = 0; // clear msg
            outMessage[0] = bsAcknowledge;
            outMessage[1] = 0;
            outMessage[2] = inMessage[2];
            outMessage[3] = inMessage[2];
            Radio.sendMessage(shortSendDuration, outMessage);
            licensedList[inMessage[2]] = 1;
        }

        // check if all connection success
        synFlag = true;
        for (byte i = 0; i < numLicensed; i++) {
            if (licensedList[i] == 0) {
                synFlag = false;
                break;
            }
        }
    }
    Serial.println("Syn with all licensed users");
    delay(3000);                // count down 3 seconds before yell out start
    outMessage[0] = bsStart;
    outMessage[1] = 0;
    outMessage[2] = 0;
    outMessage[3] = 0;
    Radio.sendMessage(longSendDuration, outMessage);   // broad cast to start all time
    // longSendDuration can ensure delay in base Station between all other device. good for senseSpectrum
    Radio.switchChannel(0);
}


void senseSpectrum(byte t) {
    byte clients = 0;
    byte client1 = 0;
    byte client2 = 0;
    for (byte ch = 1; ch < maxChannel; ch++) {
        Radio.switchChannel(ch);
        inMessage[0] = 0;
        // long receive ensure to receive the thing
        Radio.receiveMessage(shortReceiveMaxDuration, inMessage, ANY, 0);
        if (inMessage[0] == 0) { // channel FREE
            clients = getChannelOccupation(ch);
            releaseUpdate((clients >> 4), (clients & 0x0F), ch, FREE); // this is a channel release
        }
        else if (inMessage[0] == lbuInterrupt) { // channel Occupied by Priority user
            clients = getChannelOccupation(ch);
            releaseUpdate((clients >> 4), (clients & 0x0F), ch, OP);  // this is an channel update
            selectionTable[t][ch] += 2;                               // update selection table, increment frequency
        }
        else if(inMessage[0] == cpeSend) {
            selectionTable[t][ch] += 1;                               // update selection table,
        }
    }
    Radio.switchChannel(0);                                           // switch back to reserve channel
    Serial.println("Sense all channels");
}

void initialize_table() {
    for (byte t = 0; t < scheduleSize; t++) {
        for (byte ch = 0; ch < maxChannel; ch++) {
            if (ch == 0) {
                selectionTable[t][ch] = -1;                           // purposely make it the lowest
            }
            else {
                selectionTable[t][ch] = 0;
            }
        }
    }

    // for debug
    //selectionTable[3][1] = 9;
    //selectionTable[3][2] = 7;
    //selectionTable[3][3] = 8;
    //selectionTable[3][4] = 9;
    //selectionTable[3][5] = 9;
    //selectionTable[3][6] = 9;
    //selectionTable[3][7] = 9;
    //selectionTable[3][8] = 9;
    //selectionTable[3][9] = 7;
    //selectionTable[3][10] = 9;
    //selectionTable[3][11] = 9;
}

//https://arduino.stackexchange.com/questions/38177/how-to-sort-elements-of-array-in-arduino-code
int sort_desc(const void* cmp1, const void* cmp2)
{
    // Need to cast the void * to int *
    int a = *((int*)cmp1);
    int b = *((int*)cmp2);
    // The comparison
    return a > b ? -1 : (a < b ? 1 : 0);
    // A simpler, probably faster way:
    //return b - a;
}

// channel selection algorithm 
byte selectChannel(byte option, int t, byte ct) {
    byte ch = 1;
    
    if (option == 0) { // pick start from ch 1 to max channel
        while (ch < maxChannel) {
            if (getChannelOccupation(ch) == 0) {
                break;
            }
            ch++;
        }
    }
    else if (option == 1) { // randomly pick range from ch 1 to max channel
        while (ch < maxChannel) {
            ch = random(1, maxChannel);
            if (getChannelOccupation(ch) == 0) {
                break;
            }
        }
    }
    else if (option == 2) { // pick base on weight
        // we have t as the next period cycle time dev
        //Serial.println("select");
        //Serial.println(t);
        int sortList[maxChannel-1];
        if (ct == 1) {            // look ahead next time period
            sortList[0] = ((selectionTable[t][1])<<8) + 1;
            sortList[1] = ((selectionTable[t][2])<<8) + 2;
            sortList[2] = ((selectionTable[t][3])<<8) + 3;
            sortList[3] = ((selectionTable[t][4])<<8) + 4;
            sortList[4] = ((selectionTable[t][5])<<8) + 5;
            sortList[5] = ((selectionTable[t][6])<<8) + 6;
            sortList[6] = ((selectionTable[t][7])<<8) + 7;
            sortList[7] = ((selectionTable[t][8])<<8) + 8;
            sortList[8] = ((selectionTable[t][9])<<8) + 9;
            sortList[9] = ((selectionTable[t][10])<<8) + 10;
            sortList[10] = ((selectionTable[t][11])<<8) + 11;
        }
        else if(ct == 2) {            // look ahead next 2 time periods
            sortList[0] = ((selectionTable[t][1])<<8) + ((selectionTable[(t+1)%scheduleSize][1])<<8) + 1;
            sortList[1] = ((selectionTable[t][2])<<8) + ((selectionTable[(t+1)%scheduleSize][2])<<8) + 2;
            sortList[2] = ((selectionTable[t][3])<<8) + ((selectionTable[(t+1)%scheduleSize][3])<<8) + 3;
            sortList[3] = ((selectionTable[t][4])<<8) + ((selectionTable[(t+1)%scheduleSize][4])<<8) + 4;
            sortList[4] = ((selectionTable[t][5])<<8) + ((selectionTable[(t+1)%scheduleSize][5])<<8) + 5;
            sortList[5] = ((selectionTable[t][6])<<8) + ((selectionTable[(t+1)%scheduleSize][6])<<8) + 6;
            sortList[6] = ((selectionTable[t][7])<<8) + ((selectionTable[(t+1)%scheduleSize][7])<<8) + 7;
            sortList[7] = ((selectionTable[t][8])<<8) + ((selectionTable[(t+1)%scheduleSize][8])<<8) + 8;
            sortList[8] = ((selectionTable[t][9])<<8) + ((selectionTable[(t+1)%scheduleSize][9])<<8) + 9;
            sortList[9] = ((selectionTable[t][10])<<8) + ((selectionTable[(t+1)%scheduleSize][10])<<8) + 10;
            sortList[10] = ((selectionTable[t][11])<<8) + ((selectionTable[(t+1)%scheduleSize][11])<<8) + 11;
        }
        else if(ct == 3) {            // look ahead next 3 time periods
            sortList[0] = ((selectionTable[t][1])<<8) + ((selectionTable[(t+1)%scheduleSize][1])<<8) + ((selectionTable[(t+2)%scheduleSize][1])<<8) + 1;
            sortList[1] = ((selectionTable[t][2])<<8) + ((selectionTable[(t+1)%scheduleSize][2])<<8) + ((selectionTable[(t+2)%scheduleSize][2])<<8) + 2;
            sortList[2] = ((selectionTable[t][3])<<8) + ((selectionTable[(t+1)%scheduleSize][3])<<8) + ((selectionTable[(t+2)%scheduleSize][3])<<8) + 3;
            sortList[3] = ((selectionTable[t][4])<<8) + ((selectionTable[(t+1)%scheduleSize][4])<<8) + ((selectionTable[(t+2)%scheduleSize][4])<<8) + 4;
            sortList[4] = ((selectionTable[t][5])<<8) + ((selectionTable[(t+1)%scheduleSize][5])<<8) + ((selectionTable[(t+2)%scheduleSize][5])<<8) + 5;
            sortList[5] = ((selectionTable[t][6])<<8) + ((selectionTable[(t+1)%scheduleSize][6])<<8) + ((selectionTable[(t+2)%scheduleSize][6])<<8) + 6;
            sortList[6] = ((selectionTable[t][7])<<8) + ((selectionTable[(t+1)%scheduleSize][7])<<8) + ((selectionTable[(t+2)%scheduleSize][7])<<8) + 7;
            sortList[7] = ((selectionTable[t][8])<<8) + ((selectionTable[(t+1)%scheduleSize][8])<<8) + ((selectionTable[(t+2)%scheduleSize][8])<<8) + 8;
            sortList[8] = ((selectionTable[t][9])<<8) + ((selectionTable[(t+1)%scheduleSize][9])<<8) + ((selectionTable[(t+2)%scheduleSize][9])<<8) + 9;
            sortList[9] = ((selectionTable[t][10])<<8) + ((selectionTable[(t+1)%scheduleSize][10])<<8) + ((selectionTable[(t+2)%scheduleSize][10])<<8) + 10;
            sortList[10] = ((selectionTable[t][11])<<8) + ((selectionTable[(t+1)%scheduleSize][11])<<8) + ((selectionTable[(t+2)%scheduleSize][11])<<8) + 11;
        }
        //for (byte i = 0; i < maxChannel-1; i++) {
        //    Serial.print(sortList[i]);
        //    Serial.print(" ");
        //}
        //Serial.println();
        //exit(0);

        qsort(sortList, maxChannel-1, sizeof(sortList[0]), sort_desc);
        
        for (byte i = maxChannel-2; i >= 0; i++) {
            ch = (sortList[i] & 0xFF);
            if (getChannelOccupation(ch) == 0) { // check if the channel currently free
                break;
            }
        }
    }
    else {
        // more advance algorithm
    }

    return ch;                                  // return channel at the end
}



// 5/1/21 *update* remove done flag in base station
// let it run forever
void bs_process() {
    //bool doneFlag = false;
    byte r;
    //byte ch;
    int t = 0; // program run time in time div
    Radio.switchChannel(0);
    startTime = millis();

    while (true) {
      
        // sense the spectrum every loop
        if ((millis() - startTime) / (1000*secDiv) == t) {
            delay(100); // make sure lbu change first then scan
            senseSpectrum(t%scheduleSize);
            Serial.println("sense Spectrum");
            t++;
        }

        if (state == 0) { // try to receive request
            inMessage[0] = 0; // clear msg
            Radio.receiveMessage(shortReceiveMaxDuration, inMessage, ANY, 0);
            if (inMessage[0] == cpeRequest) {
                //receive request, forward request
                outMessage[0] = bsRequest;
                outMessage[1] = selectChannel(alg, t%scheduleSize, inMessage[1]);;    // choose a channel for them        
                outMessage[2] = inMessage[2];
                outMessage[3] = inMessage[3];
                Radio.sendMessage(shortSendDuration, outMessage);
                Serial.println("send Request");
                state = 1;
            }
        }
        else if (state == 1) { // try to receive response
            inMessage[0] = 0; // clear msg
            Radio.receiveMessage(longReceiveMaxDuration, inMessage, BS, outMessage[3]);
            if (inMessage[0] == cpeRespond) {
                //receive response, forward response back
                outMessage[0] = bsRespond;
                outMessage[1] = inMessage[1]; // assign channel
                outMessage[2] = inMessage[2];
                outMessage[3] = inMessage[3];
                Radio.sendMessage(longSendDuration, outMessage);
                occupationUpdate(inMessage[2], inMessage[3], inMessage[1]);
                Serial.println("send Respond");
                digitalWrite(LED1, HIGH); // blue lighton indicate send a respond (connection)
            }
            state = 0;
        }
        else if (state == 2) // for debugging purposes
        {
            //Serial.print("0  1  2  3  4  5  6  7  8  9  10 11\n");
            //for(byte i = 0; i < maxChannel; i++)
            //{
            //    Serial.print(getChannelOccupation(i));
            //    Serial.print("  ");
            //}
            delay(500);            
            //Serial.println();
            for(byte i = 0; i < numClient; i++)
            {
                Serial.print(getClientStatus(i));
                Serial.print("  ");
            }
            //Serial.println();
            //ch = selectChannel(1, t);
            //Serial.println(ch);
            //Serial.print(" 0  1  2  3  4  5  6  7  8  9  10 11\n");
            //for (byte sche = 0; sche < scheduleSize; sche++) {
            //    for (byte ch = 0; ch < maxChannel; ch++) {
            //        Serial.print(selectionTable[sche][ch]);
            //        Serial.print("  ");
            //    }
            //    Serial.println();
            //}
            //
            //ch = selectChannel(alg, t%scheduleSize, 1);
            Serial.print("                                                  ");
            //Serial.print(ch);
            Serial.println();
        }
    }
}

void setup()
{
    Serial.begin(9600);
    pinMode(LED1,OUTPUT); // initialize leds
    pinMode(LED2,OUTPUT);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    //  initialize tx rx
    Radio.initialize_trans();
}

void loop() {
    if (operation == 0) {
        digitalWrite(LED1, HIGH); // blue lighton indicate on operation 0
        initialize_table();
        connectClients();
        synLicensedUsers();
        digitalWrite(LED1, LOW); // blue lightout indicate finish all connection and yell out start
        bs_process();

        // for this setup, below code never reach
        operation = 1;
    }
    else {
        ;
    }
}
