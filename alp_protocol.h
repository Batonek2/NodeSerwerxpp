#ifndef ALP_PROTOCOL_H
#define ALP_PROTOCOL_H

#include <stdint.h>

// Typy wiadomości
#define ALP_MSG_HELLO       0x01 
#define ALP_MSG_HELLO_ACK   0x02 
#define ALP_MSG_DATA        0x03 
#define ALP_MSG_DATA_ACK    0x04
#define ALP_MSG_PIXEL       0x05 
#define ALP_MSG_HANDOVER    0x06 
#define ALP_MSG_SET_STATE   0x07 

// Struktura naglowka
typedef struct __attribute__((packed)) {
    uint8_t type;       
    uint8_t seq;        
    uint16_t length;    
} alp_header_t;

// Rejestracja
typedef struct __attribute__((packed)) {
    uint8_t id;         
} alp_payload_hello_t;

// Odpowiedz serwera
typedef struct __attribute__((packed)) {
    uint16_t min_x;     
    uint16_t max_x;     
    uint16_t min_y;
    uint16_t max_y;
    int16_t angle_step; 
} alp_payload_hello_ack_t;

// Dane do rysowania
typedef struct __attribute__((packed)) {
    char data[64];      
} alp_payload_data_t;

// Piksel
typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
    char symbol;
} alp_payload_pixel_t;

// Handover
typedef struct __attribute__((packed)) {
    float x;
    float y;
    float angle;
    uint32_t progress;
} alp_payload_handover_t;

// Ustawienie zolwia
typedef struct __attribute__((packed)) {
    float x;
    float y;
    float angle;
} alp_payload_set_state_t;

#endif