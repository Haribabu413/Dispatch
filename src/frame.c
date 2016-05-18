#include "frame.h"
#include <stddef.h>

#define START_OF_FRAME 0xf7
#define END_OF_FRAME 0x7f
#define ESC 0xf6
#define ESC_XOR 0x20

static uint8_t rxFrame[RX_FRAME_LENGTH];
static uint16_t rxFrameIndex = 0;

static uint16_t f16Sum1 = 0, f16Sum2 = 0;

void FRM_pushToChannel(uint8_t data);
uint16_t FRM_fletcher16(uint8_t* data, size_t bytes);

uint16_t (*channelReadableFunctPtr)();
uint16_t (*channelWriteableFunctPtr)();
void (*channelReadFunctPtr)(uint8_t* data, uint16_t length);
void (*channelWriteFunctPtr)(uint8_t* data, uint16_t length);

void FRM_init(void){
    uint8_t dataToSend = START_OF_FRAME;
    channelWriteFunctPtr(&dataToSend, 1);
    
    f16Sum1 = f16Sum2 = 0;
}

void FRM_push(uint8_t data){
    f16Sum1 = (f16Sum1 + (uint16_t)data) & 0xff;
    f16Sum2 = (f16Sum2 + f16Sum1) & 0xff;
    
    FRM_pushToChannel(data);
}

void FRM_finish(void){
    FRM_pushToChannel(f16Sum1);
    FRM_pushToChannel(f16Sum2);
    
    uint8_t dataToSend = END_OF_FRAME;
    channelWriteFunctPtr(&dataToSend, 1);
}

void FRM_pushToChannel(uint8_t data){
    uint8_t byte = data;
    
    /* add proper escape sequences */
    if((byte == START_OF_FRAME) || (byte == END_OF_FRAME) || (byte == ESC)){
        uint8_t escChar = ESC;
        uint8_t escData = byte ^ ESC_XOR;
        channelWriteFunctPtr(&escChar, 1);
        channelWriteFunctPtr(&escData, 1);
    }else{
        channelWriteFunctPtr(&byte, 1);
    }
}

uint16_t FRM_pull(uint8_t* data){
    uint16_t sofIndex = 0, eofIndex = 0;
    uint16_t length = 0;
    
    uint16_t i = 0;
    
    /* read the available bytes into the rxFrame */
    uint16_t numOfBytes = channelReadableFunctPtr();
    channelReadFunctPtr(&rxFrame[rxFrameIndex], numOfBytes);
    rxFrameIndex += numOfBytes;
    
    /* find the START_OF_FRAME */
    while(i < RX_FRAME_LENGTH){
        if(rxFrame[i] == START_OF_FRAME){
            sofIndex = i;
            break;
        }else{
            rxFrame[i] = 0; // clear the byte that isn't the START_OF_FRAME
        }
        i++;
    }
    
    /* find the END_OF_FRAME */
    i = sofIndex;
    while(i < RX_FRAME_LENGTH){
        if(rxFrame[i] == END_OF_FRAME){
            eofIndex = i;
            break;
        }
        i++;
    }
    
    /* ensure that the start of frame and the end of frame are both present */
    if(((rxFrame[0] == START_OF_FRAME) || (sofIndex > 0)) && (eofIndex != 0)){
        /* extract the received frame and shift the remainder of the
         * bytes into the beginning of the frame */
        i = 0;
        int16_t frameIndex = sofIndex;
        rxFrame[frameIndex] = 0;
        frameIndex++;
        while(frameIndex < eofIndex){
            if(rxFrame[frameIndex] == ESC){
                frameIndex++;
                data[i] = rxFrame[frameIndex] ^ ESC_XOR;
            }else{
                data[i] = rxFrame[frameIndex];
            }
            i++;
            frameIndex++;
        }
        
        length = i;
        
        /* a full frame was just processed, find the next START_OF_FRAME and
         * copy the remainder of the frame forward in preparation for
         * the next frame reception */
        i = eofIndex;
        sofIndex = 0;
        while(i < RX_FRAME_LENGTH){
            if(rxFrame[i] == START_OF_FRAME){
                sofIndex = i;
                break;
            }else{
                rxFrame[i] = 0;
            }
            i++;
        }
        rxFrameIndex = 0;
        
        /* copy and clear */
        if(sofIndex > 0){
            i = 0;
            while((i + sofIndex) < RX_FRAME_LENGTH){
                rxFrame[i] = rxFrame[(i + sofIndex)];
                rxFrame[(i + sofIndex)] = 0;
                i++;
            }
        }
        
        /* check the data integrity using the last two bytes as
         * the fletcher16 checksum */
        uint16_t checksum = data[length - 2] | (data[length - 1] << 8);
        length -= 2;
        uint16_t check = FRM_fletcher16(data, length);
        if(check != checksum){
            length = 0;
        }
    }
    
    return length;
}

uint16_t FRM_fletcher16(uint8_t* data, size_t length){
	uint16_t sum1 = 0;
	uint16_t sum2 = 0;
    
    uint16_t i = 0;
    while(i < length){
        sum1 = (sum1 + (uint16_t)data[i]) & 0xff;
        sum2 = (sum2 + sum1) & 0xff;
        i++;
    }
    
    uint16_t checksum = (sum2 << 8) | sum1;
    
	return checksum;
}

void FRM_assignChannelReadable(uint16_t (*functPtr)()){
    channelReadableFunctPtr = functPtr;
}

void FRM_assignChannelWriteable(uint16_t (*functPtr)()){
    channelWriteableFunctPtr = functPtr;
}

void FRM_assignChannelRead(void (*functPtr)(uint8_t* data, uint16_t length)){
    channelReadFunctPtr = functPtr;
}

void FRM_assignChannelWrite(void (*functPtr)(uint8_t* data, uint16_t length)){
    channelWriteFunctPtr = functPtr;
}