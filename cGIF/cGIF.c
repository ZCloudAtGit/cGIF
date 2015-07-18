/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#include "cGif.h"

/**
@brief Internal function for expanding frames
*/
cGif_Error _cGif_expand_frames(
	unsigned* k,
	unsigned frameCount,
	unsigned char*** indexes,
	cGif_State_Frame** frameState)
{
	if (frameCount == 1024)
	{
		return cGif_Too_Many_Frames;
	}
	else
	{
		if (frameCount == 0)//First allocation
		{
			*k = 16;
			*frameState = malloc((*k) * sizeof(cGif_State_Frame));
			*indexes = malloc((*k) * sizeof(unsigned char**));
			assert(*frameState);
			assert(*indexes);
		}
		else if (frameCount == 16 || frameCount == 32
			|| frameCount == 64 || frameCount == 128
			|| frameCount == 256 || frameCount == 512)
		{
			//double the buffer, and copy the data
			*k *= 2;
			*frameState = realloc(*frameState, (*k) * sizeof(cGif_State_Frame));
			*indexes = realloc(*indexes, (*k) * sizeof(unsigned));
			//call of realloc must succeed, otherwise abort.
			assert(*frameState);
			assert(*indexes);
		}

		//Can not expand to more than 1024 frames
		if (frameCount>=1024)
		{
			assert(0);
		}
		return cGif_Success;
	}
}

/**
@brief Decode static picture into indexes of colors
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
	char* flag)
{
	//TODO Secure correctness of the initializing methods below.
	Header header = { 0 };
	ImageData imgData = { 0 };
	FILE* file = 0;
	unsigned char byteBuf0 = 0, byteBuf1 = 0;
	cGif_State_Global globalState = { 0 };
	cGif_State_Frame frameState = { 0 };

	//open GIF file | 打开gif文件
	file = fopen(filePath, "rb");
	if (file == NULL)
	{
		return cGif_File_Not_Exist;
	}

	//clear output parameters
	*indexes = NULL;
	*canvasWidth = 0;
	*canvasHeight = 0;
	*image_position_x = 0;
	*image_position_y = 0;
	*imageWidth = 0;
	*imageHeight = 0;
	*palette = NULL;
	*palette_color_count = 0;
	*transparent_color_index = 0;
	*flag = 0;

	ReadHeader(&header, file);//Header

	ReadLCD(&globalState.lsd, file);//Logical Screen Descriptor

	if (globalState.lsd.GCTFlag == 1)
	{
		ReadGCT(&globalState.lsd, &globalState.gct, file);//Global color table
	}

	while (1)
	{
		//读取可能的Introducer和Label
		fread(&byteBuf0, sizeof(byteBuf0), 1, file);
		fread(&byteBuf1, sizeof(byteBuf1), 1, file);
		fseek(file, -2, SEEK_CUR);//回退2字节
		if (byteBuf0 == 0x21 && byteBuf1 == 0xf9)
		{
			ReadGCE(&frameState.gce, file);//Graphic Control Extension
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xf9)
		{
			ReadPTE(file);//Plain Text Extension
		}
		else if (byteBuf0 == 0x2c)
		{
			ReadGRB(&imgData, &frameState, file);//Graphic-Rendering Block
			globalState.error = DecompressImageData(indexes, &imgData, &frameState.imgDesc);
			ReleaseSubBlock(&imgData, NULL, NULL);
			break;//只取读到的第一个Image数据
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xff)
		{
			ReadAE(&globalState.ae, file);//Application Extension
			ReleaseSubBlock(NULL, &globalState.ae, NULL);
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xfe)
		{
			CommentExtension CE = { 0 };
			ReadCE(&CE, file);//Comment Extension 
			ReleaseSubBlock(NULL, NULL, &CE);
		}
		else if (byteBuf0 == 0x3B)
		{
			break;
		}
		else
		{
			globalState.error = cGif_Unknown_Introducer_Or_Label;
			break;
		}
	}
	fclose(file);
	file = 0;

	if (globalState.error == cGif_Success)
	{
		//assign output parameters when successfully decoded
		*canvasWidth = globalState.lsd.LogicalScreenWidth;
		*canvasHeight = globalState.lsd.LogicalScreenHeight;
		*image_position_x = frameState.imgDesc.LeftPosition;
		*image_position_y = frameState.imgDesc.TopPosition;
		*imageWidth = frameState.imgDesc.Width;
		*imageHeight = frameState.imgDesc.Height;

		*flag |= ((frameState.gce.DisposalMethod & 0x07) << 2);

		//Set the palette(perfer local to global one)
		if (frameState.imgDesc.LocalColorTableFlag)
		{
			*palette = (unsigned char *)frameState.lct.vColor;
			*palette_color_count = frameState.lct.ColorCount - 1;
		}
		else if (globalState.lsd.GCTFlag)
		{
			*palette = (unsigned char *)globalState.gct.vColor;
			*palette_color_count = globalState.gct.ColorCount - 1;
		}

		//Set background color index if any
		/*
		If the Global Color Table Flag is set to (zero), 
		Background Color Index should be zero
		and should be ignored.
		*/
		if (globalState.lsd.GCTFlag)
		{
			*background_color_index = globalState.lsd.BackgroundColorIndex;
			*flag |= 0x01;
		}
		else//No Global Color Table, so the background color index is invalid
		{
			*background_color_index = (unsigned char)-1;
		}

		//Set transparent color index if any
		if (frameState.gce.TransparentColorFlag)
		{
			*transparent_color_index = frameState.gce.TransparentColorIndex;
			*flag |= 0x02;
		}
		else
		{
			*transparent_color_index = (unsigned char)-1;//No transparent color
		}
	}

	return globalState.error;
}


/**
@brief Decode dynamic picture into indexes of colors
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
	char** flag)
{
	Header header = { 0 };
	ImageData imgData = { 0 };
	cGif_State_Global state = { 0 };
	cGif_State_Frame* frameState = NULL;
	FILE* file = 0;
	unsigned char byteBuf0 = 0, byteBuf1 = 0;
	unsigned i = 0;	//current frame index
	int k = 0;	//for expanding frames

	//open the gif file
	file = fopen(filePath, "rb");
	if (file == NULL)
	{
		return cGif_File_Not_Exist;
	}

	//Clear output parameters
	*frameCount = 0;
	*canvasWidth = 0;
	*canvasHeight = 0;
	*background_color_index = 0;
	*globalFlag = 0;

	//Firset allocation for frames
	_cGif_expand_frames(&k, *frameCount, indexes, &frameState);

	//Start parsing the file
	ReadHeader(&header, file);//Header
	ReadLCD(&state.lsd, file);//Logical Screen Descriptor

	*canvasWidth = state.lsd.LogicalScreenWidth;
	*canvasHeight = state.lsd.LogicalScreenHeight;

	if (state.lsd.GCTFlag == 1)
	{
		//Set the flag of background color 
		/*
		If the Global Color Table Flag is set to (zero),
		Background Color Index is invalid.
		*/
		*globalFlag |= 0x01;

		ReadGCT(&state.lsd, &state.gct, file);//Global color table

		*globalPalette = (unsigned char *)(state.gct.vColor);
		*globalPalette_color_count = state.gct.ColorCount;
		*background_color_index = state.lsd.BackgroundColorIndex;
	}
	else//No global color table
	{
		state.gct.ColorCount = 0;

		//the background color index is invalid
		*background_color_index = (unsigned char)-1;
		*globalPalette = NULL;
		*globalPalette_color_count = 0;
	}

	while (1)
	{
		//read possible Introducer and Label
		fread(&byteBuf0, sizeof(byteBuf0), 1, file);
		fread(&byteBuf1, sizeof(byteBuf1), 1, file);
		fseek(file, -2, SEEK_CUR);//fallback 2 bytes
		if (byteBuf0 == 0x21 && byteBuf1 == 0xf9)
		{
			ReadGCE(&frameState[*frameCount].gce, file);
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xf9)
		{
			ReadPTE(file);
		}
		else if (byteBuf0 == 0x2c)
		{
			/*NOTE: imgData will be reused per frame*/
			ReadGRB(&imgData, &frameState[*frameCount], file);
			DecompressImageData(&(*indexes)[*frameCount], &imgData,
				&frameState[*frameCount].imgDesc);
			ReleaseSubBlock(&imgData, NULL, NULL);
			++(*frameCount);
			//Detect if frame count exceeds the defined limit.
			//If so, realloc the allocated memory for all frames.
			_cGif_expand_frames(
				&k,
				*frameCount,
				indexes,
				&frameState);
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xff)
		{
			ReadAE(&state.ae, file);
			ReleaseSubBlock(NULL, &state.ae, NULL);
		}
		else if (byteBuf0 == 0x21 && byteBuf1 == 0xfe)
		{
			//CommentExtension CE = { 0 };
			//ReadCE(&CE, file);//TODO: Just skip it: no need to read.
			//ReleaseSubBlock(NULL, NULL, &CE);
			SkipCE(file);
		}
		else if (byteBuf0 == 0x3B)//encounter the end of file data
		{
			break;
		}
		else
		{ 
			state.error = cGif_Unknown_Introducer_Or_Label;
			break;
		}
	}

	fclose(file);
	file = 0;

	//Generate per frame output
	//Allocate memory for output
	char* tmp = malloc(
		(*frameCount)
		*(sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		+ sizeof(unsigned)//(*imageWidth)[i]
		+ sizeof(unsigned)//(*imageHeight)[i]
		+ sizeof(unsigned)//(*frameTimeInMS)[i]
		+ sizeof(unsigned)//(*transparent_color_index)[i]
		+ sizeof(char)//(*flag)[i]
		));
	*palette = tmp;
	*palette_color_count = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		);
	*image_position_x = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		);
	*image_position_y = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		);
	*imageWidth = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		);
	*imageHeight = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		+ sizeof(unsigned)//(*imageWidth)[i]
		);
	*frameTimeInMS = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		+ sizeof(unsigned)//(*imageWidth)[i]
		+ sizeof(unsigned)//(*imageHeight)[i]
		);
	*transparent_color_index = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		+ sizeof(unsigned)//(*imageWidth)[i]
		+ sizeof(unsigned)//(*imageHeight)[i]
		+ sizeof(unsigned)//(*frameTimeInMS)[i]
		);
	*flag = tmp + (*frameCount)*(
		sizeof(unsigned char*)//(*palette)[i]
		+ sizeof(unsigned char)//(*palette_color_count)[i]
		+ sizeof(unsigned)//(*image_position_x)[i]
		+ sizeof(unsigned)//(*image_position_y)[i]
		+ sizeof(unsigned)//(*imageWidth)[i]
		+ sizeof(unsigned)//(*imageHeight)[i]
		+ sizeof(unsigned)//(*frameTimeInMS)[i]
		+ sizeof(unsigned)//(*transparent_color_index)[i]
		);
	for (i = 0; i < (*frameCount); ++i)
	{
		(*palette)[i] = (unsigned char*)(frameState[i].lct.vColor);
		(*palette_color_count)[i] = frameState[i].lct.ColorCount;
		(*image_position_x)[i] = frameState[i].imgDesc.LeftPosition;
		(*image_position_y)[i] = frameState[i].imgDesc.TopPosition;
		(*imageWidth)[i] = frameState[i].imgDesc.Width;
		(*imageHeight)[i] = frameState[i].imgDesc.Height;
		//NOTE: For delay time in GCE, 1 unit equals 10ms
		(*frameTimeInMS)[i] = max(frameState[i].gce.DelayTime * 10, 50);

		(*flag)[i] = 0;
		//Set the flag of disposal method
		(*flag)[i] |= ((frameState[i].gce.DisposalMethod & 0x07) << 2);

		//Set the flag and index of transparent color
		if (frameState[i].gce.TransparentColorFlag)
		{
			(*transparent_color_index)[i] = frameState[i].gce.TransparentColorIndex;
			(*flag)[i] |= 0x01;
		}
		else
		{
			(*transparent_color_index)[i] = (unsigned char)-1;//No transparent color
		}
	}
	free(frameState);

	return state.error;
}


const char* cGif_error_text(cGif_Error e)
{
	switch (e)
	{
	case cGif_Success: return "No error, everything went OK. | 没有错误，成功";
	case cGif_File_Not_Exist: return "Problem when trying to open the file: File do not exist. | 文件不存在";
	case cGif_Unknown_Introducer_Or_Label: return "Problem when reading introducer or label: unknown introducer or label. The file is corrupted. | 未知的Introducer或Label，文件可能已经损坏";
	//case cGif_Next_Not_Clear_Or_EOI: return "Problem when extracting code array: the next code is not clear code or EOI code. | code序列提取错误，下一个code不是Clear code或者EOF code";
	case cGif_Encounter_EOI_Before_ImageData_Completed: return "Problem when extracting code array: EOI code is encountered before finishing reading image data. | code序列提取错误，在图像数据解析完成之前遇到了EOF code";
	case cGif_Too_Many_Frames: return "Too many frames in this picture. Processing Stopped. | 图像文件中的帧过多，停止解析";
	}
	return "Unknown error code. | 未知的错误码";
}
