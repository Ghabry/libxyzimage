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
	int zlib_error;
};

static void xyzpriv_set_error(xyzimage_error_t* error, xyzimage_error_t which) {
	if (error != NULL) {
		*error = which;
	}
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

	if (comp_size > len_out) {
		free(buffer_out_tmp);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_BUFFER_TOO_SMALL);
		return 0;
	}

	memcpy(buffer_out, buffer_out_tmp, comp_size);

	free(buffer_out_tmp);

	return comp_size;
}

static size_t xyzpriv_fwrite_func(void* userdata, void* buffer, size_t amount, xyzimage_error_t* error) {
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

	img->zlib_error = 0;

	return img;
}

XYZImage* xyzimage_alloc(uint16_t width, uint16_t height, enum XYZImage_Format format, xyzimage_error_t* error) {
	unsigned int multiplier = 0;

	switch (format) {
		case XYZIMAGE_FORMAT_DEFAULT:
			multiplier = 1;
			break;
		case XYZIMAGE_FORMAT_RGBX:
			multiplier = 4;
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

	image->data_len = width * height * multiplier;
	void* data = malloc(image->data_len);

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
	if (file == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_POINTER_BAD);
		return NULL;
	}

	return xyzimage_open(file, xyzpriv_fread_func, error);
}

XYZImage* xyzimage_memopen(const void* buffer, size_t len, xyzimage_error_t* error) {
	xyzpriv_set_error(error, XYZIMAGE_ERROR_NOT_IMPLEMENTED);
	return NULL; // TODO Not implemented
}

XYZImage* xyzimage_open(void* userdata, xyzimage_read_func_t read_func, xyzimage_error_t* error) {
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

	uint16_t height;
	res = read_func(userdata, &height, 2, error);

	if (res != 2 || (error && *error != 0)) {
		return NULL;
	}

	// Allocate XYZImage structure
	XYZImage* image = xyzimage_alloc(width, height, XYZIMAGE_FORMAT_DEFAULT, error);

	if (!image || (error && *error != 0)) {
		return NULL;
	}

	// Allocate intermediate buffers
	size_t xyz_size = (size_t)(XYZIMAGE_PALETTE_SIZE + image->width * image->height);

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

	if (e != XYZIMAGE_ERROR_IO_READ_END_OF_FILE) {
		xyzimage_free(image);
		free(compressed_xyz);
		free(decompressed_xyz);

		if (res == xyz_size) {
			// Not EOF and compressed image is larger than uncompressed
			xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_BAD_IMAGE);
			return NULL;
		}

		xyzpriv_set_error(error, e);
		return NULL;
	}

	image->data_len_compressed = res;

	// Decompress the XYZ image
	uLongf xyz_size_out = (int)xyz_size;
	image->zlib_error = uncompress(
			decompressed_xyz, &xyz_size_out, compressed_xyz, (uLong)xyz_size);

	free(compressed_xyz);

	if (image->zlib_error != Z_OK) {
		xyzimage_free(image);
		free(decompressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_ZLIB);
		return NULL;
	}

	if (xyz_size_out != xyz_size) {
		xyzimage_free(image);
		free(decompressed_xyz);
		xyzpriv_set_error(error, XYZIMAGE_ERROR_IO_READ_BAD_IMAGE);
		return NULL;
	}

	// Fill the data structures
	for (int i = 0; i < XYZIMAGE_PALETTE_ENTRIES; ++i) {
		image->palette.entry[i].red = decompressed_xyz[i * 3];
		image->palette.entry[i].green = decompressed_xyz[i * 3 + 1];
		image->palette.entry[i].blue = decompressed_xyz[i * 3 + 2];
	}

	size_t img_buffer_len;
	void* img_buffer = xyzimage_get_image(image, &img_buffer_len);
	if (img_buffer == NULL || (error && *error != 0)) {
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

void* xyzimage_get_image(XYZImage* image, size_t* len) {
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

	return image->data_len + 8;
}

size_t xyzimage_get_compressed_filesize(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return image->data_len_compressed + 8;
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

int xyzimage_memwrite(XYZImage* image, void* buffer, size_t len, xyzimage_error_t* error) {
	// TODO Not implemented
	xyzpriv_set_error(error, XYZIMAGE_ERROR_NOT_IMPLEMENTED);

	return 0;
}

int xyzimage_write(XYZImage* image, void* userdata, xyzimage_write_func_t write_func, xyzimage_error_t* error) {
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
		case XYZIMAGE_FORMAT_RGBX:
		default:
			xyzpriv_set_error(error, XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED);
			return 0;
	}

	// Allocate intermediate buffer
	size_t xyz_size = (size_t)(XYZIMAGE_PALETTE_SIZE + image->width * image->height);

	Bytef* decompressed_xyz = malloc(xyz_size);

	if (decompressed_xyz == NULL) {
		xyzpriv_set_error(error, XYZIMAGE_ERROR_OUT_OF_MEMORY);
		return 0;
	}

	// Fill the buffer
	for (int i = 0; i < XYZIMAGE_PALETTE_ENTRIES; ++i) {
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

	size_t compressed_size = image->compress_func(decompressed_xyz, xyz_size, compressed_xyz, xyz_size, error);

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
	res = write_func(userdata, &image->width, 2, error);

	if (res != 2 || (error && *error != 0)) {
		free(compressed_xyz);
		return 0;
	}

	res = write_func(userdata, &image->height, 2, error);

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
			return "Success";
		case XYZIMAGE_ERROR_IO_READ_GENERIC:
			return "Reading the X";
		case XYZIMAGE_ERROR_IO_WRITE:
			return "A write operation failed";
		default:
			return "Unknown error";
	}
}

int xyzimage_get_zlib_error(const XYZImage* image) {
	if (!xyzimage_is_valid(image)) {
		return 0;
	}

	return image->zlib_error;
}
