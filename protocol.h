#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef struct{
    int worker_id;
    int chunk_id;
    uint32_t data_size;
    unsigned char payload[1500];
}ImagePacket;

#endif