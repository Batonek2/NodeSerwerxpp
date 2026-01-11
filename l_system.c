#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "l_system.h"

#define MAX_LINE 1024
#define MAX_RULES 256 // Tablica ASCII, żeby mieć szybki dostęp rules['F']

// Tablica przechowująca reguły: rules['F'] = "F+F-F"
char *rules[MAX_RULES]; 

// Funkcja pomocnicza: zwalnianie reguł
void clear_rules() {
    for (int i = 0; i < MAX_RULES; i++) {
        if (rules[i]) {
            free(rules[i]);
            rules[i] = NULL;
        }
    }
}

// Funkcja wykonująca jedną iterację derywacji
char* derive_once(const char* input) {
    long new_len = 0;
    long input_len = strlen(input);

    // KROK 1: Obliczamy długość nowego łańcucha
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

    // KROK 2: Budowanie łańcucha
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

    // Czyścimy stare reguły (na wszelki wypadek)
    clear_rules();

    char buffer[MAX_LINE];
    char axiom[MAX_LINE];
    int angle = 90; // Domyślnie

    // 1. Wczytanie kąta (pierwsza linia)
    if (fgets(buffer, MAX_LINE, fp)) {
        angle = atoi(buffer);
    }
    
    // 2. Wczytanie aksjomatu (druga linia)
    if (fgets(buffer, MAX_LINE, fp)) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        strcpy(axiom, buffer);
    }

    // 3. Wczytanie wszystkich reguł (kolejne linie)
    while (fgets(buffer, MAX_LINE, fp)) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strlen(buffer) < 2) continue; // Puste linie

        // Format: ZNAK=CIĄG (np. F=F+F lub X=X+YF)
        char *eq = strchr(buffer, '=');
        if (eq) {
            unsigned char key = (unsigned char)buffer[0]; // Lewa strona (np. 'F')
            // Prawa strona to wszystko po '='
            rules[key] = strdup(eq + 1);
            printf("Loaded rule: %c -> %s\n", key, rules[key]);
        }
    }
    fclose(fp);

    printf("L-System Config: Angle=%d, Axiom=%s\n", angle, axiom);

    // Derywacja (pętla)
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
    
    // Czyścimy reguły, bo już nie są potrzebne (mamy gotowy string)
    clear_rules();

    return 0;
}

void free_lsystem_result(l_system_result_t *result) {
    if (result->result_string) {
        free(result->result_string);
        result->result_string = NULL;
    }
}