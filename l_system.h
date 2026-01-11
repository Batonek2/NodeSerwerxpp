#ifndef L_SYSTEM_H
#define L_SYSTEM_H

#include <stdint.h>

// Struktura przechowująca wynik derywacji
typedef struct {
    char *result_string; // Wynikowy łańcuch (np. "F+F-F...")
    int angle;           // Kąt wczytany z pliku
    long length;         // Długość łańcucha
} l_system_result_t;

// Funkcja wczytująca plik i generująca wynik po N iteracjach
// Zwraca 0 w przypadku sukcesu, -1 błąd
int generate_lsystem(const char *filename, int iterations, l_system_result_t *out_result);

// Funkcja zwalniająca pamięć po wyniku
void free_lsystem_result(l_system_result_t *result);

#endif