#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ClampToByte
#define  ClampToByte(v)  (((unsigned)(int)(v)) <(255) ? (v) : (v < 0) ? (0) : (255))
#endif

#include <stdint.h>

#if   defined(__APPLE__)
# include <mach/mach_time.h>
#elif defined(_WIN32)
# define WIN32_LEAN_AND_MEAN

# include <windows.h>

#else // __linux

# include <time.h>

# ifndef  CLOCK_MONOTONIC //_RAW
#  define CLOCK_MONOTONIC CLOCK_REALTIME
# endif
#endif

static
uint64_t nanotimer() {
    static int ever = 0;
#if defined(__APPLE__)
    static mach_timebase_info_data_t frequency;
    if (!ever) {
        if (mach_timebase_info(&frequency) != KERN_SUCCESS) {
            return 0;
        }
        ever = 1;
    }
    return  (mach_absolute_time() * frequency.numer / frequency.denom);
#elif defined(_WIN32)
    static LARGE_INTEGER frequency;
    if (!ever) {
        QueryPerformanceFrequency(&frequency);
        ever = 1;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (t.QuadPart * (uint64_t) 1e9) / frequency.QuadPart;
#else // __linux
    struct timespec t;
    if (!ever) {
        if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
            return 0;
        }
        ever = 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec * (uint64_t) 1e9) + t.tv_nsec;
#endif
}


static double now() {
    static uint64_t epoch = 0;
    if (!epoch) {
        epoch = nanotimer();
    }
    return (nanotimer() - epoch) / 1e9;
};

double calcElapsed(double start, double end) {
    double took = -start;
    return took + end;
}

unsigned char *loadImage(const char *filename, int *width, int *height, int *channels) {
    return stbi_load(filename, width, height, channels, 0);
}

void saveImage(const char *filename, int width, int height, int channels, unsigned char *Output) {
    if (!stbi_write_jpg(filename, width, height, channels, Output, 100)) {
        fprintf(stderr, "save file fail.\n");
        return;
    }
}

void splitpath(const char *path, char *drv, char *dir, char *name, char *ext) {
    const char *end;
    const char *p;
    const char *s;
    if (path[0] && path[1] == ':') {
        if (drv) {
            *drv++ = *path++;
            *drv++ = *path++;
            *drv = '\0';
        }
    } else if (drv)
        *drv = '\0';
    for (end = path; *end && *end != ':';)
        end++;
    for (p = end; p > path && *--p != '\\' && *p != '/';)
        if (*p == '.') {
            end = p;
            break;
        }
    if (ext)
        for (s = end; (*ext = *s++);)
            ext++;
    for (p = end; p > path;)
        if (*--p == '\\' || *p == '/') {
            p++;
            break;
        }
    if (name) {
        for (s = p; s < end;)
            *name++ = *s++;
        *name = '\0';
    }
    if (dir) {
        for (s = path; s < p;)
            *dir++ = *s++;
        *dir = '\0';
    }
}


unsigned int skinDetection(unsigned char *rgb_src, int width, int height, int channels) {
    int stride = width * channels;
    int lastCol = width * channels - channels;
    int lastRow = height * stride - stride;
    unsigned int sum = 0;
    for (int y = 0; y < height; y++) {
        int cur_row = stride * y;
        int next_row = min(cur_row + stride, lastRow);
        unsigned char *next_scanLine = rgb_src + next_row;
        unsigned char *cur_scanLine = rgb_src + cur_row;
        for (int x = 0; x < width; x++) {
            int cur_col = x * channels;
            int next_col = min(cur_col + channels, lastCol);
            unsigned char *c00 = cur_scanLine + cur_col;
            unsigned char *c10 = cur_scanLine + next_col;
            unsigned char *c01 = next_scanLine + cur_col;
            unsigned char *c11 = next_scanLine + next_col;
            int r_avg = ((c00[0] + c10[0] + c01[0] + c11[0])) >> 2;
            int g_avg = ((c00[1] + c10[1] + c01[1] + c11[1])) >> 2;
            int b_avg = ((c00[2] + c10[2] + c01[2] + c11[2])) >> 2;
            if (r_avg >= 60 && g_avg >= 40 && b_avg >= 20 && r_avg >= b_avg && (r_avg - g_avg) >= 10 &&
                max(max(r_avg, g_avg), b_avg) - min(min(r_avg, g_avg), b_avg) >= 10) {
                sum++;
            }
        }
    }
    return sum;
}

void skinFilter(unsigned char *input, unsigned char *output, int width, int height, int channels) {
    int stride = width * channels;
    int lastCol = width * channels - channels;
    int lastRow = height * stride - stride;
    for (int y = 0; y < height; y++) {
        int cur_row = stride * y;
        int next_row = min(cur_row + stride, lastRow);
        unsigned char *next_scanOutLine = output + next_row;
        unsigned char *cur_scanOutLine = output + cur_row;
        unsigned char *scanOutLine = output + y * stride;
        unsigned char *scanInLine = input + y * stride;
        for (int x = 0; x < width; x++) {
            int cur_col = x * channels;
            int next_col = min(cur_col + channels, lastCol);
            unsigned char *c00 = cur_scanOutLine + cur_col;
            unsigned char *c10 = cur_scanOutLine + next_col;
            unsigned char *c01 = next_scanOutLine + cur_col;
            unsigned char *c11 = next_scanOutLine + next_col;
            int r_avg = ((c00[0] + c10[0] + c01[0] + c11[0])) >> 2;
            int g_avg = ((c00[1] + c10[1] + c01[1] + c11[1])) >> 2;
            int b_avg = ((c00[2] + c10[2] + c01[2] + c11[2])) >> 2;
            int is_skin = !(r_avg >= 60 && g_avg >= 40 && b_avg >= 20 && r_avg >= b_avg && (r_avg - g_avg) >= 10 &&
                            max(max(r_avg, g_avg), b_avg) - min(min(r_avg, g_avg), b_avg) >= 10);
            if (is_skin)
                for (int c = 0; c < channels; ++c)
                    scanOutLine[c] = scanInLine[c];
            scanOutLine += channels;
            scanInLine += channels;
        }
    }
}

void getOffsetPos(int *offsetPos, int length, int left, int right, int step) {
    if (offsetPos == NULL) return;
    if ((length < 0) || (left < 0) || (right < 0))
        return;
    for (int x = -left; x < length + right; x++) {
        int pos = x;
        int length2 = length + length;
        if (pos < 0) {
            do {
                pos += length2;
            } while (pos < 0);
        } else if (pos >= length2) {
            do {
                pos -= length2;
            } while (pos >= length2);
        }
        if (pos >= length)
            pos = length2 - 1 - pos;
        offsetPos[x + left] = pos * step;
    }
}

int Abs(int v) {
    return (v ^ (v >> 31)) - (v >> 31);
}

void skinDenoise(unsigned char *input, unsigned char *output, int width, int height, int channels, int radius,
                 int smoothingLevel) {
    if ((input == NULL) || (output == NULL)) return;
    if ((width <= 0) || (height <= 0)) return;
    if ((radius <= 0) || (smoothingLevel <= 0)) return;
    if ((channels != 1) && (channels != 3)) return;
    int windowSize = (2 * radius + 1) * (2 * radius + 1);
    int *colPower = (int *) malloc(width * channels * sizeof(int));
    int *colValue = (int *) malloc(width * channels * sizeof(int));
    int *rowPos = (int *) malloc((width + radius + radius) * channels * sizeof(int));
    int *colPos = (int *) malloc((height + radius + radius) * channels * sizeof(int));
    if ((colPower == NULL) || (colValue == NULL) || (rowPos == NULL) || (colPos == NULL)) {
        if (colPower) free(colPower);
        if (colValue) free(colValue);
        if (rowPos) free(rowPos);
        if (colPos) free(colPos);
        return;
    }
    int stride = width * channels;
    int smoothLut[256] = {0};
    float ii = 0.f;
    for (int i = 0; i <= 255; i++, ii -= 1.) {
        smoothLut[i] = (int) ((expf(ii * (1.0f / (smoothingLevel * 255.0f))) + (smoothingLevel * (i + 1)) + 1) * 0.5f);
        smoothLut[i] = max(smoothLut[i], 1);
    }
    getOffsetPos(rowPos, width, radius, radius, channels);
    getOffsetPos(colPos, height, radius, radius, stride);
    int *rowOffset = rowPos + radius;
    int *colOffSet = colPos + radius;
    for (int y = 0; y < height; y++) {
        unsigned char *scanInLine = input + y * stride;
        unsigned char *scanOutLine = output + y * stride;
        if (y == 0) {
            for (int x = 0; x < stride; x += channels) {
                int colSum[3] = {0};
                int colSumPow[3] = {0};
                for (int z = -radius; z <= radius; z++) {
                    unsigned char *sample = input + colOffSet[z] + x;
                    for (int c = 0; c < channels; ++c) {
                        colSum[c] += sample[c];
                        colSumPow[c] += sample[c] * sample[c];
                    }
                }
                for (int c = 0; c < channels; ++c) {
                    colValue[x + c] = colSum[c];
                    colPower[x + c] = colSumPow[c];
                }
            }
        } else {
            unsigned char *lastCol = input + colOffSet[y - radius - 1];
            unsigned char *nextCol = input + colOffSet[y + radius];
            for (int x = 0; x < stride; x += channels) {
                for (int c = 0; c < channels; ++c) {
                    colValue[x + c] -= lastCol[x + c] - nextCol[x + c];
                    colPower[x + c] -= lastCol[x + c] * lastCol[x + c] - nextCol[x + c] * nextCol[x + c];
                }
            }
        }
        int prevSum[3] = {0};
        int prevPowerSum[3] = {0};
        for (int z = -radius; z <= radius; z++) {
            int index = rowOffset[z];
            for (int c = 0; c < channels; ++c) {
                prevSum[c] += colValue[index + c];
                prevPowerSum[c] += colPower[index + c];
            }
        }
        for (int c = 0; c < channels; ++c) {
            const int mean = prevSum[c] / windowSize;
            const int diff = mean - scanInLine[c];
            const int edge = ClampToByte(diff);
            const int masked_edge = (edge * scanInLine[c] + (256 - edge) * mean) >> 8;
            const int var = Abs(prevPowerSum[c] - mean * prevSum[c]) / windowSize;
            const int out = masked_edge - diff * var / (var + smoothLut[scanInLine[c]]);
            scanOutLine[c] = ClampToByte(out);
        }
        scanInLine += channels;
        scanOutLine += channels;
        for (int x = 1; x < width; x++) {
            int lastRow = rowOffset[x - radius - 1];
            int nextRow = rowOffset[x + radius];
            for (int c = 0; c < channels; ++c) {
                prevSum[c] = prevSum[c] - colValue[lastRow + c] + colValue[nextRow + c];
                prevPowerSum[c] = prevPowerSum[c] - colPower[lastRow + c] + colPower[nextRow + c];
                const int mean = prevSum[c] / windowSize;
                const int diff = mean - scanInLine[c];
                const int edge = ClampToByte(diff);
                const int masked_edge = (edge * scanInLine[c] + (256 - edge) * mean) >> 8;
                const int var = Abs(prevPowerSum[c] - mean * prevSum[c]) / windowSize;
                const int out = masked_edge - diff * var / (var + smoothLut[scanInLine[c]]);
                scanOutLine[c] = ClampToByte(out);
            }
            scanInLine += channels;
            scanOutLine += channels;
        }
    }
    if (colPower) free(colPower);
    if (colValue) free(colValue);
    if (rowPos) free(rowPos);
    if (colPos) free(colPos);
}


void skinSmoothing(unsigned char *input, unsigned char *output, int width, int height, int channels,
                   int smoothingLevel, int apply_skin_filter) {
    if (input == NULL || output == NULL || width == 0 || height == 0 || channels == 1)
        return;
    //1.detect skin color, adapt radius according to skin color ratio
    unsigned int skinSum = skinDetection(input, width, height, channels);
    float skin_rate = skinSum / (float) (width * height) * 100;
    int radius = min(width, height) / skin_rate + 1;
    //2.perform edge detection to obtain a edge map && smoothing level for apply skin denoise
    skinDenoise(input, output, width, height, channels, radius, smoothingLevel);
    //3.re-detect skin color based on the denoise results, filtered non-skin areas
    if (apply_skin_filter)
        skinFilter(input, output, width, height, channels);
}


int main(int argc, char **argv) {
    printf("Image Processing \n ");
    printf("blog:http://cpuimage.cnblogs.com/ \n ");
    printf("Skin Smoothing\n ");
    if (argc < 2) {
        printf("usage: \n ");
        printf("%s filename \n ", argv[0]);
        printf("%s image.jpg \n ", argv[0]);
        getchar();
        return 0;
    }
    char *in_file = argv[1];
    char drive[3];
    char dir[256];
    char fname[256];
    char ext[256];
    char out_file[1024];
    splitpath(in_file, drive, dir, fname, ext);
    sprintf(out_file, "%s%s%s_out.jpg", drive, dir, fname);
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *input = NULL;
    input = loadImage(in_file, &width, &height, &channels);
    if (input) {
        unsigned char *output = (unsigned char *) calloc(width * channels * height * sizeof(unsigned char), 1);
        if (output) {
            int smoothingLevel = 10;
            int apply_skin_filter = 0;
            double startTime = now();
            skinSmoothing(input, output, width, height, channels, smoothingLevel, apply_skin_filter);
            double elapsed = calcElapsed(startTime, now());
            printf("elapsed time: %d ms.\n ", (int) (elapsed * 1000));
            saveImage(out_file, width, height, channels, output);
            free(output);
        }
        free(input);
    } else {
        printf("load file: %s fail!\n", in_file);
    }
    printf("press any key to exit. \n");
    getchar();
    return 0;
}
