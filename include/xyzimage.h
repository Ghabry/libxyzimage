/*
 * This file is part of libxyzimage. Copyright (c) 2018 liblcf authors.
 * https://github.com/EasyRPG/libxyzimage - https://easyrpg.org
 *
 * libxyzimage is Free/Libre Open Source Software, released under the
 * MIT License. For the full copyright and license information, please view
 * the COPYING file that was distributed with this source code.
 */

#ifndef LIBXYZIMAGE_XYZIMAGE_H
#define LIBXYZIMAGE_XYZIMAGE_H

#include <stdio.h>
#include <stdint.h>

typedef struct XYZImage XYZImage;

#define XYZIMAGE_CURRENT_VERSION 1

#define XYZIMAGE_PALETTE_ENTRIES 256
#define XYZIMAGE_PALETTE_SIZE (XYZIMAGE_PALETTE_ENTRIES * 3)

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} XYZImage_PaletteEntry;

typedef struct {
	XYZImage_PaletteEntry entry[XYZIMAGE_PALETTE_ENTRIES];
} XYZImage_Palette;

enum XYZImage_Error {
	XYZIMAGE_ERROR_OK = 0,
	XYZIMAGE_ERROR_IO_READ_GENERIC,
	XYZIMAGE_ERROR_IO_READ_BAD_HEADER,
	XYZIMAGE_ERROR_IO_READ_BAD_IMAGE,
	XYZIMAGE_ERROR_IO_READ_END_OF_FILE,
	XYZIMAGE_ERROR_IO_WRITE,
	XYZIMAGE_ERROR_IO_COMPRESS,
	XYZIMAGE_ERROR_XYZIMAGE_INVALID,
	XYZIMAGE_ERROR_IMAGE_TOO_MANY_COLORS,
	XYZIMAGE_ERROR_IMAGE_ALPHA_CHANNEL,
	XYZIMAGE_ERROR_IMAGE_NOT_INDEXED,
	XYZIMAGE_ERROR_POINTER_BAD,
	XYZIMAGE_ERROR_OUT_OF_MEMORY,
	XYZIMAGE_ERROR_ZLIB,
	XYZIMAGE_ERROR_NOT_IMPLEMENTED,
	XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED
};

enum XYZImage_Format {
	XYZIMAGE_FORMAT_NONE = 0,
	XYZIMAGE_FORMAT_DEFAULT,
	XYZIMAGE_FORMAT_RGBX
};

typedef enum XYZImage_Error xyzimage_error_t;

typedef size_t (*xyzimage_read_func_t)(void* userdata, void* buffer, size_t amount, xyzimage_error_t* error);
typedef size_t (*xyzimage_compress_func_t)(void* buffer_in, size_t size_in, void* buffer_out, size_t size_out, xyzimage_error_t* error);
typedef size_t (*xyzimage_write_func_t)(void* userdata, void* buffer, size_t amount, xyzimage_error_t* error);

XYZImage* xyzimage_alloc(uint16_t width, uint16_t height, enum XYZImage_Format format, xyzimage_error_t* error);
int xyzimage_free(XYZImage* image);

XYZImage* xyzimage_fopen(FILE* file, xyzimage_error_t* error);
XYZImage* xyzimage_memopen(void* buffer, size_t len, xyzimage_error_t* error);
XYZImage* xyzimage_open(void* userdata, xyzimage_read_func_t read_func, xyzimage_error_t* error);

uint16_t xyzimage_get_width(XYZImage* image);
uint16_t xyzimage_get_height(XYZImage* image);
XYZImage_Palette* xyzimage_get_palette(XYZImage* image, xyzimage_error_t* error);
enum XYZImage_Format xyzimage_get_format(XYZImage* image);
void* xyzimage_get_image(XYZImage* image, size_t* len);

void xyzimage_set_compress_func(XYZImage* image, xyzimage_compress_func_t compress_func);

int xyzimage_fwrite(XYZImage* image, FILE* file, xyzimage_error_t* error);
int xyzimage_memwrite(XYZImage* image, void* buffer, size_t len, xyzimage_error_t* error);
int xyzimage_write(XYZImage* image, void* userdata, xyzimage_write_func_t write_func, xyzimage_error_t* error);

int xyzimage_is_valid(XYZImage* image);

const char* xyzimage_get_error_message(xyzimage_error_t error);
int xyzimage_get_zlib_error(XYZImage* image);

#endif // LIBXYZIMAGE_XYZIMAGE_H
