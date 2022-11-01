#ifndef BLOCS__ATLAS
#define BLOCS__ATLAS

////////////////////////////////////
//
// debug methods for logging program
// progress, benchmark, and assertions
//

#include <stdarg.h>
#include <stdio.h>

enum
{
    blocs__log_INFO  = 6,
    blocs__log_GOOD  = 2,
    blocs__log_WARN  = 3,
    blocs__log_ERROR = 1,
    blocs__log_WHITE = 7,
};

#include <stdlib.h>

void blocs__log(int type, const char* message, ...);
void blocs__assert(int condition, const char* message, ...);

#include <time.h>

double blocs__get_time_ms();

////////////////////////////////////
//
// image loading, unloading, saving
// and pixel data manipulation
//

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define CHANNELS 4

typedef struct blocs__image
{
    int32_t     w;
    int32_t     h;
    uint8_t*    data;
} blocs__image;

int blocs__load_from_path(const char* path, blocs__image* image);
void blocs__unload_image(blocs__image* image);
void blocs__clear_pixels(blocs__image* image);
void blocs__save_png(const char* output, blocs__image* image);
size_t blocs__generate_hash(blocs__image* image, int len);
int blocs__check_hashes(size_t hash, size_t* hashes, int len);

////////////////////////////////////
//
// texture atlas generation
//

#define max(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

typedef struct blocs__rect
{
    int32_t     x;
    int32_t     y;
    int32_t     w;
    int32_t     h;
} blocs__rect;

typedef struct blocs__texture
{
    blocs__rect rect;
    int32_t     name_index;
    uint32_t    buffer_index;
} blocs__texture;

int blocs__compare_area(const void* a, const void* b);
void blocs__add_texture(blocs__texture* textures, int index, uint8_t* buffer, uint32_t* buffer_index, blocs__image image, int32_t name_index);
void blocs__pack_atlas(blocs__texture* textures, int len, int size, int expand, int border);
void blocs__set_pixels(blocs__image* image, uint8_t* data, blocs__rect dst);
void blocs__generate_atlas(blocs__texture* textures, int len, uint8_t* buffer, blocs__image* atlas_bmp, int expand);
void blocs__save_json(const char* output, blocs__image* atlas_bmp, blocs__texture* textures, int len, const char** names);

////////////////////////////////////
//
// file system apis
//

#include <dirent.h>

#define PNG_EXT "png"
#define JPG_EXT "jpg"

const char* blocs__file_path(const char* file);
const char* blocs__file_name(const char* file);
const char* blocs__file_ext(const char* file);
int blocs__ext_is_img(const char* ext);
int blocs__size_dir(const char* path);
void blocs__enumerate_dir(const char* path, char** files, int* index);

////////////////////////////////////
//
// demo structs and funcs for
// random HSLA box generation
//

typedef struct blocs__color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} blocs__color;

float blocs__hue(float p, float q, float t);
void blocs__fill_color(blocs__image* image, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
blocs__color blocs__hsla(float h, float s, float l, float a);
blocs__image blocs__rand_box(int w, int h);

#endif
