/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "CANMsg.h"
#include "rover_config.h"
#include "blockingneopixel.h"

constexpr uint16_t    NEOPIXEL_CAN_ID_INCOMING = 0x784;
constexpr uint16_t    NEOPIXEL_CAN_ID_OUTGOING = 0x785;
CAN                   can(CAN1_RX, CAN1_TX, ROVER_CANBUS_FREQUENCY);
CANMsg                rxMsg, txMsg;
BlockingNeopixel      neopixel(16);

// Color is specified by the data inside the packet
// 0 is solid red
// 1 is solid blue
// 2 is flashing green
// 3 is off

void initCAN() {
    can.filter(ROVER_CANID_FIRST_GIMBTONOMY_RX, ROVER_CANID_FILTER_MASK, CANStandard);
}

void handleSetNeoPixelColor(CANMsg *p_newMsg){
    // 0 = solid red, 1 = solid blue, 2 = flashing green, 3 = off
    uint8_t neoPixelMode = 0;
    *p_newMsg >> neoPixelMode;
    switch (neoPixelMode){
    case 0:
        printf("Setting neo pixels to solid red\r\n");
        neopixel.displayRed();
        break;
    case 1:
        printf("Setting neo pixels to solid blue\r\n");
        neopixel.displayBlue();
        break;
    case 2:
        printf("Setting neo pixels to flashing green\r\n");
        neopixel.flashGreen(10,2);
        break;
    case 3:
        printf("Setting neo pixels to flashing green\r\n");
        neopixel.shutdown();
        break;
    default:
        printf("Neo pixels tried to be set to unknow mode\r\n");
        break;
    }
}

void processCANMsg(CANMsg *p_newMsg){
    switch (p_newMsg->id){
        case NEOPIXEL_CAN_ID_INCOMING:
            // Send an acknowledgement CANMsg back to the Jetson
            printf("Sending acknowledgement message\r\n");
            txMsg.clear();
            txMsg.id = NEOPIXEL_CAN_ID_OUTGOING;
            txMsg << true;
            can.write(txMsg);
            printf("Updating neo pixels\r\n");
            handleSetNeoPixelColor(p_newMsg);
            break;
        default:
            printf("Recieved unimplemented command for neopixels\r\n");
            break;
    }
}

// main() runs in its own thread in the OS
int main(){
    printf("Beginning neopixel fw app.\r\n");
    initCAN();
    while(1){
        if (can.read(rxMsg)){
            processCANMsg(&rxMsg);
            rxMsg.clear();
        }
    }
    return 1;

}