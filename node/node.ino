#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h> 
#include "alp_protocol.h"

// Funkcje z drawing logic
void processDrawingData(char *data, uint16_t len, uint8_t seq_num);
void setAngleStep(int16_t deg);
void setTurtleState(float x, float y, float ang);

// Konfiguracja sieciowa
// Dla kazdego wyeksportowanego pliku binarnego zmieniano adres mac oraz port // 01;4321 02;4322 ...
byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x04}; 
unsigned int localPort = 4324; 

ZsutEthernetUDP Udp;
ZsutIPAddress serverIP = ZsutIPAddress(192, 168, 56, 106);
unsigned int serverPort = 12345;

//Poltno
uint16_t my_min_x, my_max_x;
uint16_t my_min_y, my_max_y; 

void sendHello();
void sendDataAck(uint8_t seq);

// Inicjalizacja
void setup() {
  Serial.begin(115200);
  Serial.println("Node starting...");
  
  // Uruchomienie karty sieciowej i nasluchu UDP
  ZsutEthernet.begin(mac); 
  Udp.begin(localPort);
  
  Serial.println(ZsutEthernet.localIP());
  
  sendHello();
}

// Obsluga pakietow
void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint8_t packetBuffer[128];
    Udp.read(packetBuffer, 128);
    alp_header_t *header = (alp_header_t *)packetBuffer;

    // Hello ACK
    if (header->type == ALP_MSG_HELLO_ACK) {
       alp_payload_hello_ack_t *pl = (alp_payload_hello_ack_t *)(packetBuffer + sizeof(alp_header_t));
       
       // Zapisanie granic strefy oraz kat
       my_min_x = pl->min_x;
       my_max_x = pl->max_x;
       my_min_y = pl->min_y;
       my_max_y = pl->max_y;
       setAngleStep(pl->angle_step);
       
       Serial.print("Registered! Bounds X: ");
       Serial.print(my_min_x); Serial.print("-"); Serial.print(my_max_x);
       Serial.print(" Y: ");
       Serial.print(my_min_y); Serial.print("-"); Serial.println(my_max_y);
    }
    // Odbior danych
    else if (header->type == ALP_MSG_DATA) {
       alp_payload_data_t *pl = (alp_payload_data_t *)(packetBuffer + sizeof(alp_header_t));
       
       Serial.print("Data received, len: "); Serial.println(header->length);
       
       // Przekazanie danych do interpretera (drawing logic)
       processDrawingData(pl->data, header->length, header->seq);
    }
    // 3. Odbior handover
    else if (header->type == ALP_MSG_SET_STATE) {
       alp_payload_set_state_t *pl = (alp_payload_set_state_t *)(packetBuffer + sizeof(alp_header_t));
       
       // Ustawienie zolwia
       setTurtleState(pl->x, pl->y, pl->angle);
       
       Serial.println("State updated (Handover received)!"); 
    }
  }
}

void sendHello() {
    uint8_t buf[sizeof(alp_header_t) + sizeof(alp_payload_hello_t)];
    alp_header_t *hdr = (alp_header_t *)buf;
    alp_payload_hello_t *pl = (alp_payload_hello_t *)(buf + sizeof(alp_header_t));
    
    hdr->type = ALP_MSG_HELLO;
    hdr->seq = 1;
    hdr->length = sizeof(alp_payload_hello_t);
    pl->id = 1;
    
    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}

// Potwierdzenie po narysowaniu
void sendDataAck(uint8_t seq_num) {
    uint8_t buf[sizeof(alp_header_t)];
    alp_header_t *hdr = (alp_header_t *)buf;
    
    hdr->type = ALP_MSG_DATA_ACK;
    hdr->seq = seq_num; 
    hdr->length = 0;
    
    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buf, sizeof(buf));
    Udp.endPacket();
}