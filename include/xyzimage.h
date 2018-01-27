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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XYZImage XYZImage;

/** Number of palette entries (colors) in the XYZ color palette */
#define XYZIMAGE_PALETTE_ENTRIES 256
/** Size of the whole palette in bytes */
#define XYZIMAGE_PALETTE_SIZE (XYZIMAGE_PALETTE_ENTRIES * 3)

/**
 * Contains the color information of one palette entry
 */
typedef struct {
	/** Red color component */
	uint8_t red;
	/** Green color component */
	uint8_t green;
	/** Blue color component */
	uint8_t blue;
} XYZImage_PaletteEntry;

/**
 * Color palette of the XYZ format consisting of 256 palette entries
 */
typedef struct {
	XYZImage_PaletteEntry entry[XYZIMAGE_PALETTE_ENTRIES];
} XYZImage_Palette;

/**
 * Possible error codes returned by API functions through the xyzimage_error_t variable
 */
enum XYZImage_Error {
	/** No error occurred */
	XYZIMAGE_ERROR_OK = 0,
	/** An unspecified read error occurred */
	XYZIMAGE_ERROR_IO_READ_GENERIC,
	/** The image file does not have a XYZ1 magic */
	XYZIMAGE_ERROR_IO_READ_BAD_HEADER,
	/** After decompression the image has a size != 256 * 3 + width * height */
	XYZIMAGE_ERROR_IO_READ_BAD_IMAGE,
	/** Internal error code to signal that the whole XYZ image was read */
	XYZIMAGE_ERROR_IO_READ_END_OF_FILE,
	/** Saving the picture failed due to a write error */
	XYZIMAGE_ERROR_IO_WRITE,
	/** The compression step during saving failed */
	XYZIMAGE_ERROR_IO_COMPRESS,
	/** The passed XYZImage struct is invalid */
	XYZIMAGE_ERROR_XYZIMAGE_INVALID,
	/** The passed buffer is not large enough */
	XYZIMAGE_ERROR_BUFFER_TOO_SMALL,
	/** The image is not indexed and has more than 256 colors */
	XYZIMAGE_ERROR_IMAGE_TOO_MANY_COLORS,
	/** The image has alpha channel values different from 0 or 255 */
	XYZIMAGE_ERROR_IMAGE_ALPHA_CHANNEL,
	/** The palette can only be accessed for color formats that are indexed */
	XYZIMAGE_ERROR_IMAGE_NOT_INDEXED,
	/** At least one mandatory passed pointer is a NULL pointer */
	XYZIMAGE_ERROR_POINTER_BAD,
	/** A memory allocation failed */
	XYZIMAGE_ERROR_OUT_OF_MEMORY,
	/** zlib was unable to decompress the image */
	XYZIMAGE_ERROR_ZLIB,
	/** The used API call is not supported by this library version */
	XYZIMAGE_ERROR_NOT_IMPLEMENTED,
	/** The requested XYZImage_Format is not supported by this library version */
	XYZIMAGE_ERROR_FORMAT_NOT_SUPPORTED
};

/**
 * The color format of the image buffer.
 * During writing all formats are internally converted to the default format because
 * that is the only format supported by XYZ.
 * Currently no other formats are supported.
 */
enum XYZImage_Format {
	/** No format specified */
	XYZIMAGE_FORMAT_NONE = 0,
	/** Default XYZ format: 1 byte per pixel referencing 256 colors in the palette */
	XYZIMAGE_FORMAT_DEFAULT
};

typedef enum XYZImage_Error xyzimage_error_t;

/**
 * Prototype of the custom read function passed to xyzimage_open.
 *
 * @param userdata userdata passed to xyzimage_open
 * @param buffer Output buffer
 * @param amount How many bytes to read into the buffer
 * @param error When non-null receives the error code when an error occurred
 * @return How many bytes were read into the buffer. When different to amount an error code should be set.
 *  When the end of userdata is reached the error code must be set to XYZIMAGE_ERROR_IO_READ_END_OF_FILE.
 */
typedef size_t (*xyzimage_read_func_t)(void* userdata, void* buffer, size_t amount, xyzimage_error_t* error);

/**
 * Prototype of the custom compress function used by the write functions.
 * The output buffer must receive bytes compressed by the DEFLATE algorithm, otherwise the image will be corrupted.
 *
 * @param buffer_in Uncompressed XYZ image input buffer
 * @param size_in Size of the input buffer in bytes
 * @param buffer_out Compressed XYZ image output buffer
 * @param size_out Size of the output buffer in bytes
 * @param error When non-null receives the error code when an error occurred
 * @return How many bytes were written into the output buffer. In error case 0 must be returned and an error code set.
 */
typedef size_t (*xyzimage_compress_func_t)(
		const void* buffer_in, size_t size_in, void* buffer_out, size_t size_out, xyzimage_error_t* error);

/**
 * Prototype of the custom read function passed to xyzimage_open.
 *
 * @param userdata userdata passed to xyzimage_write
 * @param buffer Input buffer
 * @param amount How many bytes to write from the buffer
 * @param error When non-null receives the error code when an error occurred
 * @return How many bytes were written from the buffer. When different to amount an error code should be set.
 */
typedef size_t (*xyzimage_write_func_t)(void* userdata, const void* buffer, size_t amount, xyzimage_error_t* error);

/**
 * Instantiates a new, empty (black) XYZ image providing a buffer in the specified format.
 *
 * @param width Width of the image in pixel
 * @param height Height of the image in pixel
 * @param format Pixel format of the image buffer
 * @param error When non-null receives the error code when an error occurred
 * @return An instance of XYZImage when successful, on error NULL is returned and an error code set.
 */
XYZImage* xyzimage_alloc(uint16_t width, uint16_t height, enum XYZImage_Format format, xyzimage_error_t* error);

/**
 * Frees the memory of the passed XYZ image.
 *
 * @param image The XYZ image to delete
 * @return 1 on success, 0 on error (passed
 */
int xyzimage_free(XYZImage* image);

/**
 * Loads a XYZ image from a FILE handle.
 * The pixel format of images loaded through this function is XYZIMAGE_FORMAT_DEFAULT.
 *
 * @param file Handle to read from
 * @param error When non-null receives the error code when an error occurred
 * @return An instance of XYZImage when successful, on error NULL is returned and an error code set.
 */
XYZImage* xyzimage_fopen(FILE* file, xyzimage_error_t* error);

/**
 * Loads a XYZ image using a custom read function.
 * The pixel format of images loaded through this function is XYZIMAGE_FORMAT_DEFAULT.
 *
 * @param userdata Custom data forwarded to read_func
 * @param read_func Custom read function used for parsing
 * @param error When non-null receives the error code when an error occurred
 * @return An instance of XYZImage when successful, on error NULL is returned and an error code set.
 */
XYZImage* xyzimage_open(void* userdata, xyzimage_read_func_t read_func, xyzimage_error_t* error);

/**
 * Retrieves the width of the XYZ image.
 *
 * @param image Instance of XYZImage
 * @return width of the image, 0 when the image is invalid.
 */
uint16_t xyzimage_get_width(const XYZImage* image);

/**
 * Retrieves the height of the XYZ image.
 *
 * @param image Instance of XYZImage
 * @return height of the image, 0 when the image is invalid.
 */
uint16_t xyzimage_get_height(const XYZImage* image);

/**
 * Retrieves the palette of the XYZ image for read/write operations.
 * Fails when the image format of the buffer is not using an indexed palette.
 *
 * @param image Instance of XYZImage
 * @param error When non-null receives the error code when an error occurred
 * @return Pointer to the palette or NULL on error
 */
XYZImage_Palette* xyzimage_get_palette(XYZImage* image, xyzimage_error_t* error);

/**
 * Retrieves the pixel format of the XYZ image.
 *
 * @param image Instance of XYZImage
 * @return Pixel format of the image
 */
enum XYZImage_Format xyzimage_get_format(const XYZImage* image);

/**
 * Retrieves the image buffer of the XYZ image for read/write operations.
 *
 * @param image Instance of XYZImage
 * @param len When non-null receives the size of the image buffer in bytes
 * @return Pointer to the image buffer or NULL on error
 */
void* xyzimage_get_buffer(XYZImage* image, size_t* len);

/**
 * Calculates the theoretical filesize when the image would be saved uncompressed.
 *
 * @param image Instance of XYZImage
 * @return Filesize calculated as width * height + 256 * 3 + 8, 0 on error
 */
size_t xyzimage_get_filesize(const XYZImage* image);

/**
 * Retrieves the filesize of the compressed image.
 *
 * When xyzpriv_alloc was used the result is undefined.
 * When any read function was used the result is the amount of read bytes.
 * When any write function was used the result is the amount of written bytes.
 *
 * @param image Instance of XYZImage
 * @return Filesize calculates as size of compressed buffer + 8
 */
size_t xyzimage_get_compressed_filesize(const XYZImage* image);

/**
 * Allows specifying of a custom compression function for the write functions.
 * Only for advanced use cases.
 *
 * @param image Instance of XYZImage
 * @param compress_func Custom compress function to use, when NULL zlib DEFLATE is used.
 */
void xyzimage_set_compress_func(XYZImage* image, xyzimage_compress_func_t compress_func);

/**
 * Writes a XYZ image to a FILE handle.
 *
 * @param image Instance of XYZImage
 * @param file Handle to write to
 * @param error When non-null receives the error code when an error occurred
 * @return 1 on success, on error 0 is returned and an error code set.
 */
int xyzimage_fwrite(XYZImage* image, FILE* file, xyzimage_error_t* error);

/**
 * Writes a XYZ image using a custom write function.
 *
 * @param userdata Custom data forwarded to write_func
 * @param write_func Custom write function
 * @param error When non-null receives the error code when an error occurred
 * @return 1 on success, on error 0 is returned and an error code set.
 */
int xyzimage_write(XYZImage* image, void* userdata, xyzimage_write_func_t write_func, xyzimage_error_t* error);

/**
 * Checks if the passed pointer points to a valid XYZImage struct.
 * This only fails when the struct was freed or the pointer is invalid.
 * The alloc and open functions always return valid pointers.
 *
 * @param image Pointer to check.
 * @return 1 if valid, 0 if invalid
 */
int xyzimage_is_valid(const XYZImage* image);

/**
 * Converts the passed error code to a textual representation.
 *
 * @param error error code
 * @return Error message
 */
const char* xyzimage_get_error_message(xyzimage_error_t error);

#ifdef __cplusplus
}
#endif

#endif // LIBXYZIMAGE_XYZIMAGE_H
