#ifndef L_SYSTEM_H
#define L_SYSTEM_H

#include <stdint.h>

// Struktura przechowujaca wynik derywacji
typedef struct {
    char *result_string; // Wynikowy łancuch
    int angle;           // Kat wczytany z pliku
    long length;         // Dlugosc lancucha
} l_system_result_t;

// Wczytywanie pliku i generowanie wyniku po N iteracjach
int generate_lsystem(const char *filename, int iterations, l_system_result_t *out_result);

// Zwolnienie pamieci po wyniku
void free_lsystem_result(l_system_result_t *result);

#endif