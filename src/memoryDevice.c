#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/log.h"
#include "lib/memoryDevice.h"

#ifdef __cplusplus
extern "C" {
#endif

static void data_read(const hwStream* stream, u_int8_t* data, u_int32_t addr, size_t len, u_int8_t memReadOnly, u_int8_t byteEna){
    int i = 0;
    for(i = 0; i < len; i++){
        int doAccess = 1;
        if(byteEna != 0xFF){
            int bytIdx = (addr + i) & 0x3;
            int bytIdxMsk = 1 << bytIdx;
            if((bytIdxMsk & byteEna) == 0){
                doAccess = 0;
            }
        }
        if(doAccess){
            data[i] = ((u_int8_t*)stream->getBuf((Stream*)stream))[i + addr];
        }else{
            data[i] = 0;
        }
    }
}

static void data_write(const hwStream* stream, u_int8_t* data, u_int32_t addr, size_t len, u_int8_t memReadOnly, u_int8_t byteEna){
    int i = 0;
    for(i = 0; i < len; i++){
        int doAccess = 1;
        if(byteEna != 0xFF){
            int bytIdx = (addr + i) & 0x3;
            int bytIdxMsk = 1 << bytIdx;
            if((bytIdxMsk & byteEna) == 0){
                doAccess = 0;
            }
        }
        if(doAccess){
            if(memReadOnly){
                log_print(LOG_SEV_WRN, "Tried to write to read-only memory at addr 0x%08X", addr + i);
            }else{
                ((u_int8_t*)stream->getBuf((Stream*)stream))[i + addr] = data[i];
            }
        }
    }
}

void memInit(void* mem, void* base, u_int32_t size, u_int8_t readOnly, u_int8_t byteEna){
    ((MemoryDevice*)mem)->data_read = data_read;
    ((MemoryDevice*)mem)->data_write = data_write;
    ((MemoryDevice*)mem)->base = base;
    ((MemoryDevice*)mem)->size = size;
    ((MemoryDevice*)mem)->readOnly = readOnly;
    ((MemoryDevice*)mem)->byteEna = byteEna;
}

#ifdef __cplusplus
}
#endif