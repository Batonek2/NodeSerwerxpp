#ifndef ALP_PROTOCOL_H
#define ALP_PROTOCOL_H

#include <stdint.h>

// Typy wiadomości
#define ALP_MSG_HELLO       0x01 
#define ALP_MSG_HELLO_ACK   0x02 
#define ALP_MSG_DATA        0x03 
#define ALP_MSG_DATA_ACK    0x04 // NOWE: Potwierdzenie odbioru danych
#define ALP_MSG_PIXEL       0x05 
#define ALP_MSG_HANDOVER    0x06 
#define ALP_MSG_SET_STATE   0x07 

// Struktura nagłówka (wspólna)
typedef struct __attribute__((packed)) {
    uint8_t type;       
    uint8_t seq;        
    uint16_t length;    
} alp_header_t;

// Payload: Rejestracja
typedef struct __attribute__((packed)) {
    uint8_t id;         
} alp_payload_hello_t;

// Payload: Odpowiedź serwera
typedef struct __attribute__((packed)) {
    uint16_t min_x;     
    uint16_t max_x;     
    uint16_t min_y;
    uint16_t max_y;
    int16_t angle_step; 
} alp_payload_hello_ack_t;

// Payload: Dane do rysowania
typedef struct __attribute__((packed)) {
    char data[64];      
} alp_payload_data_t;

// Payload: Piksel
typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
    char symbol;
} alp_payload_pixel_t;

// Payload: Handover (Zgłoszenie wyjścia)
typedef struct __attribute__((packed)) {
    float x;
    float y;
    float angle;
    uint32_t progress; // Ile znaków przetworzono przed uderzeniem w ścianę
} alp_payload_handover_t;

// Payload: Set State (Ustawienie żółwia)
typedef struct __attribute__((packed)) {
    float x;
    float y;
    float angle;
} alp_payload_set_state_t;

#endif