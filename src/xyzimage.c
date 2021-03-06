/*
 * This file is part of libxyzimage. Copyright (c) 2018 liblcf authors.
 * https://github.com/EasyRPG/libxyzimage - https://easyrpg.org
 *
 * libxyzimage is Free/Libre Open Source Software, released under the
 * MIT License. For the full copyright and license information, please view
 * the COPYING file that was distributed with this source code.
 */

#include <memory.h>
#include <stdlib.h>
#include <zlib.h>

#include "xyzimage.h"

// Increment when the data format of struct XYZImage changes
#define XYZPRIV_CURRENT_STRUCT_VERSION 1

#define XYZPRIV_HEADER_SIZE 8u

struct XYZImage {
	uint8_t header[4];
	uint32_t version;
	uint16_t width;
	uint16_t height;
	enum XYZImage_Format format;
	XYZImage_Palette palette;
	size_t data_len;
	size_t data_len_compressed;
	void* data;
	xyzimage_compress_func_t compress_func;
};

static void xyzpriv_set_error(xyzimage_error_t* error, xyzimage_error_t which) {
	if (error != NULL) {
		*error = which;
	}
}

static void xyzpriv_swap_when_big(uint16_t* us) {
	union {
		uint32_t i;
		char c[4];
	} d = {0x01020304};

	int is_big = d.c[0] == 1;

	if (!is_big) {
		return;
	}

	*us = (*us >> 8) |
		  (*us << 8);
}

static size_t xyzpriv_fread_func(void* userdata, void* buffer, size_t amount, xyzimage_error_t* error) {
	if (userdata == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return 0;
	}

	size_t res = fread(buffer, 1, amount, (FILE*)userdata);
	if (res != amount) {
		if (feof((FILE*)userdata)) {
			xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_END_OF_FILE);
		} else {
			xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_GENERIC);
		}
	}

	return res;
}

static size_t xyzpriv_compress_func(const void* buffer_in, size_t len_in, void* buffer_out, size_t len_out, xyzimage_error_t* error) {
	// buffer_in, buffer_out, len_in and len_out verified by caller
	uLong comp_size = compressBound(len_in);

	if (comp_size > len_out) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_BUFFER_TOO_SMALL);
		return 0;
	}

	void* buffer_out_tmp = malloc(comp_size);

	if (buffer_out_tmp == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return 0;
	}

	int error_code = compress2(
			buffer_out_tmp, &comp_size, buffer_in, len_in, Z_BEST_COMPRESSION);

	if (error_code != Z_OK) {
		free(buffer_out_tmp);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_COMPRESS);
		return 0;
	}

	memcpy(buffer_out, buffer_out_tmp, comp_size);

	free(buffer_out_tmp);

	return comp_size;
}

static size_t xyzpriv_fwrite_func(void* userdata, const void* buffer, size_t amount, xyzimage_error_t* error) {
	if (userdata == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return 0;
	}

	size_t res = fwrite(buffer, 1, amount, (FILE*)userdata);
	if (res != amount) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_WRITE);
	}

	return res;
}

static XYZImage* xyzpriv_alloc() {
	XYZImage* img = (XYZImage*)malloc(sizeof(struct XYZImage));

	// Magic bytes of the struct, not of the XYZ image
	img->header[0] = 'L';
	img->header[1] = 'X';
	img->header[2] = 'Y';
	img->header[3] = 'Z';

	img->version = XYZPRIV_CURRENT_STRUCT_VERSION;

	img->format = XYZIMAGE_FORMAT_DEFAULT;

	memset(&img->palette, '\0', sizeof(img->palette));

	img->data = NULL;
	img->data_len = 0;
	img->data_len_compressed = 0;

	img->compress_func = xyzpriv_compress_func;

	return img;
}

XYZImage* xyzimage_alloc(uint16_t width, uint16_t height, enum XYZImage_Format format, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	unsigned int multiplier = 0;

	switch (format) {
		case XYZIMAGE_FORMAT_DEFAULT:
			multiplier = 1;
			break;
		default:
			xyzpriv_set_error(error, XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED);
			return NULL;
	}

	XYZImage* image = xyzpriv_alloc();

	if (image == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	image->width = width;
	image->height = height;

	image->data_len = (uint32_t)width * height * multiplier;
	void* data = calloc(1, image->data_len);

	if (data == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		image->data_len = 0;
		return NULL;
	}

	image->data = data;

	return image;
}

int xyzimage_free(XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	// Corrupt header to prevent use after free
	image->header[0] = '!';

	free(image->data);
	image->data = NULL;
	image->data_len = 0;
	free(image);

	return 1;
}

XYZImage* xyzimage_fopen(FILE* file, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	if (file == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return NULL;
	}

	return xyzimage_open(file, xyzpriv_fread_func, error);
}

XYZImage* xyzimage_open(void* userdata, xyzimage_read_func_t read_func, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	if (read_func == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return NULL;
	}

	// Check for XYZ1 header
	char xyz_header[4];
	size_t res = read_func(userdata, xyz_header, 4, error);

	if (res != 4 || (error && *error != 0)) {
		return NULL;
	}

	if (memcmp(xyz_header, "XYZ1", 4) != 0) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_BAD_HEADER);
		return NULL;
	}

	// Read width and height
	uint16_t width;
	res = read_func(userdata, &width, 2, error);

	if (res != 2 || (error && *error != 0)) {
		return NULL;
	}

	xyzpriv_swap_when_big(&width);

	uint16_t height;
	res = read_func(userdata, &height, 2, error);

	if (res != 2 || (error && *error != 0)) {
		return NULL;
	}

	xyzpriv_swap_when_big(&height);

	// Allocate XYZImage structure
	XYZImage* image = xyzimage_alloc(width, height, XYZIMAGE_FORMAT_DEFAULT, error);

	if (!image || (error && *error != 0)) {
		return NULL;
	}

	// Allocate intermediate buffers
	size_t xyz_size = (size_t)(XYZIMAGE_PALETTE_SIZE + (uint32_t)image->width * image->height);

	Bytef* compressed_xyz = malloc(xyz_size);

	if (compressed_xyz == NULL) {
		xyzimage_free(image);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	Bytef* decompressed_xyz = malloc(xyz_size);

	if (decompressed_xyz == NULL) {
		xyzimage_free(image);
		free(compressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	// Read the XYZ image
	// Special error handling for EOF check
	xyzimage_error_t e = XYZIMAGE_ERROR_OK;
	res = read_func(userdata, compressed_xyz, xyz_size, &e);

	if (e == XYZIMAGE_ERROR_OK) {
		// Compression ratio is worse than 1, double the buffer size and try again
		Bytef* compressed_xyz_new = realloc(compressed_xyz, xyz_size * 2);

		if (compressed_xyz_new == NULL) {
			xyzimage_free(image);
			free(compressed_xyz);
			free(decompressed_xyz);
			xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
			return NULL;
		}

		compressed_xyz = compressed_xyz_new;

		res += read_func(userdata, compressed_xyz + xyz_size, xyz_size, &e);
	}

	if (e != XYZIMAGE_ERROR_IO_READ_END_OF_FILE) {
		xyzimage_free(image);
		free(compressed_xyz);
		free(decompressed_xyz);

		if (e == XYZIMAGE_ERROR_OK) {
			// Not EOF and compressed image is larger than twice the uncompressed
			xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_IMAGE_TOO_BIG);
			return NULL;
		}

		xyzpriv_set_error(error, e);
		return NULL;
	}

	image->data_len_compressed = res;

	// Decompress the XYZ image
	uLongf xyz_size_out = (uLongf)xyz_size;
	int zlib_error = uncompress(
			decompressed_xyz, &xyz_size_out, compressed_xyz, (uLong)res);

	free(compressed_xyz);

	if (zlib_error != Z_OK) {
		xyzimage_free(image);
		free(decompressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_ZLIB);
		return NULL;
	}

	if (xyz_size_out != xyz_size) {
		xyzimage_free(image);
		free(decompressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_IMAGE_TOO_SMALL);
		return NULL;
	}

	// Fill the data structures
	int i;
	for (i = 0; i < XYZIMAGE_PALETTE_ENTRIES; ++i) {
		image->palette.entry[i].red = decompressed_xyz[i * 3];
		image->palette.entry[i].green = decompressed_xyz[i * 3 + 1];
		image->palette.entry[i].blue = decompressed_xyz[i * 3 + 2];
	}

	size_t img_buffer_len;
	void* img_buffer = xyzimage_get_buffer(image, &img_buffer_len);
	if (img_buffer == NULL) {
		xyzimage_free(image);
		free(decompressed_xyz);
		return NULL;
	}

	memcpy(img_buffer, decompressed_xyz + XYZIMAGE_PALETTE_SIZE, img_buffer_len);

	free(decompressed_xyz);

	return image;
}

uint16_t xyzimage_get_width(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return image->width;
}

uint16_t xyzimage_get_height(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return image->height;
}

XYZImage_Palette* xyzimage_get_palette(XYZImage* image, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	if (!xyzimage_is_valid(image)) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_XYZIMAGE_INVALID);
		return NULL;
	}

	if (image->format != XYZIMAGE_FORMAT_DEFAULT) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IMAGE_NOT_INDEXED);
		return NULL;
	}

	return &image->palette;
}

enum XYZImage_Format xyzimage_get_format(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return XYZIMAGE_FORMAT_NONE;
	}

	return image->format;
}

void* xyzimage_get_buffer(XYZImage* image, size_t* len) {
	if (!xyzimage_is_valid(image)) {
		return NULL;
	}

	if (len) {
		*len = image->data_len;
	}

	return image->data;
}

size_t xyzimage_get_filesize(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return (uint32_t)image->width * image->height + XYZPRIV_HEADER_SIZE + XYZIMAGE_PALETTE_SIZE;
}

size_t xyzimage_get_compressed_filesize(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return image->data_len_compressed + XYZPRIV_HEADER_SIZE;
}

void xyzimage_set_compress_func(XYZImage* image, xyzimage_compress_func_t compress_func) {
	if (!xyzimage_is_valid(image)) {
		return;
	}

	if (compress_func == NULL) {
		image->compress_func = xyzpriv_compress_func;
	} else {
		image->compress_func = compress_func;
	}
}

int xyzimage_fwrite(XYZImage* image, FILE* file, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	if (!xyzimage_is_valid(image)) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_XYZIMAGE_INVALID);
		return 0;
	}

	if (file == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return 0;
	}

	return xyzimage_write(image, file, xyzpriv_fwrite_func, error);
}

int xyzimage_write(XYZImage* image, void* userdata, xyzimage_write_func_t write_func, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_OK);

	if (!xyzimage_is_valid(image)) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_XYZIMAGE_INVALID);
		return 0;
	}

	if (!write_func) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return 0;
	}

	switch (image->format) {
		case XYZIMAGE_FORMAT_DEFAULT:
			break;
		default:
			xyzpriv_set_error(error, XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED);
			return 0;
	}

	// Allocate intermediate buffer
	size_t xyz_size = (size_t)(XYZIMAGE_PALETTE_SIZE + (uint32_t)image->width * image->height);

	Bytef* decompressed_xyz = malloc(xyz_size);

	if (decompressed_xyz == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return 0;
	}

	// Fill the buffer
	int i;
	for (i = 0; i < XYZIMAGE_PALETTE_ENTRIES; ++i) {
		decompressed_xyz[i * 3] = image->palette.entry[i].red;
		decompressed_xyz[i * 3 + 1] = image->palette.entry[i].green;
		decompressed_xyz[i * 3 + 2] = image->palette.entry[i].blue;
	}

	memcpy(decompressed_xyz + XYZIMAGE_PALETTE_SIZE, image->data, image->data_len);

	// Compress using the compression function
	void* compressed_xyz = malloc(xyz_size);

	if (compressed_xyz == NULL) {
		free(decompressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return 0;
	}

	// Special error handling for buffer too small check (compressed > decompressed)
	xyzimage_error_t e = XYZIMAGE_ERROR_OK;
	size_t compressed_size = image->compress_func(decompressed_xyz, xyz_size, compressed_xyz, xyz_size, &e);

	if (e == XYZIMAGE_ERROR_BUFFER_TOO_SMALL) {
		// Compression ratio is worse than 1, double the buffer size and try again
		Bytef* compressed_xyz_new = realloc(compressed_xyz, xyz_size * 2);

		if (compressed_xyz_new == NULL) {
			xyzimage_free(image);
			free(compressed_xyz);
			free(decompressed_xyz);
			xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
			return 0;
		}

		compressed_xyz = compressed_xyz_new;

		compressed_size = image->compress_func(decompressed_xyz, xyz_size, compressed_xyz, xyz_size * 2, error);
	} else {
		xyzpriv_set_error(error, e);
	}

	free(decompressed_xyz);

	if (compressed_size == 0 || (error && *error)) {
		free(compressed_xyz);
		return 0;
	}

	// Update compressed size information (for statistical purposes)
	image->data_len_compressed = compressed_size;

	// Write the header
	char xyz_header[4] = {'X', 'Y', 'Z', '1'};

	size_t res = write_func(userdata, xyz_header, 4, error);

	if (res != 4 || (error && *error != 0)) {
		free(compressed_xyz);
		return 0;
	}

	// Write width and height
	uint16_t width = image->width;
	xyzpriv_swap_when_big(&width);

	res = write_func(userdata, &width, 2, error);

	if (res != 2 || (error && *error != 0)) {
		free(compressed_xyz);
		return 0;
	}

	uint16_t height = image->height;
	xyzpriv_swap_when_big(&height);

	res = write_func(userdata, &height, 2, error);

	if (res != 2 || (error && *error != 0)) {
		free(compressed_xyz);
		return 0;
	}

	// Write compressed image
	res = write_func(userdata, compressed_xyz, compressed_size, error);

	free(compressed_xyz);

	if (res != compressed_size || (error && *error != 0)) {
		return 0;
	}

	return 1;
}

int xyzimage_is_valid(const XYZImage* image) {
	if (!image) {
		return 0;
	}

	if (memcmp(image->header, "LXYZ", 4) != 0) {
		return 0;
	}

	if (image->version != 1) {
		return 0;
	}

	return 1;
}

const char* xyzimage_get_error_message(xyzimage_error_t error) {
	switch (error) {
		case XYZIMAGE_ERROR_OK:
			return "Success.";
		case XYZIMAGE_ERROR_IO_READ_GENERIC:
			return "A read error occurred.";
		case XYZIMAGE_ERROR_IO_READ_BAD_HEADER:
			return "The file does not have a XYZ1 magic.";
		case XYZIMAGE_ERROR_IO_READ_IMAGE_TOO_BIG:
			return "The compressed image exceeds the size of 256 * 3 + width * height by a factor of 2 or more.";
		case XYZIMAGE_ERROR_IO_READ_IMAGE_TOO_SMALL:
			return "The image is truncated (size < 256 * 3 + width * height after decompression).";
		case XYZIMAGE_ERROR_IO_READ_END_OF_FILE:
			return "Internal error code to signal that the whole XYZ image was read.";
		case XYZIMAGE_ERROR_IO_WRITE:
			return "Saving the picture failed due to a write error.";
		case XYZIMAGE_ERROR_IO_COMPRESS:
			return "The compression step during saving failed.";
		case XYZIMAGE_ERROR_XYZIMAGE_INVALID:
			return "The passed XYZImage struct is invalid.";
		case XYZIMAGE_ERROR_BUFFER_TOO_SMALL:
			return "The passed buffer is not large enough.";
		case XYZIMAGE_ERROR_IMAGE_TOO_MANY_COLORS:
			return "The image is not indexed and has more than 256 colors.";
		case XYZIMAGE_ERROR_IMAGE_ALPHA_CHANNEL:
			return "The image has alpha channel values different from 0 or 255.";
		case XYZIMAGE_ERROR_IMAGE_NOT_INDEXED:
			return "The palette can only be accessed for color formats that are indexed.";
		case XYZIMAGE_ERROR_POINTER_BAD:
			return "At least one mandatory passed pointer is a NULL pointer.";
		case XYZIMAGE_ERROR_OUT_OF_MEMORY:
			return "A memory allocation failed.";
		case XYZIMAGE_ERROR_ZLIB:
			return "zlib was unable to decompress the image.";
		case XYZIMAGE_ERROR_NOT_IMPLEMENTED:
			return "The used API call is not supported by this library version.";
		case XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED:
			return "The requested XYZImage_Format is not supported by this library version.";
		default:
			return "Unknown error.";
	}
}
