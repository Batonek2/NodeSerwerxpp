#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h> 
#include "alp_protocol.h"
#include "l_system.h" 

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define CANVAS_W 100
#define CANVAS_H 100

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Struktura pojedynczego wezla
typedef struct {
    struct sockaddr_in addr; // Adres i port
    uint16_t min_x, max_x;   // Zakresy wezla
    uint16_t min_y, max_y;   
} node_t;

node_t nodes[4]; // Tablica 4 wezlow
int registered_count = 0;

// Plotno
char canvas[CANVAS_H][CANVAS_W];

// wynik generowania L-Systemu
l_system_result_t current_lsystem;

// Stan wysylania
long global_idx = 0;       // Indeks wyslanych znakow
long last_chunk_start = 0; // Indeks poczatku ostatniej wyslanej paczki (pod handover)

// Zmienne do wyswietlania plotna 
int pixel_counter = 0;      // Licznik pikseli
#define REFRESH_RATE 50
int snapshot_id = 0;

// Zmienne retransmisja
int waiting_for_ack = 0;           // 1 = czekamy, 0 = gotowy
time_t sent_time = 0;              // Czas wysłania ostatniego ważnego pakietu
uint8_t last_sent_packet[256];     // Kopia ostatniego pakietu
int last_sent_len = 0;             // Dlugosc kopii
struct sockaddr_in last_dest_addr; // Adresat kopii

// Statusy pod plotnami
char last_status_msg[256] = "Waiting for nodes...";
char last_status_color[16] = ANSI_COLOR_BLUE;

// Plotno
void init_canvas() {
    for(int i=0; i<CANVAS_H; i++) for(int j=0; j<CANVAS_W; j++) canvas[i][j] = '.'; 
}

// Ustawienia statusu
void set_status(const char* msg, const char* color) {
    strncpy(last_status_msg, msg, 255);
    strncpy(last_status_color, color, 15);
}

// Glowna funkcja rysujaca serwer
void print_canvas() {
    snapshot_id++;
    
    printf("\n"); 
    printf("==============================================================\n");
    printf(" #%d | Data Sent: %ld/%ld | Nodes: %d \n", snapshot_id, global_idx, current_lsystem.length, registered_count);
    printf(" STATUS: %s%s%s\n", last_status_color, last_status_msg, ANSI_COLOR_RESET);
    printf("==============================================================\n");

    for(int i=0; i<CANVAS_H; i++) {
        for(int j=0; j<CANVAS_W; j++) {
            putchar(canvas[i][j]);
        }
        putchar('\n');
    }
    printf("==============================================================\n");
    
    fflush(stdout);
}

// Funkcja wysyłająca
void send_reliable(int sockfd, uint8_t *buf, int len, struct sockaddr_in dest) {
    sendto(sockfd, buf, len, 0, (const struct sockaddr *)&dest, sizeof(dest));
    
    alp_header_t *hdr = (alp_header_t *)buf;
    // Tylko pakiety niosące dane lub stan są krytyczne
    if (hdr->type == ALP_MSG_DATA || hdr->type == ALP_MSG_SET_STATE) {
        memcpy(last_sent_packet, buf, len); // Kopia zapasowa
        last_sent_len = len;
        last_dest_addr = dest;
        sent_time = time(NULL);
        waiting_for_ack = 1;
    } else {
        waiting_for_ack = 0;
    }
}

// Szukanie indeksu wezla po IP i porcie
int find_node_index(struct sockaddr_in cliaddr) {
    for (int i = 0; i < registered_count; i++) {
        if (nodes[i].addr.sin_addr.s_addr == cliaddr.sin_addr.s_addr &&
            nodes[i].addr.sin_port == cliaddr.sin_port) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    // Sprawdzenie argumentow wywołania
    if (argc < 3) {
        printf("Uzycie: %s <plik_lsystemu> <liczba_iteracji>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int iterations = atoi(argv[2]);

    printf(ANSI_COLOR_BLUE "Ladowanie L-Systemu: %s (Iteracje: %d)...\n" ANSI_COLOR_RESET, filename, iterations);

    if (generate_lsystem(filename, iterations, &current_lsystem) != 0) {
        return 1;
    }
    
    // Wyswietlenie wyniku
    printf(ANSI_COLOR_GREEN "Wygenerowano %ld znakow. Wynik (Ostatnia iteracja):\n" ANSI_COLOR_RESET, current_lsystem.length);
    printf("--------------------------------------------------\n");
    for (long i = 0; i < current_lsystem.length; i++) {
        putchar(current_lsystem.result_string[i]);
        if ((i + 1) % 100 == 0) putchar('\n');
    }
    printf("\n--------------------------------------------------\n");
    printf("Nacisnij ENTER, aby uruchomic serwer...");
    getchar();

    init_canvas();
    
    set_status("System Started. Waiting for nodes...", ANSI_COLOR_BLUE);
    printf("Serwer wystartowal. Czekam na 4 wezly...\n");

    // Inicjalizacja gniazda UDP
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buffer[BUFFER_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) exit(EXIT_FAILURE);
    
    // Ustawienie timeoutu, zapobiega blokowaniu programu
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Nasluch na wszystkich interfejsach
    servaddr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) exit(EXIT_FAILURE);

    while (1) {
        // Sprawdzenie timeoutu retransmisji
        if (waiting_for_ack) {
            time_t now = time(NULL);
            if (now - sent_time >= 6) {
                set_status("TIMEOUT! Retransmitting packet...", ANSI_COLOR_RED);
                print_canvas();
                // Retransmisja
                sendto(sockfd, last_sent_packet, last_sent_len, 0, (const struct sockaddr *)&last_dest_addr, sizeof(last_dest_addr));
                sent_time = now;
            }
        }

        // Odbior pakietu
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        
        if (n < 0) continue; // Timeout
        if (n < (int)sizeof(alp_header_t)) continue;

        alp_header_t *header = (alp_header_t *)buffer;

        // Maszyna stanow ALP
        switch (header->type) {
            
            // Rejestracja wezla
            case ALP_MSG_HELLO: {
                int existing_idx = find_node_index(cliaddr);
                int current_node_id;

				// Weryfikacja wezla
                if (existing_idx != -1) {
                    current_node_id = existing_idx;
                } else if (registered_count < 4) {
                    current_node_id = registered_count;
                    nodes[current_node_id].addr = cliaddr;
                    
                    // Przypisanie stref
                    if (current_node_id == 0) { nodes[0].min_x=0; nodes[0].max_x=50; nodes[0].min_y=0; nodes[0].max_y=50; }
                    else if (current_node_id == 1) { nodes[1].min_x=50; nodes[1].max_x=100; nodes[1].min_y=0; nodes[1].max_y=50; }
                    else if (current_node_id == 2) { nodes[2].min_x=0; nodes[2].max_x=50; nodes[2].min_y=50; nodes[2].max_y=100; }
                    else if (current_node_id == 3) { nodes[3].min_x=50; nodes[3].max_x=100; nodes[3].min_y=50; nodes[3].max_y=100; }

                    char msg[64];
                    snprintf(msg, 64, "Node %d registered.", current_node_id);
                    set_status(msg, ANSI_COLOR_GREEN);
                    
                    // Logowanie rejestracji
                    printf("Node %d registered (%d/4)\n", current_node_id, registered_count + 1);
                    
                    registered_count++;
                } else {
                    continue;
                }

                // Hello_ACK
                uint8_t resp_buf[sizeof(alp_header_t) + sizeof(alp_payload_hello_ack_t)];
                alp_header_t *resp_hdr = (alp_header_t *)resp_buf;
                alp_payload_hello_ack_t *resp_pl = (alp_payload_hello_ack_t *)(resp_buf + sizeof(alp_header_t));

                resp_hdr->type = ALP_MSG_HELLO_ACK;
                resp_hdr->seq = header->seq;
                resp_hdr->length = sizeof(alp_payload_hello_ack_t);
                // Przekazanie granic do wezla
                resp_pl->min_x = nodes[current_node_id].min_x;
                resp_pl->max_x = nodes[current_node_id].max_x;
                resp_pl->min_y = nodes[current_node_id].min_y;
                resp_pl->max_y = nodes[current_node_id].max_y;
                resp_pl->angle_step = (int16_t)current_lsystem.angle;
                
                sendto(sockfd, resp_buf, sizeof(resp_buf), 0, (const struct sockaddr *)&cliaddr, len);

                // Uruchomienie
                if (registered_count == 4 && existing_idx == -1) { 
                    set_status("ALL 4 NODES READY! Starting...", ANSI_COLOR_GREEN);
                    print_canvas();
                    sleep(2); 
                    
                    int start_node = 0; 
                    uint8_t data_buf[128];
                    alp_header_t *d_hdr = (alp_header_t *)data_buf;
                    alp_payload_data_t *d_pl = (alp_payload_data_t *)(data_buf + sizeof(alp_header_t));
                    
                    d_hdr->type = ALP_MSG_DATA;
                    d_hdr->seq = 10;
                    
                    global_idx = 0;
                    last_chunk_start = 0;
                    // Przygotowanie pierwszej paczki
                    long copy_len = (current_lsystem.length > 60) ? 60 : current_lsystem.length;
                    strncpy(d_pl->data, current_lsystem.result_string, copy_len); 
                    d_hdr->length = copy_len;
                    global_idx += copy_len;
                    
                    // Wysłanie
                    send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, nodes[start_node].addr);
                }
                break;
            }

            // Potwierdzenie odbioru
            case ALP_MSG_DATA_ACK: {
                waiting_for_ack = 0;
                
                // Sprawdzenie czy jest jeszcze do wyslania
                if (global_idx < current_lsystem.length) {
                    uint8_t data_buf[128];
                    alp_header_t *d_hdr = (alp_header_t *)data_buf;
                    alp_payload_data_t *d_pl = (alp_payload_data_t *)(data_buf + sizeof(alp_header_t));
                    
                    d_hdr->type = ALP_MSG_DATA;
                    d_hdr->seq = header->seq + 1;
                    
                    long remaining = current_lsystem.length - global_idx;
                    long len_to_send = (remaining > 60) ? 60 : remaining;
                    last_chunk_start = global_idx; // Poczatek paczki (pod handover)

                    strncpy(d_pl->data, current_lsystem.result_string + global_idx, len_to_send); 
                    d_hdr->length = len_to_send;
                    global_idx += len_to_send;
                    
                    send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, cliaddr);
                } else {
                    set_status("DATA SENT. Waiting for drawing...", ANSI_COLOR_GREEN);
					print_canvas();
                }
                break;
            }

            // Handover
            case ALP_MSG_HANDOVER: {
                waiting_for_ack = 0; 
                alp_payload_handover_t *pl = (alp_payload_handover_t *)(buffer + sizeof(alp_header_t));
                
                // Gdzie stanal zolw
                long actual_resume_idx = last_chunk_start + pl->progress;
                global_idx = actual_resume_idx; 

                float new_x = pl->x; 
                float new_y = pl->y;

                // Zawijanie plotna
                if (new_x >= 100.0) new_x -= 100.0;
                else if (new_x < 0.0) new_x += 100.0;
                if (new_y >= 100.0) new_y -= 100.0;
                else if (new_y < 0.0) new_y += 100.0;

                // Nowy wezel
                int col = (new_x >= 50.0) ? 1 : 0;
                int row = (new_y >= 50.0) ? 1 : 0;
                int target_node = row * 2 + col; 

                // Unikanie bledow na granicach
                if (col == 1 && new_x < 50.0) new_x = 50.1;
                if (col == 0 && new_x >= 50.0) new_x = 49.9;
                if (row == 1 && new_y < 50.0) new_y = 50.1;
                if (row == 0 && new_y >= 50.0) new_y = 49.9;

                // Log handover
                char msg[128];
                snprintf(msg, 128, "*** HANDOVER (%.1f, %.1f) -> Node %d ***", pl->x, pl->y, target_node);
                
                set_status(msg, ANSI_COLOR_YELLOW);
                print_canvas();

                //Stan zolwia do nowego wezla
                uint8_t state_buf[sizeof(alp_header_t) + sizeof(alp_payload_set_state_t)];
                alp_header_t *s_hdr = (alp_header_t *)state_buf;
                alp_payload_set_state_t *s_pl = (alp_payload_set_state_t *)(state_buf + sizeof(alp_header_t));
                
                s_hdr->type = ALP_MSG_SET_STATE;
                s_hdr->seq = 20;
                s_hdr->length = sizeof(alp_payload_set_state_t);
                s_pl->x = new_x; 
                s_pl->y = new_y; 
                s_pl->angle = pl->angle;
                
                send_reliable(sockfd, state_buf, sizeof(state_buf), nodes[target_node].addr);
                usleep(20000);

                // Dane do nowego wezla
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
                
                send_reliable(sockfd, data_buf, sizeof(alp_header_t) + d_hdr->length, nodes[target_node].addr);
                break;
            }

            // Piksel
            case ALP_MSG_PIXEL: {
                alp_payload_pixel_t *pix = (alp_payload_pixel_t *)(buffer + sizeof(alp_header_t));
                if (pix->x < CANVAS_W && pix->y < CANVAS_H) {
                    canvas[pix->y][pix->x] = pix->symbol;
                    
                    // Log plotna
                    pixel_counter++;
                    if (pixel_counter >= REFRESH_RATE) {
                        set_status("Drawing...", ANSI_COLOR_BLUE);
                        print_canvas();
                        pixel_counter = 0;
                    }
                }
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}