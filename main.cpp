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

#include "main.hpp"

using namespace blocs__atlas;

////////////////////////////////////
//
// image loading, unloading, saving
// and pixel data manipulation
//

image::image() {}

/**
 * @brief       Creates an empty image of a size
 *              to have pixels added later
 * 
 * @param w     Image width
 * @param h     Image height
 */
image::image(int32_t w, int32_t h)
    : w(w), h(h)
{
    data = new uint8_t[w * h * CHANNELS];
}

/**
 * @brief       Reads RGBA pixel data from an image located
 *              at the provided file path
 * 
 * @param path  File path to read from
 * @return      true false 
 */
bool image::load(const std::string& path)
{
    int bpp;
    data = stbi_load(path.c_str(), &w, &h, &bpp, CHANNELS);
    return data != nullptr;
}

/**
 * @brief       Frees bitmaps data from memory
 */
void image::unload()
{
    log_assert(data != nullptr, "cannot dispose an unloaded image");
    stbi_image_free(data);
}

/**
 * @brief       Fills a bitmap's pixel data with transparent
 *              empty pixels (RGBA of 0U)
 */
void image::clear()
{
    data = new uint8_t[w * h * CHANNELS];
    memset(data, 0U, w * h * CHANNELS * sizeof(uint8_t));
}

/**
 * @brief       Blits pixel data onto a portion on a bitmap
 * 
 * @param pxls  New pixel data
 * @param dst   Destination rect on the image
 */
void image::set_pixels(uint8_t* pxls, const rect& dst)
{
    log_assert(dst.x + dst.w <= w && dst.y + dst.h <= h,
        "new pixels (%dpx, %dpx) cannot be larger than image (%dpx, %dpx)",
        dst.w, dst.y, w, h);

    for (int y = 0; y < dst.h; y++)
    {
        int from = y * dst.w;
        int to = dst.x + (dst.y + y) * w;
        memcpy(
            data + to * CHANNELS,
            pxls + from * CHANNELS,
            dst.w * CHANNELS * sizeof(uint8_t)
        );
    }
}

/**
 * @brief        Saves bitmap data as a png file
 * 
 * @param output Output directory
 */
void image::save_png(const std::string& output)
{
    log_assert(data != nullptr, "cannot save unloaded image");
    log_assert(w > 0 && h > 0, "image too small to save");
	
    // TODO: custom compression settings
    stbi_write_force_png_filter = 0;
    stbi_write_png_compression_level = 0;

    stbi_write_png(output.c_str(), w, h, CHANNELS, data, w * CHANNELS);
}

////////////////////////////////////
//
// texture atlas generation
//

/**
 * @brief               Creates a new atlas object
 * 
 * @param n             Number of expected textures to be added
 * @param size          Size of final atlas
 * @param expand        Amount of pixels to repeat on edges
 * @param border        Amount of empty space between bitmaps
 * @param unique        Whether to enforce unique bitmaps
 */
atlas::atlas(std::size_t n, int size, int expand, int border, bool unique)
    : m_size(size), m_expand(expand), m_border(border), m_unique(unique)
{
    m_buffer = new uint8_t[size * size * CHANNELS];
    m_buffer_index = 0U;
    m_textures.reserve(n);
}

atlas::~atlas()
{
    m_bitmap->unload();
    delete[] m_buffer;
}

/**
 * @brief               Adds a texture to the list of textures to be packed
 *                      and adds bitmap data to buffer
 * 
 * @param image         Image to be packed and bitmap data added to buffer
 */
void atlas::add_texture(const image& image)
{
    log_assert(image.data != nullptr, "could not read texture data");
    log_assert(image.w < m_size && image.h < m_size, "pixel data (%dpx, %dpx) too large for atlas (%dpx)",
        image.w, image.h, m_size);

    m_textures.push_back({
        { 0, 0, image.w, image.h },
        m_buffer_index
    });
    uint32_t buffer_length = image.w * image.h * CHANNELS;
    memcpy(m_buffer + m_buffer_index, image.data, buffer_length);
    m_buffer_index += buffer_length;
}

/**
 * @brief               Packs bitmap rects into smallest possible
 *                      configuration and updates texture positions
 */
void atlas::pack()
{
    int area  = 0;
    int max_w = 0;
    int max_h = 0;
    int padding = m_expand * 2 + m_border;
    for (int i = 0; i < m_textures.size(); i++)
    {
        rect rect = m_textures[i].rect;
        area += (rect.w + padding) * (rect.h + padding);
        max_w = std::max(max_w, rect.w + padding);
        max_h = std::max(max_h, rect.h + padding);
    }
    double suboptimal_coefficient = 0.85; // assumes sub-100% space utilization

    log_assert(max_w <= m_size && max_h <= m_size,
        "max size needed (%dpx, %dpx) larger than atlas size (%dpx)",
        max_w, max_h, m_size);
    log_assert(area <= m_size * m_size * suboptimal_coefficient,
        "total area needed (%dpx) cannot fit in atlas size (%.fpx, AKA %d x %d with %.2f%% space utilization)",
        area, m_size * m_size * suboptimal_coefficient, m_size, m_size, suboptimal_coefficient);

    std::vector<rect> spaces = { { 0, 0, m_size, m_size } };

    std::sort(m_textures.begin(), m_textures.end(), [](texture a, texture b)
    {
        return a.rect.h > b.rect.h;
    });

    for (auto& texture : m_textures)
    {
        auto& rect = texture.rect;
        log_assert(rect.w < m_size && rect.h < m_size,
            "texture larger (%dpx, %dpx) than maximum size (%dpx)",
            rect.w, rect.h, m_size);

        for (int i = spaces.size() - 1; i >= 0; i--)
        {
            auto space = spaces[i];

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
            rect.x = space.x + m_expand;
            rect.y = space.y + m_expand;

            if (w == space.w && h == space.h)
            {
                // remove space if perfect fit
                // |---------------|
                // |               |
                // |      box      |
                // |               |
                // |_______________|
                auto last = spaces.back();
                spaces.pop_back();
                
                if (i < spaces.size())
                    spaces[i] = last;
            }
            else if (h == space.h)
            {
                // space matches image height
                // move space right and cut off width
                // |-------|---------------|
                // |  box  | updated space |
                // |_______|_______________|
                space.x += w;
                space.w -= w;
                spaces[i] = space;
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
                space.y += h;
                space.h -= h;
                spaces[i] = space;
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
                spaces.push_back({
                    space.x + w,
                    space.y,
                    space.w - w,
                    h
                });
                space.y += h;
                space.h -= h;
                spaces[i] = space;
            }

            break;
        }
    }
}

/**
 * @brief               Generates atlas texture data based on packed
 *                      rects using the buffer of pixels
 */
image* atlas::generate_bitmap()
{
    m_bitmap = new image(m_size, m_size);
    m_bitmap->clear();

    for (auto texture : m_textures)
    {
        auto rect = texture.rect;
        if (m_expand > 0)
        {
            uint8_t* src_buffer = m_buffer + texture.buffer_index;
            uint8_t* dst_buffer = m_bitmap->data;
            for (int y = -m_expand * CHANNELS; y < (rect.h + m_expand) * CHANNELS; y += CHANNELS)
            {
                for (int x = -m_expand * CHANNELS; x < (rect.w + m_expand) * CHANNELS; x += CHANNELS)
                {
                    int src_x = x < 0 ?
                        0 : x > (rect.w - 1) * CHANNELS ?
                            (rect.w - 1) * CHANNELS : x;
                    int src_y = y < 0 ?
                        0 : y > (rect.h - 1) * CHANNELS ?
                            (rect.h - 1) * CHANNELS : y;

                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*m_size+0] = src_buffer[src_x+src_y*rect.w+0];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*m_size+1] = src_buffer[src_x+src_y*rect.w+1];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*m_size+2] = src_buffer[src_x+src_y*rect.w+2];
                    dst_buffer[rect.x*CHANNELS+x+(rect.y*CHANNELS+y)*m_size+3] = src_buffer[src_x+src_y*rect.w+3];
                }
            }
        }
        else
        {
            uint8_t r = (m_buffer + texture.buffer_index)[0];
            uint8_t g = (m_buffer + texture.buffer_index)[1];
            uint8_t b = (m_buffer + texture.buffer_index)[2];
            uint8_t a = (m_buffer + texture.buffer_index)[3];
            log(Log::WARN, "%d, %d, %d, %d", r, g, b, a);

            log(Log::WARN, "%d, %d, %d, %d", rect.x, rect.y, rect.w, rect.h);
            
            m_bitmap->set_pixels(m_buffer + texture.buffer_index, rect);
        }
    }

    return m_bitmap;
}

////////////////////////////////////

namespace
{
    std::string     input_dir;
    std::string     output_dir;

    bool            log_verbose;
    bool            is_demo;

    int32_t         atlas_size;
    int32_t         atlas_expand;
    int32_t         atlas_border;
    bool            atlas_unique;

    atlas*          packer;
    image*          atlas_bmp;

    std::vector<image> images;
}

int main(int argc, const char *argv[])
{
    std::ios_base::sync_with_stdio(false);

    log_assert(argc > 2, "expected \"pack [INPUT] [OPTS...]\"");

    // Set default output dir to "atlas.png" in relative path
    output_dir = "atlas.png";
    // Set default size to 4k
    atlas_size = 4096;

    // Parse command line arguments
    {
        input_dir = argv[1];

        // If first arg is demo, set demo defaults
        if (input_dir == "-d" || input_dir == "--demo")
        {
            is_demo = true; 
            output_dir = "demo.png";
            atlas_size = 960;
        }

        for (int i = 2; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "-o" || arg == "--output")
            {
                i++;
                log_assert(i < argc, "went out of bounds looking for output argument value");
                output_dir = argv[i];
            }
            else if (arg == "-s" || arg == "--size")
            {
                i++;
                log_assert(i < argc, "went out of bounds looking for size argument value");
                atlas_size = std::stoi(argv[i]);
            }
            else if (arg == "-v" || arg == "--verbose")
                log_verbose = true;
            else if (arg == "-u" || arg == "--unique")
                atlas_unique = true;
            else if (arg == "-e" || arg == "--expand")
            {
                i++;
                log_assert(i < argc, "went out of bounds looking for expand argument value");
                atlas_expand = std::stoi(argv[i]);
            }
            else if (arg == "-b" || arg == "--border")
            {
                i++;
                log_assert(i < argc, "went out of bounds looking for border argument value");
                atlas_border = std::stoi(argv[i]);
            }
            else
                log_assert(0, "unrecognized arg \"%s\"", arg.c_str());
        }
    }

    // Set start time for logging
    double time_start, time_prev, time_curr;
    time_start = time_prev = time_curr = get_time_ms();

    if (log_verbose)
    {
        log(Log::WHITE, "blocs___atlas v.0.0");
        log(Log::WHITE, "===================");
        log(Log::WHITE, "Begin Texture Atlas");
    }

    // Find images from directory in first argument
    {
        if (!is_demo)
        {
            std::vector<std::string> filenames;
            enumerate_dir(input_dir, filenames);

            if (log_verbose)
            {
                time_curr = get_time_ms();
                log(Log::WHITE,
                    " - Find Graphics ............. %.2fms",
                    time_curr - time_prev
                );
                time_prev = time_curr;
            }

            for (auto f : filenames)
            {
                std::string ext = file_ext(f);
                if (ext_is_img(ext))
                {
                    image bmp = {};
                    if (bmp.load(f))
                    {
                        images.emplace_back(bmp);
                        if (log_verbose)
                            log(Log::GOOD, "   ✓ \"%s\"", f.c_str());
                    }
                    else
                    {
                        if (log_verbose)
                            log(Log::ERROR, "   x \"%s\"", f.c_str());
                    }
                }
            }
        }
        else
        {
            srand(time(NULL));
            if (((float)rand() / RAND_MAX) > 0.5)         images.emplace_back(demo::rand_box(400,  80));
            if (((float)rand() / RAND_MAX) > 0.5)         images.emplace_back(demo::rand_box( 80, 400));
            if (((float)rand() / RAND_MAX) > 0.5)         images.emplace_back(demo::rand_box(250, 250));
            if (((float)rand() / RAND_MAX) > 0.5)         images.emplace_back(demo::rand_box(100, 250));
            if (((float)rand() / RAND_MAX) > 0.5)         images.emplace_back(demo::rand_box(250, 100));
            for (int i = rand() % 20; i >= 0; i--)        images.emplace_back(demo::rand_box(100, 100));
            for (int i = rand() % 10; i >= 0; i--)        images.emplace_back(demo::rand_box( 60,  60));
            for (int i = rand() % 30; i >= 0; i--)        images.emplace_back(demo::rand_box( 50,  50));
            for (int i = rand() % 40; i >= 0; i--)        images.emplace_back(demo::rand_box( 50,  20));
            for (int i = 50 + rand() % 50; i >= 0; i--)   images.emplace_back(demo::rand_box( 20,  50));
            for (int i = 300 + rand() % 200; i >= 0; i--) images.emplace_back(demo::rand_box( 10,  10));
            for (int i = 500 + rand() % 500; i >= 0; i--) images.emplace_back(demo::rand_box(  5,   5));
        }
        
        if (log_verbose)
        {
            time_curr = get_time_ms();
            log(Log::WHITE,
                " - Load Graphics ............. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }

        log_assert(images.size() > 2, "not enough images (%d) to pack", images.size());
    }
    
    // Allocate pixel data buffer and copy textures into buffer
    {
        packer = new atlas(images.size(), atlas_size, atlas_expand, atlas_border, atlas_unique);
        for (auto& image : images)
            packer->add_texture(image);
    }
    
    // Bin packing image rects
    {
        packer->pack();      

        if (log_verbose)
        {
            time_curr = get_time_ms();
            log(Log::WHITE,
                " - Pack Grahpics ............. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }

    // Generate atlas and blit textures data onto image
    {
        atlas_bmp = packer->generate_bitmap();       

        if (log_verbose)
        {
            time_curr = get_time_ms();
            log(Log::WHITE,
                " - Generate Texture .......... %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }
    
    // Save atlas as png
    {
        atlas_bmp->save_png(output_dir);

        if (log_verbose)
        {
            time_curr = get_time_ms();
            log(Log::WHITE,
                " - Save PNG .................. %.2fms",
                time_curr - time_prev
            );
            time_prev = time_curr;
        }
    }

    // Done!
    if (log_verbose)
    {
        log(Log::WHITE,
            "Done ......................... %.2fms",
            time_curr - time_start
        );
        log(Log::WHITE, "===================");
    }
    
    // Clean up
    {
        for (int i = 0; i < images.size(); i++)
        {
            if (!is_demo)
            {
                auto* image = &images.back();
                image->unload();
                images.pop_back();
            }
        }
        
        delete packer;
    }

    log(Log::WHITE, "Saved to %s", output_dir.c_str());

    return 0;
}
