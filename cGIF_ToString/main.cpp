/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#ifdef _DEBUG
#include <vld.h>
#endif
#include <cstdlib>
#include <cstdio>
extern "C"
{
#include "../cGIF/cGIF.h"
}
#pragma comment(lib, "../Debug/cGIF.lib")

void usage()
{
	printf("Usage: cGIF [/D] <gif file path>\n"
		"\tUse /D if it is a dynamic picture.\n");
}

typedef enum _PictureType {Static, Dynamic} PictureType;

int main(int argc, char* argv[])
{
	if (!(argc == 2 || argc ==3))
	{
		usage();
		return 1;
	}

	PictureType type = Static;
	char* path = argv[1];
	if (argc == 3)//imply it is a dynamic picture
	{
		if (strcmp("/D", argv[1]) == 0)
		{
			type = Dynamic;
			path = argv[2];
		}
		else
		{
			usage();
			return 1;
		}
	}
	
	if (type == Static)
	{
		unsigned char* pColorIndexArray = NULL;
		unsigned int canvasWidth = 0, canvasHeight = 0;
		unsigned int image_position_x = 0, image_position_y = 0;
		unsigned int imageWidth = 0, imageHeight = 0;
		unsigned char* palette = NULL;
		unsigned char palette_color_count = 0;
		unsigned char background_color_index = 0;
		unsigned char transparent_color_index = 0;
		char flag = 0;

		cGif_Error error = cGif_decode_static_indexed(
			path,
			&pColorIndexArray,
			&canvasWidth, &canvasHeight,
			&background_color_index,
			&image_position_x, &image_position_y,
			&imageWidth, &imageHeight,
			(unsigned char**)&palette, &palette_color_count,
			&transparent_color_index,
			&flag);
		if (error != cGif_Success)
		{
			printf(cGif_error_text(error));
		}
		else//print info of this picture
		{
			unsigned int i = 0, j = 0;

			printf("The canvas size is %d¡Á%d\n", canvasWidth, canvasHeight);
			printf("The image is located at (%d,%d), size: %d¡Á%d\n",
				image_position_x, image_position_y,
				imageWidth, imageHeight);
			puts("palette:");
			for (i = 0; i < palette_color_count; i++)
			{
				printf("%d: (0x%02x, 0x%02x, 0x%02x)\t",
					i, *(palette + 3*i), *(palette + 3*i + 1), *(palette + 3*i + 2));
				if ((i+1) % 4 == 0)
					putchar('\n');
			}
			putchar('\n');
			if (flag & 0x01)
			{
				printf("Background color: exist, index: 0x%02x\n", background_color_index);
			}
			else
			{
				puts("Background color: none");
			}
			if (flag & 0x01)
			{
				printf("Transparent color: exist, index: 0x%02x\n", transparent_color_index);
			}
			else
			{
				puts("Transparent color: none");
			}
			puts("Disposal method:");
			int disposalMethod = ((flag & 0x1C) >> 2);
			switch (disposalMethod)
			{
			case 0:
				puts("No disposal specified.");
				break;
			case 1:
				puts("Do not dispose.");
				break;
			case 2:
				puts("Restore to background color.");
				break;
			case 3:
				puts("Restore to previous.");
				break;
			default:
				puts("Not defined.");
				break;
			}
			puts("Image data(indexes in hex):");
			for (i = 0; i < imageHeight; ++i)
			{
				for (j = 0; j < imageWidth; ++j)
				{
					printf("%02x ", *(pColorIndexArray + imageWidth*i + j));
				}
				putchar('\n');
			}
			free(pColorIndexArray);
			free(palette);
		}
	}
	else//Dynamic picture
	{
		unsigned canvasWidth;
		unsigned canvasHeight;
		unsigned char* globalPalette;
		unsigned char globalPalette_color_count;
		unsigned char background_color_index;
		unsigned frameCount;
		unsigned* frameTimeInMS = NULL;
		unsigned char** ppColorIndexArray = NULL;//indexes
		unsigned* image_position_x = NULL;
		unsigned* image_position_y = NULL;
		unsigned* imageWidth = NULL;
		unsigned* imageHeight = NULL;
		unsigned char** palette = NULL;
		unsigned char* palette_color_count = NULL;
		unsigned char* transparent_color_index = NULL;
		char* flag;
		char globalFlag;

		cGif_Error error = cGif_decode_dynamic_indexed(
			path,
			&canvasWidth, &canvasHeight,
			&globalPalette,
			&globalPalette_color_count,
			&background_color_index,
			&globalFlag,
			&frameCount,
			&frameTimeInMS,
			&ppColorIndexArray,
			&image_position_x, &image_position_y,
			&imageWidth, &imageHeight,
			&palette, &palette_color_count,
			&transparent_color_index,
			&flag);

		if (error != cGif_Success)
		{
			puts(cGif_error_text(error));
		}
		else//print info of this picture
		{
			unsigned int i = 0, j = 0, k = 0;

			printf("The canvas size is %d¡Á%d.\n", canvasWidth, canvasHeight);
			if (globalPalette != nullptr)
			{
				puts("A global color table exists.");
				if (globalFlag & 0x01)
				{
					puts("Using background color.");
				}
				else
				{
					puts("Not using background color.");
				}
				printf("The background color index is %d\n", background_color_index);
				puts("Global palette:");
				int colorCount = globalPalette_color_count;
				if (colorCount == 0)
				{
					colorCount = 256;
				}
				for (i = 0U; i < colorCount; ++i)
				{
					printf("0x%02x: (0x%02x, 0x%02x, 0x%02x)\t",
						i, *(globalPalette + 3 * i), *(globalPalette + 3 * i + 1), *(globalPalette + 3 * i + 2));
					if ((i + 1) % 4 == 0)
						putchar('\n');
				}
			}
			else
			{
				puts("No global palette.");
			}
			putchar('\n');
			puts("Per frame info:");
			for (i = 0; i < frameCount; ++i)
			{
				unsigned char* pColorIndexArray = ppColorIndexArray[i];
				printf("--Frame %d--\n", i);
				printf("The image is located at (%d,%d), size: %d¡Á%d\n",
					image_position_x[i], image_position_y[i],
					imageWidth[i], imageHeight[i]);
				if (palette[i] == nullptr)
				{
					puts("No frame palette.");
				}
				else
				{
					int colorCount = palette_color_count[i];
					if (colorCount == 0)
					{
						colorCount = 256;
					}
					puts("Frame palette:");
					for (j = 0U; j < colorCount; j++)
					{
						printf("0x%02x: (0x%02x, 0x%02x, 0x%02x)\t",
							j, *(palette[i] + 3 * j), *(palette[i] + 3 * j + 1), *(palette[i] + 3 * j + 2));
						if ((j + 1) % 4 == 0)
							putchar('\n');
					}
					putchar('\n');
				}
				if (flag[i] & 0x01)
				{
					printf("Using transparent color, the index is 0x%02x\n", transparent_color_index[i]);
				}
				else
				{
					puts("Not using transparent color.");
				}
				puts("Disposal method:");
				int disposalMethod = ((flag[i] & 0x1C) >> 2);
				switch (disposalMethod)
				{
				case 0:
					puts("No disposal specified.");
					break;
				case 1:
					puts("Do not dispose.");
					break;
				case 2:
					puts("Restore to background color.");
					break;
				case 3:
					puts("Restore to previous.");
					break;
				default:
					puts("Not defined.");
					break;
				}
				puts("Image data(indexes in hex):");
				for (j = 0; j < imageHeight[i]; ++j)
				{
					for (k = 0; k < imageWidth[i]; ++k)
					{
						printf("%02x ", *(pColorIndexArray + j*imageWidth[i] + k));
					}
					putchar('\n');
				}
				putchar('\n');
			}
		}
	}//END of dynamic picture
}