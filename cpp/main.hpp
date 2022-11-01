
#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define CHANNELS 4

#define PNG_EXT ".png"
#define JPG_EXT ".jpg"

namespace blocs__atlas
{
    ////////////////////////////////////
    //
    // debug methods for logging program
    // progress, benchmark, and assertions
    //

    enum class Log
    {
        INFO  = 6,
        GOOD  = 2,
        WARN  = 3,
        ERROR = 1,
        WHITE = 7,
    };

    inline void log(Log type, const char* message, ...)
    {
        char s[1024];
        va_list vl;
        va_start(vl, message);
        vsnprintf(s, sizeof(char) * 1024, message, vl);
        va_end(vl);
        printf("\x1B[3%dm%s\x1B[0m\n", type, s);
    }

    inline void log_assert(bool condition, const char* message, ...)
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

    inline double get_time_ms()
    {
        return std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now()
        ).time_since_epoch().count() * 0.001;
    }

    ////////////////////////////////////
    //
    // image loading, unloading, saving
    // and pixel data manipulation
    //

    struct rect;

    struct image
    {
        std::string name;
        int32_t     w;
        int32_t     h;
        uint8_t*    data;
        
        image();
        image(int32_t w, int32_t h);

        ~image(){}

        bool load(const std::string& path);
        void unload();
        void clear();
        void set_pixels(uint8_t* data, const rect& dst);
        void save_png(const std::string& output);
        std::size_t generate_hash();
    };

    ////////////////////////////////////
    //
    // texture atlas generation
    //

    struct rect
    {
        int32_t     x;
        int32_t     y;
        int32_t     w;
        int32_t     h;
    };

    struct texture
    {
        std::string name;
        rect        rect;
        uint32_t    buffer_index;
    };
    
    class atlas
    {
    public:
        int         m_size;
        int         m_expand;
        int         m_border;

        uint8_t*    m_buffer;
        uint32_t    m_buffer_index;

        image*      m_bitmap;

        std::vector<texture> m_textures;

    public:
        atlas() = delete;
        atlas(std::size_t n, int size, int expand, int border);

        ~atlas();

        void add_texture(const image& image);
        void pack();
        void save_json(const std::string& output);
        void save_binary(const std::string& output);
        image* generate_bitmap();
    };

    inline void write_binary(std::ofstream& stream, int16_t value)
    {
        stream.put(static_cast<uint8_t>(value & 0xff));
        stream.put(static_cast<uint8_t>((value >> 8) & 0xff));
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
     * @return std::string 
     */
    inline std::string file_path(const std::string& file)
    {
        return static_cast<std::filesystem::path>(file).remove_filename().string();
    }

    /**
     * @brief       Gets the file name without extension
     *              from a file path
     * 
     * @param file  File path
     * @return std::string 
     */
    inline std::string file_name(const std::string& file)
    {
        return static_cast<std::filesystem::path>(file).stem().string();
    }

    /**
     * @brief       Gets the file extension from a file path
     * 
     * @param file  File path
     * @return std::string 
     */
    inline std::string file_ext(const std::string& file)
    {
        return static_cast<std::filesystem::path>(file).extension().string();
    }

    /**
     * @brief       Checks whether an image's extension
     *              matches the supported image file types
     * 
     * @param ext   File extension
     * @return true 
     * @return false 
     */
    inline bool ext_is_img(const std::string& ext)
    {
        return ext == PNG_EXT || ext == JPG_EXT;
    }

    /**
     * @brief       Gets number of files inside of a directory (recursive)
     * 
     * @param path  Directory to search in
     * @return std::size_t 
     */
    inline std::size_t size_dir(const std::string& path)
    {
        return (std::size_t)std::distance(std::filesystem::recursive_directory_iterator{path},
            std::filesystem::recursive_directory_iterator{});
    }

    /**
     * @brief       Gets all file names inside of a directory (recursive)
     * 
     * @param path  Directory to search in
     * @param files List of file names
     */
    inline void enumerate_dir(const std::string& path, std::vector<std::string>& files)
    {
        log_assert(std::filesystem::is_directory(path), "passed in path is not a valid directory");
        for (auto p : std::filesystem::recursive_directory_iterator(path))
            files.emplace_back(p.path().string());
    }

    ////////////////////////////////////
    //
    // demo structs and funcs for
    // random HSLA box generation
    //

    namespace demo
    {
        struct hsl_color
        {
        private:    
            float hue(float p, float q, float t)
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

        public:
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
            
            hsl_color(float h, float s, float l, float a)
            {
                this->a = a * 255U;
                if (s == 0.f)
                    r = g = b = l * 255U;
                else
                {
                    float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
                    float p = 2.f * l - q;

                    r = hue(p, q, h + 1.f / 3.f) * 255U;
                    g = hue(p, q, h) * 255U;
                    b = hue(p, q, h - 1.f / 3.f) * 255U;
                }
            }
        };

        inline void fill_color(image& image, const hsl_color& color)
        {
            int w = image.w;
            int h = image.h;
            for (int i = 0; i < w * h * CHANNELS; i += CHANNELS)
            {
                image.data[i + 0] = color.r;
                image.data[i + 1] = color.g;
                image.data[i + 2] = color.b;
                image.data[i + 3] = color.a;
            }
        }

        inline image rand_box(int w, int h)
        {
            image bmp = image(w, h);
            bmp.name = "box";
            hsl_color color = hsl_color((float)rand() / RAND_MAX, 1.0f, 0.7f, 1.0f);
            fill_color(bmp, color);
            return bmp;
        }
    }
}
