/*

MIT License

Copyright (c) 2022 Benjamin Trosch

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

////////////////////////////////////

blocs__atlas v0.x.y
===================

usage:
    pack [INPUT] -o [OUTPUT] [OPTIONS...] 

example:
    pack ~/assets/sprites/ -o ~assets/atlas.png -s 256 -e 2 -v

demo:
    pack --demo -s 960 -b 4

options:
    -o  --output            sets output file name and destination
    -v  --verbose           print packer state to the console
    -u  --unique            remove duplicates from the atlas (TODO...)
    -e  --expand            repeat pixels along image edges
    -b  --border            empty border space between images
    -s  --size              sets atlas size (width and height equal)
    -d  --demo              generates random boxes (exclude first arg)
*/

#include "main.h"

////////////////////////////////////
//
// debug methods for logging program
// progress, benchmark, and assertions
//

void blocs__log(int type, const char* message, ...)
{
    char s[1024];
    va_list vl;
    va_start(vl, message);
    vsnprintf(s, sizeof(char) * 1024, message, vl);
    va_end(vl);
    printf("\x1B[3%dm%s\x1B[0m\n", type, s);
}

void blocs__assert(int condition, const char* message, ...)
{
    if (condition) return;
    char s[1024];
    va_list vl;
    va_start(vl, message);
    vsnprintf(s, sizeof(char) * 1024, message, vl);
    va_end(vl);
    printf("\x1B[31mERROR: %s\x1B[0m\n", s);
    abort();
}

/**
 * @brief Returns current time in milliseconds
 * 
 * @return double 
 */
double blocs__get_time_ms()
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

////////////////////////////////////
//
// image loading, unloading, saving
// and pixel data manipulation
//

/**
 * @brief       Reads RGBA pixel data from an image located
 *              at the provided file path
 * 
 * @param path  File path to read from
 * @return      true 
 * @return      false 
 */
int blocs__load_from_path(const char* path, blocs__image* image)
{
    int bpp;
    image->data = stbi_load(path, &image->w, &image->h, &bpp, CHANNELS);
    return image->data != NULL;
}

/**
 * @brief       Frees bitmaps data from memory
 * 
 * @param image Image to clean up
 */
void blocs__unload_image(blocs__image* image)
{
    blocs__assert(image->data != NULL, "cannot dispose an unloaded image");
    stbi_image_free(image->data);
}

/**
 * @brief       Fills a bitmap's pixel data with transparent
 *              empty pixels (RGBA of 0U)
 * 
 * @param image Image to clear
 */
void blocs__clear_pixels(blocs__image* image)
{
    int w = image->w;
    int h = image->h;
    image->data = (uint8_t*)malloc(w * h * CHANNELS * sizeof(uint8_t));
    memset(image->data, 0U, w * h * CHANNELS * sizeof(uint8_t));
}

/**
 * @brief       Saves bitmap data as a png file
 * 
 * @param image Image being saved
 */
void blocs__save_png(const char* output, blocs__image* image)
{
    blocs__assert(image->data != NULL, "cannot save unloaded image");
    blocs__assert(image->w > 0 && image->h > 0, "image too small to save");
	
    // TODO: custom compression settings
    stbi_write_force_png_filter = 0;
    stbi_write_png_compression_level = 0;

    stbi_write_png(output, image->w, image->h, CHANNELS, image->data, image->w * CHANNELS);
}

////////////////////////////////////
//
// texture atlas generation
//

int blocs__compare_area(const void* a, const void* b)
{
    blocs__rect rect_a = ((blocs__texture*)a)->rect;
    blocs__rect rect_b = ((blocs__texture*)b)->rect;
    return rect_b.h - rect_a.h;
}

/**
 * @brief               Adds a texture to the list of textures to be packed
 *                      and adds bitmap data to buffer
 * 
 * @param textures      Textures to be packed
 * @param index         Index to add texture to in list
 * @param buffer        Buffer of pixel data for atlas
 * @param buffer_index  Current index of buffer
 * @param data          New bitmap data being added
 * @param w             Width of new bitmap
 * @param h             Height of new bitmap
 */
void blocs__add_texture(blocs__texture* textures, int index, uint8_t* buffer, uint32_t* buffer_index, blocs__image image, int32_t name_index)
{
    blocs__assert(image.data != NULL, "could not read texture data");

    blocs__texture texture = {
        { 0, 0, image.w, image.h },
        name_index,
        (*buffer_index),
    };
    textures[index] = texture;
    uint32_t buffer_length = image.w * image.h * CHANNELS;
    memcpy(buffer + (*buffer_index), image.data, buffer_length);
    (*buffer_index) += buffer_length;
}

/**
 * @brief               Packs bitmap rects into smallest possible
 *                      configuration and updates texture positions
 * 
 * @param textures      Textures being packed
 * @param len           Number of textures being packed
 * @param size          Maximum size of the final packed atlas
 * @param expand        Amount of pixels to repeat on each side of bitmap
 * @param border        Amount of empty space between bitmaps
 */
void blocs__pack_atlas(blocs__texture* textures, int len, int size, int expand, int border)
{
    int area  = 0;
    int max_w = 0;
    int max_h = 0;
    int padding = expand * 2 + border;
    for (int i = 0; i < len; i++)
    {
        blocs__rect rect = textures[i].rect;
        area += (rect.w + padding) * (rect.h + padding);
        max_w = max(max_w, rect.w + padding);
        max_h = max(max_h, rect.h + padding);
    }
    double suboptimal_coefficient = 0.85; // assumes sub-100% space utilization

    blocs__assert(max_w <= size && max_h <= size,
        "max size needed (%dpx, %dpx) larger than atlas size (%dpx)",
        max_w, max_h, size);
    blocs__assert(area <= size * size * suboptimal_coefficient,
        "total area needed (%dpx) cannot fit in atlas size (%.fpx, AKA %d x %d with %.2f%% space utilization)",
        area, size * size * suboptimal_coefficient, size, size, suboptimal_coefficient);

    blocs__rect spaces[len];
    blocs__rect start_space = { 0, 0, size, size };
    spaces[0] = start_space;
    int n = 1;

    // sort images tallest to shortest
    qsort(textures, len, sizeof(int32_t) * 5 + sizeof(uint32_t), blocs__compare_area);

    for (int i = 0; i < len; i++)
    {
        blocs__texture* texture = &textures[i];
        blocs__rect rect = (&textures[i])->rect;

        blocs__assert(rect.w <= size && rect.h <= size,
            "texture larger (%dpx, %dpx) than maximum size (%dpx)",
            rect.w, rect.h, size);

        for (int j = n - 1; j >= 0; j--)
        {
            blocs__rect space = spaces[j];

            int w = rect.w + padding;
            int h = rect.h + padding;

            // check if image too large for space
            if (w > space.w || h > space.h)
                continue;

            // add image to space's top-left
            // |-------|-------|
            // |  box  |       |
            // |_______|       |
            // |         space |
            // |_______________|
            textures[i].rect.x = space.x + expand;
            textures[i].rect.y = space.y + expand;

            if (w == space.w && h == space.h)
            {
                // remove space if perfect fit
                // |---------------|
                // |               |
                // |      box      |
                // |               |
                // |_______________|
                blocs__rect last = spaces[--n];                
                if (j < n)
                    spaces[j] = last;
            }
            else if (h == space.h)
            {
                // space matches image height
                // move space right and cut off width
                // |-------|---------------|
                // |  box  | updated space |
                // |_______|_______________|
                spaces[j].x += w;
                spaces[j].w -= w;
            }
            else if (w == space.w)
            {
                // space matches image width
                // move space down and cut off height
                // |---------------|
                // |      box      |
                // |_______________|
                // | updated space |
                // |_______________|
                spaces[j].y += h;
                spaces[j].h -= h;
            }
            else
            {
                // split width and height
                // difference into two new spaces
                // |-------|-----------|
                // |  box  | new space |
                // |_______|___________|
                // | updated space     |
                // |___________________|
                blocs__rect new_space = {
                    space.x + w,
                    space.y,
                    space.w - w,
                    h
                };
                spaces[n++] = new_space;
                spaces[j].y += h;
                spaces[j].h -= h;
            }

            break;
        }
    }
}

/**
 * @brief       Blits pixel data onto a portion on a bitmap
 * 
 * @param image Target image being altered
 * @param data  New pixel data
 * @param dst   Destination rect on the image
 */
void blocs__set_pixels(blocs__image* image, uint8_t* data, blocs__rect dst)
{
    blocs__assert(dst.x + dst.w <= image->w && dst.y + dst.h <= image->h,
        "new pixels (%dpx, %dpx) cannot be larger than image (%dpx, %dpx)",
        dst.w, dst.y, image->w, image->h);
        
    for (int y = 0; y < dst.h; y++)
    {
        int from = y * dst.w;
        int to = dst.x + (dst.y + y) * image->w;
        memcpy(
            image->data + to * CHANNELS,
            data + from * CHANNELS,
            dst.w * CHANNELS * sizeof(uint8_t)
        );
    }
}

/**
 * @brief               Generates atlas texture data based on packed
 *                      rects using the buffer of pixels
 * 
 * @param textures      Textures used for position
 * @param len           Number of textures being packed
 * @param buffer        Buffer of pixel data
 * @param atlas_bmp     Final atlas bitmap
 * @param expand        Amount of pixels to repeat on each side of bitmap
 */
void blocs__generate_atlas(blocs__texture* textures, int len, uint8_t* buffer, blocs__image* atlas_bmp, int expand)
{
    for (int i = 0; i < len; i++)
    {
        blocs__texture texture = textures[i];
        blocs__rect rect = texture.rect;

        if (expand > 0)
        {
            uint8_t* src_buffer = buffer + texture.buffer_index;
            uint8_t* dst_buffer = atlas_bmp->data;
            for (int y = -expand * CHANNELS; y < (rect.h + expand) * CHANNELS; y += CHANNELS)
            {
                for (int x = -expand * CHANNELS; x < (rect.w + expand) * CHANNELS; x += CHANNELS)
                {
                    int src_x = x < 0 ?
                        0 : x > (rect.w - 1) * CHANNELS ?
                            (rect.w - 1) * CHANNELS : x;
                    int src_y = y < 0 ?
                        0 : y > (rect.h - 1) * CHANNELS ?
                            (rect.h - 1) * CHANNELS : y;

                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*atlas_bmp->w+0] = src_buffer[src_x+src_y*rect.w+0];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*atlas_bmp->w+1] = src_buffer[src_x+src_y*rect.w+1];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*atlas_bmp->w+2] = src_buffer[src_x+src_y*rect.w+2];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*atlas_bmp->w+3] = src_buffer[src_x+src_y*rect.w+3];
                }
            }
        }
        else
            blocs__set_pixels(atlas_bmp, buffer + texture.buffer_index, rect);
    }
}

void blocs__save_json(const char* output, blocs__image* atlas_bmp, blocs__texture* textures, int len, const char** names)
{
    FILE* file = fopen(output, "w");
    fprintf(file, "{\n");
    fprintf(file, "\t\"w\": %d,\n", atlas_bmp->w);
    fprintf(file, "\t\"h\": %d,\n", atlas_bmp->h);
    fprintf(file, "\t\"n\": %d,\n", len);
    fprintf(file, "\t\"textures\": [\n");
    for (int i = 0; i < len; i++)
    {
        blocs__texture texture = textures[i];
        blocs__rect rect = texture.rect;

        fprintf(file, "\t\t{\n");
        fprintf(file, "\t\t\t\"n\": \"%s\",\n", names[texture.name_index]);
        fprintf(file, "\t\t\t\"x\": %d,\n", rect.x);
        fprintf(file, "\t\t\t\"y\": %d,\n", rect.y);
        fprintf(file, "\t\t\t\"w\": %d,\n", rect.w);
        fprintf(file, "\t\t\t\"h\": %d\n", rect.h);
        fprintf(file, "\t\t}");
        if (i != len - 1)
            fprintf(file, ",\n");
    }
    fprintf(file, "\n\t]\n");
    fprintf(file, "}");
    fclose(file);
}

////////////////////////////////////
//
// file system apis
//

/**
 * @brief       Gets the file path to a file without
 *              the file name
 * 
 * @param file  Path to file
 * @return const char*
 */
const char* blocs__file_path(const char* file)
{
    char* path = malloc(strlen(file + 1));
    strcpy(path, file);
    const char* slash = strrchr(path, '/');
    int index = slash - path;
    path[index] = '\0';
    return path;
}

/**
 * @brief       Gets the file name without extension
 *              from a file path
 * 
 * @param file  File path
 * @return const char*
 */
const char* blocs__file_name(const char* file)
{
    const char* slash = strrchr(file, '/');
    const char* file_without_path = !slash || slash == file ? file : slash + 1;

    char* name = malloc(strlen(file_without_path + 1));
    strcpy(name, file_without_path);

    const char* dot = strrchr(name, '.');
    if (!dot || dot == name) return name;
    int index = dot - name;
    name[index] = '\0';

    return name;
}

/**
 * @brief       Gets the file extension from a file path
 * 
 * @param file  File path
 * @return const char*
 */
const char* blocs__file_ext(const char* file)
{
    const char* dot = strrchr(file, '.');
    if (!dot || dot == file) return file;
    return dot + 1;
}

/**
 * @brief       Checks whether an image's extension
 *              matches the supported image file types
 * 
 * @param ext   File extension
 * @return true 
 * @return false 
 */
int blocs__ext_is_img(const char* ext)
{
    return strcmp(ext, PNG_EXT) == 0 || strcmp(ext, JPG_EXT) == 0;
}

/**
 * TODO:
 * make `blocs__size_dir` and `blocs__enumerate_dir` work
 * on windows and check OS using ifdef _WIN32 directive...
 */

/**
 * @brief       Gets number of files inside of a directory (recursive)
 * 
 * @param path  Directory to search in
 * @return int
 */
int blocs__size_dir(const char* path)
{
    DIR* dir_ptr = NULL;
    struct dirent* direntp;
    char* npath;
    blocs__assert((dir_ptr = opendir(path)) != NULL, "could not count files in directory \"%s\"", path);

    int count = 0;
    while ((direntp = readdir(dir_ptr)))
    {
        if (strcmp(direntp->d_name, ".") == 0 ||
            strcmp(direntp->d_name, "..") == 0)
            continue;

        switch (direntp->d_type)
        {
            case DT_REG:
                ++count;
                break;

            case DT_DIR:            
                npath = (char*)malloc(strlen(path) + strlen(direntp->d_name) + 2);
                sprintf(npath, "%s/%s", path, direntp->d_name);
                count += blocs__size_dir(npath);
                free(npath);
                break;
        }
    }

    closedir(dir_ptr);
    return count;
}

/**
 * @brief       Gets all file names inside of a directory (recursive)
 * 
 * @param path  Directory to search in
 * @param files List of file names
 * @param index Current index of files list
 */
void blocs__enumerate_dir(const char* path, char** files, int* index)
{
    DIR* dir_ptr = NULL;
    struct dirent* direntp;
    char* npath;
    blocs__assert((dir_ptr = opendir(path)) != NULL, "could not open directory \"%s\"", path);

    while ((direntp = readdir(dir_ptr)) != NULL)
    {
        if (strcmp(direntp->d_name, ".") == 0 ||
            strcmp(direntp->d_name, "..") == 0)
            continue;

        switch (direntp->d_type)
        {
            case DT_REG:
                files[*index] = (char*)malloc(strlen(path) + strlen(direntp->d_name) + 2);
                sprintf(files[*index], "%s/%s", path, direntp->d_name);
                (*index)++;
                break;

            case DT_DIR:            
                npath = (char*)malloc(strlen(path) + strlen(direntp->d_name) + 2);
                sprintf(npath, "%s/%s", path, direntp->d_name);
                blocs__enumerate_dir(npath, files, index);
                free(npath);
                break;
        }
    }

    closedir(dir_ptr);
}

////////////////////////////////////
//
// demo structs and funcs for
// random HSLA box generation
//

float blocs__hue(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.f / 6.f) 
        return p + (q - p) * 6.f * t;
    if (t < 1.f / 2.f) 
        return q;
    if (t < 2.f / 3.f)   
        return p + (q - p) * (2.f / 3.f - t) * 6.f;
    return p;
}

/**
 * @brief       Fill bitmap's pixel data with
 *              an RGBA color
 * 
 * @param image Image to fill
 */
void blocs__fill_color(blocs__image* image, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    int w = image->w;
    int h = image->h;
    image->data = (uint8_t*)malloc(w * h * CHANNELS * sizeof(uint8_t));
    for (int i = 0; i < w * h * CHANNELS; i += CHANNELS)
    {
        image->data[i + 0] = r;
        image->data[i + 1] = g;
        image->data[i + 2] = b;
        image->data[i + 3] = a;
    }
}

/**
 * @brief           Converts an HSLA color to RGBA
 * 
 * @param h         Hue
 * @param s         Saturation
 * @param l         Lightness
 * @param a         Alpha
 * 
 * @return blocs__color
 */
blocs__color blocs__hsla(float h, float s, float l, float a)
{
    blocs__color color;
    color.a = a * 255U;
    if (s == 0.f)
        color.r = color.g = color.b = l * 255U;
    else
    {
        float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
        float p = 2.f * l - q;

        color.r = blocs__hue(p, q, h + 1.f / 3.f) * 255U;
        color.g = blocs__hue(p, q, h) * 255U;
        color.b = blocs__hue(p, q, h - 1.f / 3.f) * 255U;
    }

    return color;
}

blocs__image blocs__rand_box(int w, int h)
{
    blocs__image image = { w, h };
    blocs__color color = blocs__hsla((float)rand() / RAND_MAX, 1.0f, 0.7f, 1.0f);
    blocs__fill_color(&image, color.r, color.g, color.b, color.a);
    return image;
}

////////////////////////////////////

const char*     input_dir;
const char*     output_dir;
const char*     output_name;

int             log_verbose;
int             demo;

uint8_t*        buffer;
uint32_t        buffer_index;
blocs__image*   atlas_bmp;

int32_t         atlas_size;
int32_t         atlas_expand;
int32_t         atlas_border;
int             atlas_unique;

const char**    names;
blocs__image*   images;
blocs__texture* textures;

int main(int argc, const char *argv[])
{
    blocs__assert(argc > 2, "expected \"pack [INPUT] [OPTS...]\"");

    // Set default output dir to "atlas.png" in relative path
    output_dir = ".";
    output_name = "atlas";
    // Set default size to 4k
    atlas_size = 4096;

    // Parse command line arguments
    {
        input_dir = argv[1];

        // If first arg is demo, set demo defaults
        if (strcmp(input_dir, "-d") == 0 || strcmp(input_dir, "--demo") == 0)
        {
            demo = 1;
            output_name = "demo";
            atlas_size = 960;
        }

        for (int i = 2; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0)
            {
                i++;
                blocs__assert(i < argc, "went out of bounds looking for output argument value");
                output_dir = blocs__file_path(argv[i]);
                output_name = blocs__file_name(argv[i]);
            }
            else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--size") == 0)
            {
                i++;
                blocs__assert(i < argc, "went out of bounds looking for size argument value");
                atlas_size = atoi(argv[i]);
            }
            else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
                log_verbose = 1;
            else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--unique") == 0)
                atlas_unique = 1;
            else if (strcmp(arg, "-e") == 0 || strcmp(arg, "--expand") == 0)
            {
                i++;
                blocs__assert(i < argc, "went out of bounds looking for expand argument value");
                atlas_expand = atoi(argv[i]);
            }
            else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--border") == 0)
            {
                i++;
                blocs__assert(i < argc, "went out of bounds looking for border argument value");
                atlas_border = atoi(argv[i]);
            }
            else
                blocs__assert(0, "unrecognized arg \"%s\"", arg);
        }
    }

    // Set start time for logging
    double time_start, time_prev, time_curr;
    time_start = time_prev = time_curr = blocs__get_time_ms();

    if (log_verbose)
    {
        blocs__log(blocs__log_WHITE, "blocs___atlas v.0.x");
        blocs__log(blocs__log_WHITE, "===================");
        blocs__log(blocs__log_WHITE, "Begin Texture Atlas");
    }

    int num_images = 0;
    // Find images from input directory in first argument
    {
        if (!demo)
        {
            int num_files = blocs__size_dir(input_dir);
            int n = 0;
            char* files[num_files];
            blocs__enumerate_dir(input_dir, files, &n);

            if (log_verbose)
            {
                time_curr = blocs__get_time_ms();
                blocs__log(blocs__log_WHITE,
                    " - Find Graphics ............. %.2fms",
                    time_curr - time_prev
                );
                time_prev = time_curr;
            }

            images = (blocs__image*)malloc(sizeof(blocs__image) * num_files);
            names = (const char**)malloc(1024 * num_files);

            for (int i = 0; i < num_files; i++)
            {
                const char* f = files[i];
                const char* ext = blocs__file_ext(f);
                if (blocs__ext_is_img(ext))
                {
                    blocs__image image;
                    if (blocs__load_from_path(f, &image))
                    {
                        names[num_images] = blocs__file_name(f);
                        images[num_images] = image;
                        if (log_verbose)
                            blocs__log(blocs__log_GOOD, "   ✓ \"%s\"", f);
                        num_images++;
                    }
                    else
                        if (log_verbose)
                            blocs__log(blocs__log_ERROR, "   x \"%s\"", f);
                }
            }
        }
        else
        {
            images = (blocs__image*)malloc(sizeof(blocs__image) * 1000);
            
            const char* box_name = "box";
            names = (const char**)malloc(sizeof(box_name));
            names[0] = box_name;

            srand(time(NULL));
            if (((float)rand() / RAND_MAX) > 0.5)         images[num_images++] = blocs__rand_box(400,  80);
            if (((float)rand() / RAND_MAX) > 0.5)         images[num_images++] = blocs__rand_box( 80, 400);
            if (((float)rand() / RAND_MAX) > 0.5)         images[num_images++] = blocs__rand_box(250, 250);
            if (((float)rand() / RAND_MAX) > 0.5)         images[num_images++] = blocs__rand_box(100, 250);
            if (((float)rand() / RAND_MAX) > 0.5)         images[num_images++] = blocs__rand_box(250, 100);
            for (int i = rand() % 20; i >= 0; i--)        images[num_images++] = blocs__rand_box(100, 100);
            for (int i = rand() % 10; i >= 0; i--)        images[num_images++] = blocs__rand_box( 60,  60);
            for (int i = rand() % 30; i >= 0; i--)        images[num_images++] = blocs__rand_box( 50,  50);
            for (int i = rand() % 40; i >= 0; i--)        images[num_images++] = blocs__rand_box( 50,  20);
            for (int i = 50 + rand() % 50; i >= 0; i--)   images[num_images++] = blocs__rand_box( 20,  50);
            for (int i = 300 + rand() % 200; i >= 0; i--) images[num_images++] = blocs__rand_box( 10,  10);
            for (int i = 500 + rand() % 500; i >= 0; i--) images[num_images++] = blocs__rand_box(  5,   5);
        }

        if (log_verbose)
        {
            time_curr = blocs__get_time_ms();
            blocs__log(blocs__log_WHITE,
                " - Load Graphics ............. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }

        blocs__assert(num_images > 2, "not enough images (%d) to pack", num_images);
    }

    // Allocate pixel data buffer and copy textures into buffer
    {
        buffer_index = 0U;
        buffer = (uint8_t*)malloc(atlas_size * atlas_size * CHANNELS * sizeof(uint8_t));
        textures = (blocs__texture*)malloc((sizeof(int32_t) * 5 + sizeof(uint32_t)) * num_images);

        for (int i = 0; i < num_images; i++)
        {
            blocs__image image = images[i];
            blocs__assert(image.w <= atlas_size && image.h <= atlas_size, "pixel data (%dpx, %dpx) too large for atlas (%dpx)", image.w, image.h, atlas_size);
            blocs__add_texture(textures,
                i,
                buffer,
                &buffer_index,
                image,
                demo ? 0 : i
            );
        }
    }

    // Bin packing image rects
    {
        blocs__pack_atlas(textures, num_images, atlas_size, atlas_expand, atlas_border);
        
        if (log_verbose)
        {
            time_curr = blocs__get_time_ms();
            blocs__log(blocs__log_WHITE,
                " - Pack Graphics ............. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }

    // Generate atlas and blit textures data onto image
    {
        atlas_bmp = (blocs__image*)malloc(
            sizeof(int32_t) * 2 + sizeof(uint8_t) * atlas_size * atlas_size * CHANNELS
        );
        atlas_bmp->w = atlas_size;
        atlas_bmp->h = atlas_size;
        blocs__clear_pixels(atlas_bmp);
        blocs__generate_atlas(textures, num_images, buffer, atlas_bmp, atlas_expand);
        
        if (log_verbose)
        {
            time_curr = blocs__get_time_ms();
            blocs__log(blocs__log_WHITE,
                " - Generate Texture .......... %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }
    
    // Save atlas as png
    {
        char* png_output;
        png_output = (char*)malloc(strlen(output_dir) + strlen(output_name) + 6);
        sprintf(png_output, "%s/%s.png", output_dir, output_name);
        blocs__save_png(png_output, atlas_bmp);

        if (log_verbose)
        {
            time_curr = blocs__get_time_ms();
            blocs__log(blocs__log_WHITE,
                " - Save PNG .................. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }

    // Serialize atlas data
    {
        char* json_output;
        json_output = (char*)malloc(strlen(output_dir) + strlen(output_name) + 7);
        sprintf(json_output, "%s/%s.json", output_dir, output_name);
        blocs__save_json(json_output, atlas_bmp, textures, num_images, names);

        if (log_verbose)
        {
            time_curr = blocs__get_time_ms();
            blocs__log(blocs__log_WHITE,
                " - Save JSON ................. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }

    // Done!
    if (log_verbose)
    {
        blocs__log(blocs__log_WHITE,
            "Done ......................... %.2fms",
            time_curr - time_start
        );
        blocs__log(blocs__log_WHITE, "===================");
    }
    
    // Clean up
    {
        for (int i = 0; i < num_images; i++)
            if (!demo)
                blocs__unload_image(&images[i]);
        free(images);
        free(textures);
        free(buffer);
        free(atlas_bmp);
    }

    blocs__log(blocs__log_WHITE, "Saved to \"%s\"", output_dir);
    return 0;
}
