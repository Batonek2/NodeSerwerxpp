#include <ZsutEthernet.h>
#include <ZsutEthernetUdp.h>
#include <ZsutFeatures.h> 
#include "alp_protocol.h"

// Deklaracje funkcji z drawing_logic.ino
void processDrawingData(char *data, uint16_t len, uint8_t seq_num); // Zmiana: seq_num
void setAngleStep(int16_t deg);
void setTurtleState(float x, float y, float ang);

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01}; 
unsigned int localPort = 4321;
ZsutEthernetUDP Udp;

ZsutIPAddress serverIP = ZsutIPAddress(192, 168, 56, 106);
unsigned int serverPort = 12345;

bool is_registered = false;
uint16_t my_min_x, my_max_x;

void sendHello();
void sendAck(uint8_t seq);
void sendDataAck(uint8_t seq); // Nowa funkcja

void setup() {
  Serial.begin(115200);
  Serial.println("Node starting...");
  ZsutEthernet.begin(mac); 
  Udp.begin(localPort);
  Serial.println(ZsutEthernet.localIP());
  sendHello();
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint8_t packetBuffer[128];
    Udp.read(packetBuffer, 128);
    
    alp_header_t *header = (alp_header_t *)packetBuffer;

    if (header->type == ALP_MSG_HELLO_ACK) {
       alp_payload_hello_ack_t *pl = (alp_payload_hello_ack_t *)(packetBuffer + sizeof(alp_header_t));
       my_min_x = pl->min_x;
       my_max_x = pl->max_x;
       setAngleStep(pl->angle_step);
       is_registered = true;
       Serial.print("Registered! Range X: ");
       Serial.print(my_min_x); Serial.print("-"); Serial.println(my_max_x);
    }
    else if (header->type == ALP_MSG_DATA) {
       alp_payload_data_t *pl = (alp_payload_data_t *)(packetBuffer + sizeof(alp_header_t));
       Serial.print("Data received, len: "); Serial.println(header->length);
       // Przekazujemy numer sekwencji do logiki rysowania
       processDrawingData(pl->data, header->length, header->seq); 
    }
    else if (header->type == ALP_MSG_SET_STATE) {
       alp_payload_set_state_t *pl = (alp_payload_set_state_t *)(packetBuffer + sizeof(alp_header_t));
       setTurtleState(pl->x, pl->y, pl->angle);
       Serial.println("State updated (Handover received)!");
       // Opcjonalnie można wysłać zwykły ACK, ale tutaj czekamy na dane
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

// Funkcja wysyłająca żądanie o więcej danych
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