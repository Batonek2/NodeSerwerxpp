#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <math.h> 
#include <time.h> // Do mierzenia czasu
#include "alp_protocol.h"
#include "l_system.h" 

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define CANVAS_W 100
#define CANVAS_H 100

typedef struct {
    struct sockaddr_in addr;
    int active;
    uint16_t min_x, max_x;
    uint16_t min_y, max_y;
} node_t;

node_t nodes[4]; 
int registered_count = 0;

char canvas[CANVAS_H][CANVAS_W];
l_system_result_t current_lsystem;

long global_idx = 0;      
long last_chunk_start = 0; 

// --- ZMIENNE RETRANSMISJI ---
int waiting_for_ack = 0;           // Flaga: czy wysłaliśmy dane i czekamy?
time_t sent_time = 0;              // Kiedy wysłaliśmy
uint8_t last_sent_packet[256];     // Kopia ostatniej paczki
int last_sent_len = 0;             // Długość ostatniej paczki
struct sockaddr_in last_dest_addr; // Do kogo wysłaliśmy

void init_canvas() {
    for(int i=0; i<CANVAS_H; i++) for(int j=0; j<CANVAS_W; j++) canvas[i][j] = '.'; 
}

void print_canvas() {
    printf("\033[H\033[J"); 
    printf("--- SERVER CANVAS (100x100) [4 NODES + RETRANSMISSION] ---\n");
    for(int i=0; i<CANVAS_H; i++) {
        for(int j=0; j<CANVAS_W; j++) {
            putchar(canvas[i][j]);
        }
        putchar('\n');
    }
}

// Funkcja pomocnicza do wysyłania z zapamiętywaniem (do retransmisji)
void send_reliable(int sockfd, uint8_t *buf, int len, struct sockaddr_in dest) {
    // 1. Wyślij normalnie
    sendto(sockfd, buf, len, 0, (const struct sockaddr *)&dest, sizeof(dest));
    
    // 2. Zapamiętaj co wysłałeś (tylko jeśli to DATA lub SET_STATE)
    alp_header_t *hdr = (alp_header_t *)buf;
    if (hdr->type == ALP_MSG_DATA || hdr->type == ALP_MSG_SET_STATE) {
        memcpy(last_sent_packet, buf, len);
        last_sent_len = len;
        last_dest_addr = dest;
        sent_time = time(NULL); // Zapisz aktualny czas
        waiting_for_ack = 1;    // Ustaw flagę
    } else {
        waiting_for_ack = 0; // Inne pakiety (np. ACK) nie wymagają potwierdzeń
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uzycie: %s <plik_lsystemu> <liczba_iteracji>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int iterations = atoi(argv[2]);

    printf("Ladowanie L-Systemu: %s (Iteracje: %d)...\n", filename, iterations);

    if (generate_lsystem(filename, iterations, &current_lsystem) != 0) {
        return 1;
    }
    
    printf("Wygenerowano %ld znakow. Czekam na 4 wezly...\n", current_lsystem.length);
    init_canvas();

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buffer[BUFFER_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) exit(EXIT_FAILURE);
    
    // --- TIMEOUT NA GNIEŹDZIE (0.1 sekundy) ---
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    // ------------------------------------------

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) exit(EXIT_FAILURE);

    while (1) {
        // RETRANSMISJA: Sprawdzamy czy minął czas
        if (waiting_for_ack) {
            time_t now = time(NULL);
            if (now - sent_time >= 2) { // Jeśli minęły 2 sekundy
                printf("!!! TIMEOUT DETECTED !!! Retransmitting packet...\n");
                sendto(sockfd, last_sent_packet, last_sent_len, 0, (const struct sockaddr *)&last_dest_addr, sizeof(last_dest_addr));
                sent_time = now; // Reset licznika
            }
        }

        socklen_t len = sizeof(cliaddr);
        // recvfrom teraz zwróci -1 co 100ms, jeśli nikt nic nie przysłał
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        
        if (n < 0) continue; // Timeout (normalne, lecimy dalej w pętli sprawdzać retransmisję)
        if (n < (int)sizeof(alp_header_t)) continue;

        alp_header_t *header = (alp_header_t *)buffer;

        switch (header->type) {
            case ALP_MSG_HELLO: {
                if (registered_count < 4) {
                    int id = registered_count;
                    nodes[id].addr = cliaddr;
                    nodes[id].active = 1;

                    if (id == 0) { nodes[id].min_x = 0; nodes[id].max_x = 50; nodes[id].min_y = 0; nodes[id].max_y = 50; }
                    else if (id == 1) { nodes[id].min_x = 50; nodes[id].max_x = 100; nodes[id].min_y = 0; nodes[id].max_y = 50; }
                    else if (id == 2) { nodes[id].min_x = 0; nodes[id].max_x = 50; nodes[id].min_y = 50; nodes[id].max_y = 100; }
                    else if (id == 3) { nodes[id].min_x = 50; nodes[id].max_x = 100; nodes[id].min_y = 50; nodes[id].max_y = 100; }

                    printf("Node %d registered.\n", id);

                    uint8_t resp_buf[sizeof(alp_header_t) + sizeof(alp_payload_hello_ack_t)];
                    alp_header_t *resp_hdr = (alp_header_t *)resp_buf;
                    alp_payload_hello_ack_t *resp_pl = (alp_payload_hello_ack_t *)(resp_buf + sizeof(alp_header_t));

                    resp_hdr->type = ALP_MSG_HELLO_ACK;
                    resp_hdr->seq = header->seq;
                    resp_hdr->length = sizeof(alp_payload_hello_ack_t);
                    resp_pl->min_x = nodes[id].min_x;
                    resp_pl->max_x = nodes[id].max_x;
                    resp_pl->min_y = nodes[id].min_y;
                    resp_pl->max_y = nodes[id].max_y;
                    resp_pl->angle_step = (int16_t)current_lsystem.angle;
                    
                    sendto(sockfd, resp_buf, sizeof(resp_buf), 0, (const struct sockaddr *)&cliaddr, len);
                    registered_count++;

                    if (registered_count == 4) {
                        printf("ALL 4 NODES READY! Starting in 2s...\n");
                        sleep(2); 
                        
                        int start_node = 0; 
                        
                        uint8_t data_buf[128];
                        alp_header_t *d_hdr = (alp_header_t *)data_buf;
                        alp_payload_data_t *d_pl = (alp_payload_data_t *)(data_buf + sizeof(alp_header_t));
                        
                        d_hdr->type = ALP_MSG_DATA;
                        d_hdr->seq = 10;
                        
                        global_idx = 0;
                        last_chunk_start = 0;
                        long copy_len = (current_lsystem.length > 60) ? 60 : current_lsystem.length;
                        strncpy(d_pl->data, current_lsystem.result_string, copy_len); 
                        d_hdr->length = copy_len;
                        global_idx += copy_len;
                        
                        // UŻYWAMY NOWEJ FUNKCJI: send_reliable
                        send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, nodes[start_node].addr);
                        printf("STARTED! Initial task sent to Node %d.\n", start_node);
                    }
                }
                break;
            }

            case ALP_MSG_DATA_ACK: {
                waiting_for_ack = 0; // Dostaliśmy ACK, więc przestajemy się martwić
                
                if (global_idx < current_lsystem.length) {
                    uint8_t data_buf[128];
                    alp_header_t *d_hdr = (alp_header_t *)data_buf;
                    alp_payload_data_t *d_pl = (alp_payload_data_t *)(data_buf + sizeof(alp_header_t));
                    
                    d_hdr->type = ALP_MSG_DATA;
                    d_hdr->seq = header->seq + 1;
                    
                    long remaining = current_lsystem.length - global_idx;
                    long len_to_send = (remaining > 60) ? 60 : remaining;
                    last_chunk_start = global_idx;

                    strncpy(d_pl->data, current_lsystem.result_string + global_idx, len_to_send); 
                    d_hdr->length = len_to_send;
                    global_idx += len_to_send;
                    
                    send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, cliaddr);
                } else {
                    printf("ACK -> Drawing finished!\n");
                }
                break;
            }

            case ALP_MSG_HANDOVER: {
                // Nie potwierdzamy handovera ACK-iem, bo zaraz wyslemy SET_STATE, 
                // ale resetujemy timeout dla poprzedniego DATA
                waiting_for_ack = 0; 

                alp_payload_handover_t *pl = (alp_payload_handover_t *)(buffer + sizeof(alp_header_t));
                
                long actual_resume_idx = last_chunk_start + pl->progress;
                global_idx = actual_resume_idx; 

                float new_x = pl->x; 
                float new_y = pl->y;

                if (new_x >= 99.0) new_x = 1.0; 
                else if (new_x <= 1.0) new_x = 99.0;
                if (new_y >= 99.0) new_y = 1.0;
                else if (new_y <= 1.0) new_y = 99.0;

                int col = (new_x >= 50.0) ? 1 : 0;
                int row = (new_y >= 50.0) ? 1 : 0;
                int target_node = row * 2 + col; 

                printf("HANDOVER (%.1f, %.1f) -> Node %d\n", pl->x, pl->y, target_node);

                uint8_t state_buf[sizeof(alp_header_t) + sizeof(alp_payload_set_state_t)];
                alp_header_t *s_hdr = (alp_header_t *)state_buf;
                alp_payload_set_state_t *s_pl = (alp_payload_set_state_t *)(state_buf + sizeof(alp_header_t));
                
                s_hdr->type = ALP_MSG_SET_STATE;
                s_hdr->seq = 20;
                s_hdr->length = sizeof(alp_payload_set_state_t);
                s_pl->x = new_x; 
                s_pl->y = new_y; 
                s_pl->angle = pl->angle;
                
                // Tutaj też reliable!
                send_reliable(sockfd, state_buf, sizeof(state_buf), nodes[target_node].addr);
                
                usleep(20000); 

                if (global_idx >= current_lsystem.length) break;

                uint8_t data_buf[128];
                alp_header_t *d_hdr = (alp_header_t *)data_buf;
                alp_payload_data_t *d_pl = (alp_payload_data_t *)(data_buf + sizeof(alp_header_t));
                d_hdr->type = ALP_MSG_DATA;
                d_hdr->seq = 21;
                
                long remaining = current_lsystem.length - global_idx;
                long len_to_send = (remaining > 60) ? 60 : remaining;
                last_chunk_start = global_idx;

                strncpy(d_pl->data, current_lsystem.result_string + global_idx, len_to_send); 
                d_hdr->length = len_to_send;
                global_idx += len_to_send;
                
                // I dane też reliable
                send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, nodes[target_node].addr);
                break;
            }

            case ALP_MSG_PIXEL: {
                alp_payload_pixel_t *pix = (alp_payload_pixel_t *)(buffer + sizeof(alp_header_t));
                if (pix->x < CANVAS_W && pix->y < CANVAS_H) {
                    canvas[pix->y][pix->x] = pix->symbol;
                    print_canvas();
                }
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}