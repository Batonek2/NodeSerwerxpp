#include <math.h> 
#include "alp_protocol.h"

#define STACK_SIZE 32 

// Zolw
typedef struct {
    float x;
    float y;
    float angle;
} turtle_state_t;

// Stos do [ oraz ]
turtle_state_t stack[STACK_SIZE];
int stack_ptr = 0;

// Pozycja startowa
float pos_x = 25.0;
float pos_y = 25.0;
float angle = 0.0; 
float angle_step_rad = 0.0;

// Zmienne z pliku node
extern uint16_t my_min_x, my_max_x;
extern uint16_t my_min_y, my_max_y;
extern ZsutEthernetUDP Udp;
extern ZsutIPAddress serverIP;
extern unsigned int serverPort;

void sendHandover(float x, float y, float ang, uint32_t progress);
void sendPixel(uint16_t px, uint16_t py);
void sendDataAck(uint8_t seq);

// Stopnie na radiany
void setAngleStep(int16_t deg) {
    angle_step_rad = (float)deg * 3.14159f / 180.0f;
}

// Zolw po handover
void setTurtleState(float x, float y, float ang) {
    pos_x = x;
    pos_y = y;
    angle = ang;
    stack_ptr = 0; // Reset stosu dla nowego wezla
}

// Wyslanie pakietu z pikselem do serwera
void sendPixel(uint16_t px, uint16_t py) {
    uint8_t buf[sizeof(alp_header_t) + sizeof(alp_payload_pixel_t)];
    alp_header_t *hdr = (alp_header_t *)buf;
    alp_payload_pixel_t *pl = (alp_payload_pixel_t *)(buf + sizeof(alp_header_t));

    hdr->type = ALP_MSG_PIXEL;
    hdr->seq = 0; 
    hdr->length = sizeof(alp_payload_pixel_t);
    pl->x = px;
    pl->y = py;
    pl->symbol = '#'; 

    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}

// Wysłanie handover
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
    pl->progress = progress; // Indeks znaku na ktorym przerwano

    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}

// Interpretacja znakow L-Systemu
void processDrawingData(char *data, uint16_t len, uint8_t seq_num) {
    for(int i=0; i<len; i++) {
        char cmd = data[i];
        
        // Do przodu
        if (cmd == 'F' || cmd == 'f') { 
             int steps = (cmd == 'F') ? 1 : 0; // F rysuje, f tylko przesuwa
             
             // Znak to 2 pixele
             for(int k=0; k<2; k++) { 
                 float next_x = pos_x + cos(angle);
                 float next_y = pos_y + sin(angle);

                 // Pilnowanie granic
                 if (next_x >= (float)my_max_x || next_x < (float)my_min_x || 
                     next_y >= (float)my_max_y || next_y < (float)my_min_y) {
                     
                     sendHandover(next_x, next_y, angle, (uint32_t)i);
                     return; 
                 }
                 
                 // Aktualizacja pozycji
                 pos_x = next_x;
                 pos_y = next_y;
                 if (steps) sendPixel((uint16_t)(pos_x + 0.5f), (uint16_t)(pos_y + 0.5f)); 
             }
        } 
        // Obrot zolwia + - oraz |
        else if (cmd == '+') angle += angle_step_rad;
        else if (cmd == '-') angle -= angle_step_rad;
        else if (cmd == '|') angle += 3.14159f;
        // Zapamietanie pozycji [
        else if (cmd == '[') {
            if (stack_ptr < STACK_SIZE) {
                stack[stack_ptr].x = pos_x;
                stack[stack_ptr].y = pos_y;
                stack[stack_ptr].angle = angle;
                stack_ptr++;
            }
        }
        // Przywrocenie pozycji ]
        else if (cmd == ']') {
            if (stack_ptr > 0) {
                stack_ptr--;
                pos_x = stack[stack_ptr].x;
                pos_y = stack[stack_ptr].y;
                angle = stack[stack_ptr].angle;
            }
        }
    }
    // ACK po przetworzeniu pakietu
    sendDataAck(seq_num);
}