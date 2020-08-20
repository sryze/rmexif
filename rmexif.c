/*
 * Copyright (c) 2020 Sergey Zolotarev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MARKER_SOI  0xFFD8
#define MARKER_SOF0 0xFFC0
#define MARKER_SOF2 0xFFC2
#define MARKER_DHT  0xFFC4
#define MARKER_DQT  0xFFDB
#define MARKER_DRI  0xFFDD
#define MARKER_SOS  0xFFDA
#define MARKER_RSTN 0xFFD0
#define MARKER_APPN 0xFFE0
#define MARKER_COM  0xFFFE
#define MARKER_EOI  0xFFD9

#ifdef DEBUG
    #define DEBUG_PRINT(...) printf(__VA_ARGS__);
#else
    #define DEBUG_PRINT(...)
#endif

static const char EXIF_HEADER[] = {'E', 'x', 'i', 'f', '\0', '\0'};

static inline uint16_t align16(uint16_t x)
{
    const uint8_t *xb = (uint8_t *)&x;
    return (xb[1] != (x & 0xFF)) ? (((x & 0xFF) << 8) | ((x & 0xFF00) >> 8)): x;
}

static inline void fclose_safe(FILE *file) {
    if (file != NULL) {
        fclose(file);
    }
}

int main(int argc, char **argv)
{
    int i;
    char *path;
    FILE *file = NULL;
    FILE *file_out = NULL;
    uint16_t marker;
    size_t size;
    size_t size_in;
    char *data = NULL;
    char *data_out = NULL;
    char *ptr;
    char *ptr_out;
    char *end;

    if (argc <= 1) {
        fprintf(stderr, "Usage: rmexif <file1> [<file2> [...]]\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        file = NULL;
        file_out = NULL;

        path = argv[i];
        file = fopen(path, "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file \"%s\": %s\n",
                argv[1], strerror(errno));
            goto error;
        }

        (void)fseek(file, 0, SEEK_END);
        size = (size_t)ftell(file);

        if (size == 0) {
            continue;
        }

        data = malloc(size);
        if (data == NULL) {
            fprintf(stderr, "Could not allocate %lu bytes of memory\n",
                    (unsigned long)size);
            goto error;
        }
        data_out = malloc(size);
        if (data_out == NULL) {
            fprintf(stderr, "Could not allocate %lu bytes of memory\n",
                    (unsigned long)size);
            goto error;
        }

        (void)fseek(file, 0, SEEK_SET);
        size_in = fread(data, 1, size, file);
        if (size_in < size) {
            fprintf(stderr, "Could not read contents of \"%s\": %s\n",
                    path, strerror(errno));
            goto error;
        }

        fclose(file);
        file = NULL;

        ptr = data;
        ptr_out = data_out;
        end = data + size_in;

        /* read all segments one by one */

        while (ptr <= end - 2) {
            uint16_t length;
            char *old_ptr = ptr;

            marker = align16(*(uint16_t *)ptr);
            ptr += 2;

            DEBUG_PRINT("Marker: %x\n", marker);

            switch (marker) {
                case MARKER_SOI:
                case MARKER_RSTN:
                case MARKER_RSTN + 1:
                case MARKER_RSTN + 2:
                case MARKER_RSTN + 3:
                case MARKER_RSTN + 4:
                case MARKER_RSTN + 5:
                case MARKER_RSTN + 6:
                case MARKER_RSTN + 7:
                case MARKER_EOI:
                    /* no payload */
                    break;
                case MARKER_SOF0:
                case MARKER_SOF2:
                case MARKER_DHT:
                case MARKER_DQT:
                case MARKER_DRI:
                case MARKER_APPN:
                case MARKER_APPN + 2:
                case MARKER_APPN + 3:
                case MARKER_APPN + 4:
                case MARKER_APPN + 5:
                case MARKER_APPN + 6:
                case MARKER_APPN + 7:
                case MARKER_APPN + 8:
                case MARKER_APPN + 9:
                case MARKER_APPN + 10:
                case MARKER_APPN + 11:
                case MARKER_APPN + 12:
                case MARKER_APPN + 13:
                case MARKER_APPN + 14:
                case MARKER_APPN + 15:
                case MARKER_COM:
                    length = align16(*(uint16_t *)ptr);
                    ptr += length;
                    break;
                case MARKER_SOS:
                    length = 0;
                    while (ptr < end) {
                        if (end - ptr >= 2) {
                            uint16_t maybe_marker = align16(*(uint16_t *)ptr);
                            if (maybe_marker > 0xFF00
                                && (maybe_marker & 0xFFF8) != MARKER_RSTN) {
                                /* found a marker! */
                                break;
                            }
                        }
                        ptr++;
                        length++;
                    }
                    break;
                case MARKER_APPN + 1: {
                    char *payload_ptr = NULL;
                    /*
                     * EXIF data is stored in APP1
                     *
                     * https://www.media.mit.edu/pia/Research/deepview/exif.html
                     */
                    length = align16(*(uint16_t *)ptr);
                    if (length < sizeof(EXIF_HEADER)) {
                        /* not EXIF */
                        ptr += length;
                        continue;
                    }
                    payload_ptr = ptr + 2;
                    ptr += length;
                    if (memcmp(payload_ptr,
                               EXIF_HEADER,
                               sizeof(EXIF_HEADER)) == 0) {
                        continue;
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Unsupported marker: %x\n", marker);
                    goto error;
            }

            if (ptr > old_ptr) {
                memcpy(ptr_out, old_ptr, (size_t)(ptr - old_ptr));
                ptr_out += ptr - old_ptr;
            }

            if (marker == MARKER_EOI) {
                break;
            }
        }

        free(data);
        data = NULL;

        file_out = fopen(path, "wb");
        if (file_out == NULL) {
            fprintf(stderr, "Could not open \"%s\" for writing\n", path);
            goto error;
        }

        fwrite(data_out, 1, ptr_out - data_out, file_out);
        fclose(file_out);
        file_out = NULL;

        free(data_out);
        data_out = NULL;
    }

    return 0;

error:
    free(data);
    free(data_out);
    fclose_safe(file);
    fclose_safe(file_out);

    return 1;
}
