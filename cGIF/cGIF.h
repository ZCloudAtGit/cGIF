/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#pragma once
#include "gif.h"

#ifdef _DEBUG
cGif_Error _cGif_expand_frames(
	unsigned* k,
	unsigned frameCount,
	unsigned char*** indexes,
	cGif_State_Frame** frameState);
#endif

cGif_Error cGif_decode_static(unsigned char** out, unsigned* w, unsigned* h, const char* filename);

/**
@brief Decode a static GIF file into color indexes

You should call free() to release \a indexes and \a palette when you have copied data from them.

About flag:
bit 76543210 (from high to low)
bit 0: 1 if background_color_index is invalid, 0 otherwise
bit 1: 1 if transparent_color_index is invalid, 0 otherwise
bit 2~4: disposal method

About disposal method:
The disposal method implies what shold be done before rendering this picture.
0: No disposal specified. The decoder is not required to take any action.
1: Do not dispose. The graphic is to be left in place.
2: Restore to background color. The area used by the graphic must be restored to the background color.
3: Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.
other value: invalid

@param[in] filePath Path of the GIF file to decode
@param[out] out The color indexes
@param[out] canvasWidth The width of the canvas
@param[out] canvasHeight The height of the canvas
@param[out] image_position_x The position of the top-left most pixel of the image
@param[out] image_position_y The height of the image
@param[out] imageWidth The width of the image
@param[out] imageHeight The height of the image
@param[out] palette The palette of the image. eg. (R:12,G:30,B:207),(R:11,G:100,B:123),...
@param[out] palette_color_count The total number of colors in the palette minus 1.
For example, if there are 128 colors in the palette, this value will be 127.
@param[out] background_color_index The Index of background color in the pattle
@param[out] transparent_color_index The Index of transparent color in the pattle
@param[out] flag flag of output info
*/
cGif_Error cGif_decode_static_indexed(
	const char* filePath,
	unsigned char** indexes,
	unsigned* canvasWidth, unsigned* canvasHeight,
	unsigned char* background_color_index,
	unsigned* image_position_x, unsigned* image_position_y,
	unsigned* imageWidth, unsigned* imageHeight,
	unsigned char** palette, unsigned char* palette_color_count,
	unsigned char* transparent_color_index,
	char* flag);

/**
@brief Decode a dynamic GIF file into color indexes

You should call free() to release
\a indexes, \a image_position_x, \a image_position_y,
\a imageWidth, \a imageHeight, \a palette, \a palette_color_count
when you have copied data from them.

About flag:
bit 76543210 (from high to low)
bit 0: 1 if background_color_index is invalid, 0 otherwise
bit 1: 1 if transparent_color_index is invalid, 0 otherwise
bit 2~4: disposal method

About disposal method:
The disposal method implies what shold be done before rendering this picture.
0: No disposal specified. The decoder is not required to take any action.
1: Do not dispose. The graphic is to be left in place.
2: Restore to background color. The area used by the graphic must be restored to the background color.
3: Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.
other value: invalid

@param[in] filePath Path of the GIF file to decode
@param[out] canvasWidth The width of the canvas
@param[out] canvasHeight The height of the canvas
@param[out] globalPalette The global palette(used only for those frames without palette). eg. (R:12,G:30,B:207),(R:11,G:100,B:123),...
@param[out] background_color_index The Index of background color in the global pattle(invalid if globalPalette is NULL)
@param[out] frameCount Count of the frames in this picture
@param[out] frameTimeInMS Frame duration in ms(per frame)
@param[out] indexes The color indexes (per frame)
@param[out] image_position_x The position of the top-left most pixel of the image (per frame)
@param[out] image_position_y The height of the image (per frame)
@param[out] imageWidth The width of the image (per frame)
@param[out] imageHeight The height of the image (per frame)
@param[out] palette The palette of the image (per frame). eg. (R:12,G:30,B:207),(R:11,G:100,B:123),...
@param[out] palette_color_count The total number of colors in the pelette minus 1 (per frame)
For example, if there are 128 colors in the palette, this value will be 127.
@param[out] transparent_color_index The Index of transparent color in the pattle (per frame)
@param[out] flag flag of output info (per frame)
*/
cGif_Error cGif_decode_dynamic_indexed(
	const char* filePath,
	unsigned* canvasWidth, unsigned* canvasHeight,
	unsigned char** globalPalette,
	unsigned char* globalPalette_color_count,
	unsigned char* background_color_index,
	char* globalFlag,
	unsigned* frameCount,
	unsigned** frameTimeInMS,
	unsigned char*** indexes,
	unsigned** image_position_x, unsigned** image_position_y,
	unsigned** imageWidth, unsigned** imageHeight,
	unsigned char*** palette, unsigned char** palette_color_count,
	unsigned char** transparent_color_index,
	char** flag);

cGif_Error cGif_Dispose();

const char* cGif_error_text(cGif_Error error);