#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l_system.h"

#define MAX_LINE 1024
#define MAX_RULES 256

// Tablica przechowujaca reguly
char *rules[MAX_RULES]; 

// Funkcja zwalniania regul
void clear_rules() {
    for (int i = 0; i < MAX_RULES; i++) {
        if (rules[i]) {
            free(rules[i]);
            rules[i] = NULL;
        }
    }
}

// Funkcja wykonująca jedna iteracje derywacji
char* derive_once(const char* input) {
    long new_len = 0;
    long input_len = strlen(input);

    // Obliczamy dlugosc nowego lancucha
    for (long i = 0; i < input_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (rules[c] != NULL) {
            new_len += strlen(rules[c]);
        } else {
            new_len += 1;
        }
    }

    // Alokacja pamięci
    char *output = (char*)malloc(new_len + 1);
    if (!output) return NULL;

    // Budowanie lancucha
    long pos = 0;
    for (long i = 0; i < input_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (rules[c] != NULL) {
            strcpy(output + pos, rules[c]);
            pos += strlen(rules[c]);
        } else {
            output[pos] = (char)c;
            pos += 1;
        }
    }
    output[pos] = '\0';
    return output;
}

int generate_lsystem(const char *filename, int iterations, l_system_result_t *out_result) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening file");
        return -1;
    }

    clear_rules();

    char buffer[MAX_LINE];
    char axiom[MAX_LINE];
    int angle = 90; 

    // Wczytanie kata
    if (fgets(buffer, MAX_LINE, fp)) {
        angle = atoi(buffer);
    }
    
    // Wczytanie aksjomatu
    if (fgets(buffer, MAX_LINE, fp)) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        strcpy(axiom, buffer);
    }

    // Wczytanie wszystkich regul
    while (fgets(buffer, MAX_LINE, fp)) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strlen(buffer) < 2) continue;

        // ZNAK=CIĄG
        char *eq = strchr(buffer, '=');
        if (eq) {
            unsigned char key = (unsigned char)buffer[0]; // Lewa strona
            rules[key] = strdup(eq + 1); // Prawa strona
            printf("Loaded rule: %c -> %s\n", key, rules[key]);
        }
    }
    fclose(fp);

    printf("L-System Config: Angle=%d, Axiom=%s\n", angle, axiom);

    // Derywacja
    char *current_str = strdup(axiom);
    
    for (int i = 0; i < iterations; i++) {
        char *next_str = derive_once(current_str);
        free(current_str); // Zwolnij stary
        if (!next_str) {
            clear_rules();
            return -1;
        }
        current_str = next_str;
        printf("Iteration %d: length %ld\n", i+1, strlen(current_str));
    }

    out_result->result_string = current_str;
    out_result->length = strlen(current_str);
    out_result->angle = angle;
    
    clear_rules();

    return 0;
}

void free_lsystem_result(l_system_result_t *result) {
    if (result->result_string) {
        free(result->result_string);
        result->result_string = NULL;
    }
}