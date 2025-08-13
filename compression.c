#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Estructura para representar una entrada en el diccionario LZW
typedef struct {
    unsigned short prefix;   // Código del prefijo (subcadena previa)
    unsigned char character; // Carácter añadido al prefijo
    int used;                 // Marca si esta entrada está en uso
} DictEntry;

// ------------------------------------------------------
// Busca si existe en el diccionario una combinación
// (prefijo, carácter) y devuelve su índice o -1 si no existe.
// ------------------------------------------------------
static int dict_find(DictEntry *dict, unsigned short prefix, unsigned char ch, int dict_size) {
    for (int i = 256; i < dict_size; ++i) { // Solo busca a partir de códigos >255 (entradas nuevas)
        if (dict[i].used && dict[i].prefix == prefix && dict[i].character == ch)
            return i;
    }
    return -1;
}

// ------------------------------------------------------
// Comprime un buffer usando LZW.
// Entrada: input, input_size
// Salida: buffer comprimido (dinámico), tamaño en *out_size_bytes
// ------------------------------------------------------
uint8_t* lzw_compress(const uint8_t* input, size_t input_size, size_t* out_size_bytes) {
    if (!input) return NULL;

    // Crear diccionario con 65536 entradas (máximo para uint16_t)
    DictEntry *dict = calloc(65536, sizeof(DictEntry));
    if (!dict) return NULL;

    // Inicializar con los 256 símbolos básicos
    for (int i = 0; i < 256; ++i) dict[i].used = 1;
    int dict_size = 256;

    unsigned short prefix = 0;
    int has_prefix = 0;

    // Lista dinámica de códigos resultantes
    uint16_t *codes = malloc(sizeof(uint16_t) * (input_size + 16));
    size_t codes_cap = input_size + 16;
    size_t codes_len = 0;

    // Recorrido de los bytes de entrada
    for (size_t idx = 0; idx < input_size; ++idx) {
        unsigned char ch = input[idx];

        // Primer carácter: se guarda como prefijo
        if (!has_prefix) {
            prefix = ch;
            has_prefix = 1;
            continue;
        }

        // Buscar combinación (prefijo, ch) en el diccionario
        int found = dict_find(dict, prefix, ch, dict_size);

        if (found != -1) {
            // Si existe: el nuevo prefijo es esa entrada
            prefix = (unsigned short)found;
        } else {
            // Si no existe: emitir el código actual
            if (codes_len >= codes_cap) {
                codes_cap *= 2;
                codes = realloc(codes, sizeof(uint16_t) * codes_cap);
            }
            codes[codes_len++] = prefix;

            // Añadir nueva entrada al diccionario
            if (dict_size < 65536) {
                dict[dict_size].used = 1;
                dict[dict_size].prefix = prefix;
                dict[dict_size].character = ch;
                dict_size++;
            }
            // Nuevo prefijo = carácter actual
            prefix = ch;
        }
    }

    // Emitir último prefijo si existe
    if (has_prefix) {
        if (codes_len >= codes_cap) {
            codes_cap *= 2;
            codes = realloc(codes, sizeof(uint16_t) * codes_cap);
        }
        codes[codes_len++] = prefix;
    }

    // ---------------- Empaquetar salida ----------------
    size_t header = sizeof(uint64_t) + sizeof(uint32_t);
    size_t data_bytes = codes_len * sizeof(uint16_t);
    *out_size_bytes = header + data_bytes;

    uint8_t *outbuf = malloc(*out_size_bytes);
    uint8_t *p = outbuf;

    // Guardar tamaño original (8 bytes)
    uint64_t orig = (uint64_t)input_size;
    memcpy(p, &orig, sizeof(uint64_t));
    p += sizeof(uint64_t);

    // Guardar cantidad de códigos generados (4 bytes)
    uint32_t cnt = (uint32_t)codes_len;
    memcpy(p, &cnt, sizeof(uint32_t));
    p += sizeof(uint32_t);

    // Guardar los códigos (2 bytes cada uno)
    memcpy(p, codes, data_bytes);

    // Liberar memoria temporal
    free(dict);
    free(codes);

    return outbuf;
}

// ------------------------------------------------------
// Descomprime un buffer generado por lzw_compress.
// Entrada: buffer comprimido, tamaño
// Salida: buffer original, tamaño en *out_size_bytes
// ------------------------------------------------------
uint8_t* lzw_decompress(const uint8_t* input, size_t input_size, size_t* out_size_bytes) {
    if (!input || input_size < (sizeof(uint64_t) + sizeof(uint32_t))) return NULL;

    const uint8_t *p = input;
    uint64_t orig_size = 0;
    memcpy(&orig_size, p, sizeof(uint64_t));
    p += sizeof(uint64_t);

    uint32_t cnt = 0;
    memcpy(&cnt, p, sizeof(uint32_t));
    p += sizeof(uint32_t);

    const uint16_t *codes = (const uint16_t *)p;

    // Diccionario de secuencias
    unsigned char **seq = calloc(65536, sizeof(unsigned char*));
    size_t *seqlen = calloc(65536, sizeof(size_t));

    // Inicializar con símbolos básicos
    for (int i = 0; i < 256; ++i) {
        seq[i] = malloc(1);
        seq[i][0] = (unsigned char)i;
        seqlen[i] = 1;
    }
    int dict_size = 256;

    // Buffer de salida
    uint8_t *out = malloc(orig_size ? orig_size : 1);
    size_t out_pos = 0;

    uint16_t prev_code = 0;
    int have_prev = 0;

    // Reconstrucción
    for (uint32_t i = 0; i < cnt; ++i) {
        uint16_t code = codes[i];
        unsigned char *entry = NULL;
        size_t entry_len = 0;

        if (code < dict_size && seq[code]) {
            // Entrada conocida
            entry = seq[code];
            entry_len = seqlen[code];
        }
        else if (code == dict_size && have_prev) {
            // Caso especial: entrada aún no registrada
            size_t l = seqlen[prev_code];
            entry = malloc(l + 1);
            memcpy(entry, seq[prev_code], l);
            entry[l] = seq[prev_code][0];
            entry_len = l + 1;
        }
        else {
            entry = NULL;
            entry_len = 0;
        }

        if (entry) {
            // Guardar en salida
            if (out_pos + entry_len > orig_size)
                out = realloc(out, out_pos + entry_len);

            memcpy(out + out_pos, entry, entry_len);
            out_pos += entry_len;

            // Añadir nueva entrada al diccionario
            if (have_prev && dict_size < 65536) {
                size_t l = seqlen[prev_code];
                seq[dict_size] = malloc(l + 1);
                memcpy(seq[dict_size], seq[prev_code], l);
                seq[dict_size][l] = entry[0];
                seqlen[dict_size] = l + 1;
                dict_size++;
            }
        }
        prev_code = code;
        have_prev = 1;
    }

    // Liberar memoria del diccionario
    for (int i = 0; i < dict_size; ++i)
        if (seq[i]) free(seq[i]);
    free(seq);
    free(seqlen);

    *out_size_bytes = out_pos;
    return out;
}

