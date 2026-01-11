#include <math.h> 
#include "alp_protocol.h"

// --- KONFIGURACJA STOSU ---
#define STACK_SIZE 32 // Pamięć na 32 rozgałęzienia (wystarczy na duże krzaki)

typedef struct {
    float x;
    float y;
    float angle;
} turtle_state_t;

turtle_state_t stack[STACK_SIZE];
int stack_ptr = 0; // Wskaźnik stosu (0 = pusty)

// --- ZMIENNE GLOBALNE ---
float pos_x = 25.0; // Start w 1. ćwiartce (dla 4 nodów)
float pos_y = 25.0;
float angle = 0.0; 
float angle_step_rad = 0.0; // Będzie ustawione przez HELLO_ACK

extern uint16_t my_min_x, my_max_x;
extern ZsutEthernetUDP Udp;
extern ZsutIPAddress serverIP;
extern unsigned int serverPort;

void sendHandover(float x, float y, float ang, uint32_t progress);
void sendPixel(uint16_t px, uint16_t py);
void sendDataAck(uint8_t seq); 

// --- IMPLEMENTACJE ---

void setAngleStep(int16_t deg) {
    angle_step_rad = (float)deg * 3.14159f / 180.0f;
}

void setTurtleState(float x, float y, float ang) {
    pos_x = x;
    pos_y = y;
    angle = ang;
    // Reset stosu przy przejęciu (bezpiecznik)
    // W idealnym świecie stos też powinien być przesyłany, ale to hardcore level.
    // Przy roślinach rzadko zdarza się handover w środku głębokiej rekurencji na granicy.
    stack_ptr = 0; 
}

void sendPixel(uint16_t px, uint16_t py) {
    uint8_t buf[sizeof(alp_header_t) + sizeof(alp_payload_pixel_t)];
    alp_header_t *hdr = (alp_header_t *)buf;
    alp_payload_pixel_t *pl = (alp_payload_pixel_t *)(buf + sizeof(alp_header_t));

    hdr->type = ALP_MSG_PIXEL;
    hdr->seq = 0; 
    hdr->length = sizeof(alp_payload_pixel_t);
    pl->x = px;
    pl->y = py;
    pl->symbol = '#'; // Możesz zmienić na '@' dla lepszego efektu liści

    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}

void sendHandover(float x, float y, float ang, uint32_t progress) {
    uint8_t buf[sizeof(alp_header_t) + sizeof(alp_payload_handover_t)];
    alp_header_t *hdr = (alp_header_t *)buf;
    alp_payload_handover_t *pl = (alp_payload_handover_t *)(buf + sizeof(alp_header_t));
    
    hdr->type = ALP_MSG_HANDOVER;
    hdr->seq = 0; 
    hdr->length = sizeof(alp_payload_handover_t);
    pl->x = x;
    pl->y = y;
    pl->angle = ang;
    pl->progress = progress;

    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}

// --- GŁÓWNA LOGIKA RYSOWANIA ---
void processDrawingData(char *data, uint16_t len, uint8_t seq_num) {
    for(int i=0; i<len; i++) {
        char cmd = data[i];
        
        if (cmd == 'F') { // Rysuj naprzód
             for(int k=0; k<2; k++) { // Krok = 2 piksele
                 float next_x = pos_x + cos(angle);
                 float next_y = pos_y + sin(angle);

                 if (next_x >= my_max_x || next_x < my_min_x || next_y >= 100.0 || next_y < 0.0) {
                     sendHandover(next_x, next_y, angle, (uint32_t)i); 
                     return; 
                 }
                 pos_x = next_x;
                 pos_y = next_y;
                 sendPixel((uint16_t)(pos_x + 0.5f), (uint16_t)(pos_y + 0.5f)); 
             }
        } 
        else if (cmd == 'f') { // Idź bez rysowania
             for(int k=0; k<2; k++) {
                 float next_x = pos_x + cos(angle);
                 float next_y = pos_y + sin(angle);

                 if (next_x >= my_max_x || next_x < my_min_x || next_y >= 100.0 || next_y < 0.0) {
                     sendHandover(next_x, next_y, angle, (uint32_t)i); 
                     return; 
                 }
                 pos_x = next_x;
                 pos_y = next_y;
             }
        }
        else if (cmd == '+') { // Obrót w prawo
            angle += angle_step_rad;
        }
        else if (cmd == '-') { // Obrót w lewo
            angle -= angle_step_rad;
        }
        else if (cmd == '|') { // Obrót o 180 (NOWE)
            angle += 3.14159f; 
        }
        else if (cmd == '[') { // STOS: Zapisz stan (NOWE)
            if (stack_ptr < STACK_SIZE) {
                stack[stack_ptr].x = pos_x;
                stack[stack_ptr].y = pos_y;
                stack[stack_ptr].angle = angle;
                stack_ptr++;
            }
        }
        else if (cmd == ']') { // STOS: Przywróć stan (NOWE)
            if (stack_ptr > 0) {
                stack_ptr--;
                pos_x = stack[stack_ptr].x;
                pos_y = stack[stack_ptr].y;
                angle = stack[stack_ptr].angle;
            }
        }
        // Inne znaki (X, Y, A, B) są ignorowane - węzeł nic nie robi, przechodzi dalej.
    }

    sendDataAck(seq_num);
}