/*
(c) 2014-2015 cloud
Website: http:\\cloud.s2.jutuo.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#include "gif.h"
#include "bitRead.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>

#include <Windows.h>

//#define Do_Benchmark_for_DecompressImageData
//#define Dump_Index_Array

#pragma region 基础结构操作函数

#pragma region SubBlock操作函数定义
//Release allocated memory for Sub Data
void ReleaseSubBlock(ImageData* pImgData, ApplicationExtension* pAppExt, CommentExtension* pCE)
{
	unsigned int i = 0;

	if (pImgData != NULL)
	{
		for (i = 0; i < pImgData->SubBlockCount; ++i)
		{
			free(pImgData->SubBlock[i]);
			pImgData->SubBlock[i] = 0;
		}
		free(pImgData->SubBlock);
		pImgData->SubBlock = NULL;
		free(pImgData->SubBlockSize);
		pImgData->SubBlockSize = NULL;
	}

	if (pAppExt != NULL)
	{
		for (i = 0; i < pAppExt->SubBlockCount; ++i)
		{
			free(pAppExt->SubBlock[i]);
			pAppExt->SubBlock[i] = 0;
		}
	}

	if (pCE != NULL)
	{
		for (i = 0; i < pCE->SubBlockCount; ++i)
		{
			free(pCE->SubBlock[i]);
			pCE->SubBlock[i] = 0;
		}
	}
}

//将ImageData中的Subblock数据整合为一整块
void CombineSubBlocks(ImageData* pImgData, unsigned char** pCodeData, unsigned int* pCodeDataByteSize)
{
	unsigned int i = 0;
	unsigned int offset = 0;
	unsigned int j = 0;

	for (i = 0; i < pImgData->SubBlockCount; ++i)
	{
		*pCodeDataByteSize += pImgData->SubBlockSize[i];
	}
	*pCodeData = (unsigned char*)malloc(*pCodeDataByteSize);
	for (i = 0; i < pImgData->SubBlockCount; ++i)
	{
		memcpy(*pCodeData + offset,
			pImgData->SubBlock[i],
			pImgData->SubBlockSize[i]);
		offset += pImgData->SubBlockSize[i];
	}
	assert(*pCodeDataByteSize == offset);


#ifdef Dump_SubBlock_Data_of_ImageData
	//打印所有subblock
	for (i = 0; i < pImgData->SubBlockCount; ++i)
	{
		printf("Subblock%02x(size %d):\n", i, pImgData->SubBlockSize[i]);
		for (j = 1; j <= pImgData->SubBlockSize[i]; ++j)
		{
			printf("%02x ", pImgData->SubBlock[i][j-1]);
			if (j % 16 == 0)
			{
				putchar('\n');
			}
			else if (j % 8 == 0)
			{
				putchar(' ');
			}
		}
		putchar('\n');
	}
#endif
}
#pragma endregion

#pragma region 解压缩相关函数定义
//从code数据块中提取code序列
cGif_Error ExtractCodeArray(unsigned char* CodeData, unsigned int CodeDataByteSize,
	unsigned int LZWMinCodeSize,
	unsigned int** ppCodeArray, unsigned int* pCodeArrayCount)
{
	unsigned int codeLength = LZWMinCodeSize + 1;//初始Code长度
	unsigned int offset = 0;
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
	unsigned int count = 0;
#endif
	unsigned int n = 0;
	unsigned int code = 0;

	unsigned int ClearCode = 1 << LZWMinCodeSize;	//Clear Code
	unsigned int EOICode = ClearCode + 1;		//EOI Code

#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
	printf("Code序列:\n");
#endif
	while (code != EOICode)
	{
		//读取1个code
		code = GetCode(CodeData, offset, codeLength);
		(*ppCodeArray)[*pCodeArrayCount] = code;
		(*pCodeArrayCount)++;
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
		printf("count(%05x),n(%d),offset(%d),codeLength(%d): #%03x\n", count, n, offset, codeLength, code);
		++count;
#endif
		++n;
		offset += codeLength;

		if (code == ClearCode)
		{
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
			printf("遇到Clear code(#%03x)，重置code长度为clear code的长度\n", code);
#endif
			codeLength = LZWMinCodeSize + 1;
			n = 1;//重置(包含这个clear code在内)
			continue;
		}

		//检查下次读取code时，是否需要增加长度
		if (n == 1 << (codeLength - 1))
		{
			if (codeLength == 12)//长度为12时
			{
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
				printf("code长度将超过12，code长度将固定为12直到读取到bit长度为12的clear code(#%03x)\n", ClearCode);
#endif
			}
			else
			{
				++codeLength;//增加长度
				n = 0;//重置
			}
		}
	}
	//检查是否读到了最后1字节（考虑到可能有高位补0的情况，所以最后一个code(EOI code)读完时offset很有可能不在codedata末尾）
	//offset==(CodeDataByteSize-1)*8时，offset指向最后一字节的第1位，这也是不对的，因为这1位是预想中下一次读取的code的第1位
	if (offset <= (CodeDataByteSize - 1) * 8)
	{
		//printf("错误：图像数据未读取完成，就遇到了EOI code(#%d)\n", EOICode);
		return cGif_Encounter_EOI_Before_ImageData_Completed;
	}
	//printf("读取到了EOI code，code序列结束\n");
	return cGif_Success;
}

//将code序列翻译为颜色索引
cGif_Error TranslateCodeArray(unsigned char** ppColorIndexArray, ImageData* pImgData, ImageDescriptor* pImgDesc, unsigned int* CodeArray, unsigned int CodeArrayCount)
{
	unsigned int i = 0, j = 0;
	unsigned int code = 0;
	unsigned int ClearCode = 0, EOICode = 0;
	CodeTable codeTable;
	unsigned int InitialColorTableLength = 0;
	unsigned int ColorIndexArrayCount = 0;
	unsigned char* CodeContent = NULL;
	unsigned int CodeContentLength = 0;
	unsigned char newcodeContent[MAX_CODE_CONTENT_LENGHTH] = { 0 };

	ClearCode = 1 << pImgData->LZWMinCodeSize;//Clear Code
	EOICode = ClearCode + 1;//EOI Code
	//分配颜色索引的内存大小
	*ppColorIndexArray = (unsigned char*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned char));

	InitialColorTableLength = 2 << (pImgData->LZWMinCodeSize - 1);
	InitCodeTable(&codeTable, InitialColorTableLength);
	ColorIndexArrayCount = 0;

	assert(CodeArray[0] == ClearCode);//第一个code必须是Clear code
	assert(CodeInTable(&codeTable, CodeArray[1]) == 1);//第二个code必须在code table中
	//将第二个code的content附加到颜色索引序列中
	code = CodeArray[1];
	//获取code的Content
	GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
	//将content附加到颜色索引序列中
	for (j = 0; j < CodeContentLength; ++j)
	{
		(*ppColorIndexArray)[ColorIndexArrayCount] = CodeContent[j];
		++ColorIndexArrayCount;
	}

	for (i = 2; i < CodeArrayCount; ++i)//跳过第一个Clear Code和第二个code
	{
		unsigned char k = 0;
		unsigned int newcode = 0;
		unsigned int newcodeContentLength = 0;

		code = CodeArray[i];
		if (code == ClearCode)
		{
#ifdef Dump_Processing_Procedure_of_Translating_Code_Sequence
			printf("遇到Clear Code(#%d)，重置code table\n", code);
#endif
			ResetCodeTable(&codeTable, InitialColorTableLength);
			//将Clear Code后的那个code的content附加到颜色索引序列中
			++i;
			code = CodeArray[i];
			//Clear Code后的那个code必须在code table中
			assert(CodeInTable(&codeTable, CodeArray[1]) == 1);
			//获取code的Content
			GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
			//将content附加到颜色索引序列中
			for (j = 0; j < CodeContentLength; ++j)
			{
				(*ppColorIndexArray)[ColorIndexArrayCount] = CodeContent[j];
				++ColorIndexArrayCount;
			}

			continue;
		}
		else if (code == EOICode)
		{
#ifdef Dump_Processing_Procedure_of_Translating_Code_Sequence
			printf("遇到EOI Code(#%d)，code码解析完成\n", code);
#endif
			break;
		}

		//判断code是否在code table中
		if (CodeInTable(&codeTable, code) > 0)//在
		{
			//获取code的Content
			GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
			//将content附加到颜色索引序列中
			for (j = 0; j < CodeContentLength; ++j)
			{
				(*ppColorIndexArray)[ColorIndexArrayCount] = CodeContent[j];
				++ColorIndexArrayCount;
			}
			//生成新code
			k = CodeContent[0];	//code的Content的第一个颜色索引值
			//获取上一个code的Content
			GetCodeContent(&codeTable, CodeArray[i - 1], &CodeContent, &CodeContentLength);
			newcodeContentLength = CodeContentLength + 1;
			assert(newcodeContentLength <= MAX_CODE_CONTENT_LENGHTH);
			memcpy(newcodeContent, CodeContent, CodeContentLength*sizeof(CodeContent[0]));
			newcodeContent[newcodeContentLength - 1] = k;//附加在后面
		}
		else//不在
		{
			//获取上一个code的Content
			GetCodeContent(&codeTable, CodeArray[i - 1], &CodeContent, &CodeContentLength);
			//生成新code
			k = CodeContent[0];
			newcodeContentLength = CodeContentLength + 1;
			assert(newcodeContentLength <= MAX_CODE_CONTENT_LENGHTH);
			memcpy(newcodeContent, CodeContent, CodeContentLength*sizeof(CodeContent[0]));
			newcodeContent[newcodeContentLength - 1] = k;//附加在后面
			//将content附加到颜色索引序列中
			for (j = 0; j < newcodeContentLength; ++j)
			{
				(*ppColorIndexArray)[ColorIndexArrayCount] = newcodeContent[j];
				++ColorIndexArrayCount;
			}
		}
		//将生成的新code添加到code table
		newcode = AppendCode(&codeTable, newcodeContent, newcodeContentLength);
#ifdef Dump_Processing_Procedure_of_Translating_Code_Sequence
		printf("新增code #%d (", newcode, codeTable.count);
		for (j = 0; j < newcodeContentLength - 1; ++j)
		{
			printf("%d,", newcodeContent[j]);
		}
		printf("%d", newcodeContent[j]);
		printf(")现有%d个code\n", codeTable.count);
#endif
	}

	//若行为交错的，则还原行顺序
	if (pImgDesc->InterlaceFlag)
	{
		unsigned char* tmp = (unsigned char*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned char));
		int i = 0, n=0;
		for (i = 0; 8 * i < pImgDesc->Height; ++i)
		{
			memcpy(tmp + 8 * i * pImgDesc->Width, (*ppColorIndexArray) + n * pImgDesc->Width, pImgDesc->Width);
#ifdef Dump_Processing_Procedure_of_Recompose_Interlaced_Index_Array
			printf("row %d copied from %d\n", 8 * i, n);
#endif
			++n;
		}
		for (i = 0; 8 * i + 4 < pImgDesc->Height; ++i)
		{
			memcpy(tmp + (8 * i + 4) * pImgDesc->Width, (*ppColorIndexArray) + n * pImgDesc->Width, pImgDesc->Width);
#ifdef Dump_Processing_Procedure_of_Recompose_Interlaced_Index_Array
			printf("row %d copied from %d\n", 8 * i + 4, n);
#endif
			++n;
		}
		for (i = 0; 4 * i + 2 < pImgDesc->Height; ++i)
		{
			memcpy(tmp + (4 * i + 2) * pImgDesc->Width, (*ppColorIndexArray) + n * pImgDesc->Width, pImgDesc->Width);
#ifdef Dump_Processing_Procedure_of_Recompose_Interlaced_Index_Array
			printf("row %d copied from %d\n", 4 * i + 2, n);
#endif
			++n;
		}
		for (i = 0; 2 * i + 1 < pImgDesc->Height; ++i)
		{
			memcpy(tmp + (2 * i + 1) * pImgDesc->Width, (*ppColorIndexArray) + n * pImgDesc->Width, pImgDesc->Width);
#ifdef Dump_Processing_Procedure_of_Recompose_Interlaced_Index_Array
			printf("row %d copied from %d\n", 2 * i + 1, n);
#endif
			++n;
		}
		free(*ppColorIndexArray);
		*ppColorIndexArray = tmp;
		tmp = NULL;
	}

	//打印颜色索引序列
	assert(ColorIndexArrayCount == pImgDesc->Width*pImgDesc->Height);
#ifdef Dump_Index_Array
	printf("--- Index Array ---\n");
	for (i = 0; i < pImgDesc->Height; ++i)
	{
		for (j = 0; j < pImgDesc->Width; ++j)
		{
			printf("%02x ", (*ppColorIndexArray)[pImgDesc->Width*i + j]);
		}
		printf("\n");
	}
#endif
	FreeCodeTable(&codeTable, InitialColorTableLength);
	free(CodeArray);

	return cGif_Success;
}

//对图像数据进行解压缩
cGif_Error DecompressImageData(unsigned char** out,
	ImageData* pImgData, ImageDescriptor* pImgDesc)
{
	unsigned char* CodeData = NULL;
	unsigned int CodeDataByteSize = 0;
	unsigned int* CodeArray = NULL;
	unsigned int CodeArrayCount = 0;
	cGif_Error error = cGif_Success;

#ifdef Do_Benchmark_for_DecompressImageData
	int begin, end;
	//1.合并所有sub blocks，将数据复制到单独的连续的内存M中
	begin = GetTickCount();
	CombineSubBlocks(pImgData, &CodeData, &CodeDataByteSize);
	end = GetTickCount();
	printf("CombineSubBlocks: %dms\n", max(0, end - begin));
	//粗略的分配需要多少大小的内存存储code序列，
	//这里的估计是一个code表示一个颜色索引时所需的内存大小
	CodeArray =
		(unsigned int*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned int));

	//2.从内存M中获取code数列
	begin = GetTickCount();
	ExtractCodeArray(CodeData, CodeDataByteSize, pImgData->LZWMinCodeSize, &CodeArray,
		&CodeArrayCount);
	free(CodeData);
	end = GetTickCount();
	printf("ExtractCodeArray: %dms\n", max(0, end - begin));

	//3.解释code为颜色索引数列（解压缩）
	begin = GetTickCount();
	TranslateCodeArray(out, pImgData, pImgDesc, CodeArray, CodeArrayCount);
	end = GetTickCount();
	printf("TranslateCodeArray: %dms\n", max(0, end - begin));
#else
	//1.合并所有sub blocks，将数据复制到单独的连续的内存M中
	CombineSubBlocks(pImgData, &CodeData, &CodeDataByteSize);
	//粗略的分配需要多少大小的内存存储code序列，
	//这里的估计是一个code表示一个颜色索引时所需的内存大小
	CodeArray =
		(unsigned int*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned int));

	//2.从内存M中获取code数列
	error = ExtractCodeArray(CodeData, CodeDataByteSize, pImgData->LZWMinCodeSize,
		&CodeArray, &CodeArrayCount);
	free(CodeData);
	if (error != cGif_Success)
	{
		return error;
	}

	//3.解释code为颜色索引数列（解压缩）
	error = TranslateCodeArray(out, pImgData, pImgDesc, CodeArray, CodeArrayCount);
	if (error != cGif_Success)
	{
		return error;
	}
#endif
	return error;
}
#pragma endregion

#pragma region 基础结构读取函数定义
//Read Header
void ReadHeader(Header* pHeader, FILE* file)
{
	fread(&pHeader->Signature, sizeof(pHeader->Signature), 1, file);//Signature
	fread(&pHeader->Version, sizeof(pHeader->Version), 1, file);//Version
}

//Read Logical Screen Descriptor
void ReadLCD(LogicalScreenDescriptor* pLSD, FILE* file)
{
	unsigned char byteBuf = 0;
	fread(&pLSD->LogicalScreenWidth, sizeof(pLSD->LogicalScreenWidth), 1, file);
	fread(&pLSD->LogicalScreenHeight, sizeof(pLSD->LogicalScreenHeight), 1, file);
	fread(&byteBuf, sizeof(byteBuf), 1, file);
	pLSD->GCTFlag = ((byteBuf & 0x80) >> 7);
	pLSD->ColorResolution = ((byteBuf & 0x70) >> 4);
	pLSD->SortFlag = ((byteBuf & 0x08) >> 3);
	pLSD->SizeOfGCT = (byteBuf & 0x07);
	fread(&pLSD->BackgroundColorIndex, sizeof(pLSD->BackgroundColorIndex), 1, file);
	fread(&pLSD->PixelAspectRatio, sizeof(pLSD->PixelAspectRatio), 1, file);
}

//Read Glocal Color Table
void ReadGCT(LogicalScreenDescriptor* pLSD, ColorTable* pGCT, FILE* file)
{
	unsigned int i = 0;
	pGCT->ColorCount = 2 << pLSD->SizeOfGCT;
	pGCT->vColor = malloc(pGCT->ColorCount * sizeof(Color));
	for (i = 0; i < pGCT->ColorCount; ++i)
	{
		Color* pColor = &pGCT->vColor[i];
		fread(&pColor->r, sizeof(pColor->r), 1, file);
		fread(&pColor->g, sizeof(pColor->g), 1, file);
		fread(&pColor->b, sizeof(pColor->b), 1, file);
	}
}

//Read Graphics Control Extension
void ReadGCE(GraphicsControlExtension* pGCE, FILE* file)
{
	unsigned char byteBuf = 0;

	fread(&pGCE->Introducer, sizeof(pGCE->Introducer), 1, file);
	assert(0x21 == pGCE->Introducer);
	fread(&pGCE->Label, sizeof(pGCE->Label), 1, file);
	assert(0xf9 == pGCE->Label);
	fread(&pGCE->BlockSize, sizeof(pGCE->BlockSize), 1, file);
	assert(0x04 == pGCE->BlockSize);
	fread(&byteBuf, sizeof(byteBuf), 1, file);
	pGCE->Reserved = ((byteBuf & 0x70) >> 5);
	pGCE->DisposalMethod = ((byteBuf & 0x1c) >> 2);
	pGCE->UserInputFlag = ((byteBuf & 0x02) >> 1);
	pGCE->TransparentColorFlag = (byteBuf & 0x01);
	fread(&pGCE->DelayTime, sizeof(pGCE->DelayTime), 1, file);
	fread(&pGCE->TransparentColorIndex, sizeof(pGCE->TransparentColorIndex), 1, file);
	fread(&pGCE->BlockTerminator, sizeof(pGCE->BlockTerminator), 1, file);
}

//Read Image Descriptor
void ReadImageDesc(ImageDescriptor* pImgDesc, FILE* file)
{
	unsigned char byteBuf = 0;

	fread(&pImgDesc->Separator, sizeof(pImgDesc->Separator), 1, file);
	assert(0x2c == pImgDesc->Separator);
	fread(&pImgDesc->LeftPosition, sizeof(pImgDesc->LeftPosition), 1, file);
	fread(&pImgDesc->TopPosition, sizeof(pImgDesc->TopPosition), 1, file);
	fread(&pImgDesc->Width, sizeof(pImgDesc->Width), 1, file);
	fread(&pImgDesc->Height, sizeof(pImgDesc->Height), 1, file);
	fread(&byteBuf, sizeof(byteBuf), 1, file);
	pImgDesc->LocalColorTableFlag = (byteBuf & 0x80) >> 7;
	pImgDesc->InterlaceFlag = (byteBuf & 0x40) >> 6;
	pImgDesc->SortFlag = (byteBuf & 0x20) >> 5;
	pImgDesc->Reserved = (byteBuf & 0x18) >> 3;
	pImgDesc->SizeOfLocalColorTable = (byteBuf & 0x07);
}

//Read Local Color Table
void ReadLCT(ImageDescriptor* pImgDesc, ColorTable* pLCT, FILE* file)
{
	unsigned int i = 0;

	pLCT->ColorCount = 2 << pImgDesc->SizeOfLocalColorTable;// 2^(SizeOfLCT+1)
	pLCT->vColor = malloc(pLCT->ColorCount * sizeof(Color));
	for (i = 0U; i < pLCT->ColorCount; ++i)
	{
		Color* pColor = &pLCT->vColor[i];
		fread(&pColor->r, sizeof(pColor->r), 1, file);
		fread(&pColor->g, sizeof(pColor->g), 1, file);
		fread(&pColor->b, sizeof(pColor->b), 1, file);
	}
}

//Read Plain Text Extension
void ReadPTE(FILE* file)
{
	unsigned char byteBuf = 0;
	unsigned char Introducer = 0;
	unsigned char Label = 0;

	fread(&Introducer, sizeof(Introducer), 1, file);
	assert(0x21 == Introducer);
	fread(&Label, sizeof(Label), 1, file);
	assert(0x01 == Label);
	fread(&byteBuf, sizeof(Label), 1, file);
	assert(12 == byteBuf);
	fseek(file, 12, SEEK_CUR);
	while (1)
	{
		unsigned char subBlockSize = 0;
		unsigned char* subBlockData = 0;
		fread(&subBlockSize, sizeof(subBlockSize), 1, file);
		if (subBlockSize == 0)
		{
			break;
		}
		fseek(file, subBlockSize, SEEK_CUR);
	};
}

void ReadImageData(ImageData* pImgData, FILE* file)
{
	unsigned char byteBuf = 0;

	fread(&pImgData->LZWMinCodeSize, sizeof(pImgData->LZWMinCodeSize), 1, file);
	pImgData->SubBlock = malloc(256 * sizeof(unsigned char*));
	pImgData->SubBlockSize = malloc(256 * sizeof(unsigned char));
	pImgData->SubBlockCount = 0;
	//Read data blocks(16999 blocks at most)
	while (1)
	{
		fread(&byteBuf, sizeof(byteBuf), 1, file);//size of this sub block
		pImgData->SubBlock[pImgData->SubBlockCount] = 0;
		if (byteBuf == 0)//end of sub blocks
		{
			break;
		}
		pImgData->SubBlockSize[pImgData->SubBlockCount] = byteBuf;
		pImgData->SubBlock[pImgData->SubBlockCount] = (unsigned char*)malloc(byteBuf);
		fread(pImgData->SubBlock[pImgData->SubBlockCount], sizeof(unsigned char), byteBuf, file);
		++pImgData->SubBlockCount;
		if (pImgData->SubBlockCount == 256)//已经用了256个SubBlock了，还不够
		{
			pImgData->SubBlock = realloc(pImgData->SubBlock, 1024 * sizeof(unsigned char*));
			pImgData->SubBlockSize = realloc(pImgData->SubBlockSize, 1024 * sizeof(unsigned char));
		}
		else if (pImgData->SubBlockCount == 1024)//已经用了1024个SubBlock了，还不够
		{
			pImgData->SubBlock = realloc(pImgData->SubBlock, 4096 * sizeof(unsigned char*));
			pImgData->SubBlockSize = realloc(pImgData->SubBlockSize, 4096 * sizeof(unsigned char));
		}
	}
	pImgData->BlockTerminator = 0;
}

//Read Graphic Rendering Block
void ReadGRB(ImageData* pImgData, cGif_State_Frame* state, FILE* file)
{
	ReadImageDesc(&state->imgDesc, file);
	if (state->imgDesc.LocalColorTableFlag)
		ReadLCT(&state->imgDesc, &state->lct, file);
	else//No LCT
	{
		state->lct.vColor = NULL;
		state->lct.ColorCount = 0;
	}
	ReadImageData(pImgData, file);
}

//Read Application Extension
void ReadAE(ApplicationExtension* pAppExt, FILE* file)
{
	unsigned char byteBuf = 0;
	int i = 0;

	fread(&pAppExt->Introducer, sizeof(pAppExt->Introducer), 1, file);
	assert(0x21 == pAppExt->Introducer);
	fread(&pAppExt->Label, sizeof(pAppExt->Label), 1, file);
	assert(0xff == pAppExt->Label);
	fread(&pAppExt->BlockSize, sizeof(pAppExt->BlockSize), 1, file);
	assert(0x0b == pAppExt->BlockSize);
	fread(&pAppExt->Identifier, sizeof(pAppExt->Identifier), 1, file);
	fread(&pAppExt->AuthCode, sizeof(pAppExt->AuthCode), 1, file);
	//Read data blocks(256 blocks at most)
	for (i = 0; i < 256; ++i)
	{
		fread(&byteBuf, sizeof(byteBuf), 1, file);
		pAppExt->SubBlock[i] = 0;
		if (byteBuf == 0)
		{
			pAppExt->SubBlockCount = i;
			break;
		}
		pAppExt->SubBlockSize[i] = byteBuf;
		pAppExt->SubBlock[i] = (unsigned char*)malloc(byteBuf);
		fread(pAppExt->SubBlock[i], sizeof(unsigned char), byteBuf, file);
	}
	assert(byteBuf == 0);
	pAppExt->BlockTerminator = 0;
}

//Read Comment Extension
void ReadCE(CommentExtension* pCommentExt, FILE* file)
{
	unsigned char byteBuf = 0;
	int i = 0;

	fread(&pCommentExt->Introducer, sizeof(pCommentExt->Introducer), 1, file);
	assert(0x21 == pCommentExt->Introducer);
	fread(&pCommentExt->Label, sizeof(pCommentExt->Label), 1, file);
	assert(0xfe == pCommentExt->Label);
	//Read data blocks(256 blocks at most)
	for (i = 0; i < 256; ++i)
	{
		fread(&byteBuf, sizeof(byteBuf), 1, file);
		pCommentExt->SubBlock[i] = 0;
		if (byteBuf == 0)
		{
			pCommentExt->SubBlockCount = i;
			break;
		}
		pCommentExt->SubBlockSize[i] = byteBuf;
		pCommentExt->SubBlock[i] = (unsigned char*)malloc(byteBuf);
		fread(pCommentExt->SubBlock[i], sizeof(unsigned char), byteBuf, file);
	}
	assert(byteBuf == 0);
	pCommentExt->BlockTerminator = 0;
}
#pragma endregion

#pragma region 基础结构打印函数定义
void HeaderToString(Header* pHeader, char* buffer)
{
	char signatureBuf[4] = {0};
	char versionBuf[4] = { 0 };

	memcpy(signatureBuf, pHeader->Signature, 3 * sizeof(char));
	signatureBuf[3] = 0;
	memcpy(versionBuf, pHeader->Version, 3 * sizeof(char));
	versionBuf[3] = 0;
	sprintf(buffer,
        "Header: \n"
		"    Signature: \"%s\"\n"
		"    Version: \"%s\"\n",
		signatureBuf,
		versionBuf);
}

void LSDToString(LogicalScreenDescriptor* pLSD, char* buffer)
{
    sprintf(buffer,
        "Logical Screen Descriptor:\n"
        "    Logical screen width: %d\n"
        "    Logical screen height: %d\n"
        "    Global color table flag: %d\n"
        "    Color resolution: %d\n"
        "    Sort flag: %d\n"
        "    Size of GCT: %d\n"
        "    Background color index: %d\n"
        "    Pixel aspect ratio: %d\n",
        pLSD->LogicalScreenWidth,
        pLSD->LogicalScreenHeight,
        pLSD->GCTFlag,
        pLSD->ColorResolution,
        pLSD->SortFlag,
        pLSD->SizeOfGCT,
        pLSD->BackgroundColorIndex,
        pLSD->PixelAspectRatio);
}

void ImageDescriptorToString( ImageDescriptor* pDesc, char* buffer )
{
    sprintf(buffer,
        "ImageDescriptor:\n"
        "    Separator: 0x%x\n"
        "    Left position: %d\n"
        "    Top position: %d\n"
        "    Width: %d\n"
        "    Height: %d\n"
        "    Local color table flag: %d\n"
        "    Interlace flag: %d\n"
        "    Sort flag: %d\n"
        "    Reserved: 0x%x\n"
        "    Size of local color table: %d\n",
        pDesc->Separator,
        pDesc->LeftPosition,
        pDesc->TopPosition,
        pDesc->Width,
        pDesc->Height,
        pDesc->LocalColorTableFlag,
        pDesc->InterlaceFlag,
        pDesc->SortFlag,
        pDesc->Reserved,
        pDesc->SizeOfLocalColorTable);
}

void GCEToString( GraphicsControlExtension* pGCE, char* buffer )
{
    sprintf(buffer,
        "Graphics control extension:\n"
        "    Introducer: 0x%x\n"
        "    Label: 0x%x\n"
        "    Block size: %d\n"
        "    Reserved: %d\n"
        "    Disposal method: %d\n"
        "    User input flag: %d\n"
        "    Transparent color flag: %d\n"
        "    Delay time: %d\n"
        "    Transparent color index: %d\n"
        "    Block terminator: 0x%x\n",
        pGCE->Introducer,
        pGCE->Label,
        pGCE->BlockSize,
        pGCE->Reserved,
        pGCE->DisposalMethod,
        pGCE->UserInputFlag,
        pGCE->TransparentColorFlag,
        pGCE->DelayTime,
        pGCE->TransparentColorIndex,
        pGCE->BlockTerminator);
}

void AEToString( ApplicationExtension* pAE, char* stringBuffer )
{
    char identifierBuffer[9];
    unsigned int i = 0;

    memcpy(identifierBuffer, pAE->Identifier, sizeof(pAE->Identifier));
    identifierBuffer[8] = 0;
    sprintf(stringBuffer,
        "Application Extension:\n"
        "    Introducer: %u\n"
        "    Label: %u\n"
        "    Block size: %u\n"
        "    Identifier: %s\n"
        "    Authentication code: 0x%x,0x%x,0x%x\n"
        "    Sub blocks(%d):\n",
        pAE->Introducer,
        pAE->Label,
        pAE->BlockSize,
        identifierBuffer,
        pAE->AuthCode[0],pAE->AuthCode[1],pAE->AuthCode[2],
        pAE->SubBlockCount
    );
    for (i=0; i<pAE->SubBlockCount; ++i)
    {
        sprintf(stringBuffer+strlen(stringBuffer),
            "        %d, size %d\n",
            i,
            pAE->SubBlockSize[i]);
    }

}

void CEToString(CommentExtension* pCE, char* stringBuffer)
{
	unsigned int i = 0;

    sprintf(stringBuffer,
        "Comment Extension:\n"
        "    Introducer: 0x%x\n"
        "    Label: 0x%x\n"
        "    Sub blocks(%d):\n",
        pCE->Introducer,
        pCE->Label,
        pCE->SubBlockCount
        );
    for (i=0; i<pCE->SubBlockCount; ++i)
    {
        sprintf(stringBuffer+strlen(stringBuffer),
            "        %d, size %d\n",
            i,
            pCE->SubBlockSize[i]);
    }
}

void ImageDataToString(ImageData* pImgData, char* stringBuffer )
{
	unsigned int i = 0;
    sprintf(stringBuffer,
        "Image Data:\n"
        "    LZWMinCodeSize: %d\n"
        "    Sub blocks(%d):\n",
        pImgData->LZWMinCodeSize,
        pImgData->SubBlockCount
        );
    for (i=0; i<pImgData->SubBlockCount; ++i)
    {
        sprintf(stringBuffer+strlen(stringBuffer),
            "        %d. size %d\n",
            i,
            pImgData->SubBlockSize[i]
        );
    }
}

void GCTToString(const ColorTable* GCT, char* stringBuffer )
{
    sprintf(stringBuffer,
        "Global color table:\n"
        "    Color count: %d\n",
        GCT->ColorCount);
}

void LCTToString(const ColorTable* LCT, char* stringBuffer )
{
    sprintf(stringBuffer,
        "Local color table:\n"
        "    Color count: %d\n",
        LCT->ColorCount);
}
#pragma endregion

#pragma endregion

#pragma region Code table相关函数定义
//Code操作函数
void InitCodeTable( CodeTable* pCodeTable, unsigned short colorTableLength )
{
    unsigned int i=0;
	unsigned char* pTmp = NULL;

	pCodeTable->content = malloc(4098 * sizeof(unsigned char*));
	pCodeTable->length = malloc(4098 * sizeof(unsigned));
	pTmp = malloc((colorTableLength + 2)*sizeof(unsigned char));
    //生成Color code
    /*
     *Color codes:
     *#0, #1, ..., #(2^i-1), ..., #(2^mincodeSize-1)
    */
    for (i=0; i<colorTableLength; ++i)
    {
        pCodeTable->content[i] = pTmp + i;
        *pCodeTable->content[i] = i;
        pCodeTable->length[i] = 1;
        //Color code的content和其value相同，只有一个数字，所以长度为1
        //如Color code (code #3, content 3, length 1)
    }
    //生成Clear code
	pCodeTable->content[colorTableLength] = pTmp + colorTableLength;
    *pCodeTable->content[colorTableLength] = 0;//Clear code的内容无作用
    pCodeTable->length[colorTableLength] = 1;
    //生成EOI code
	pCodeTable->content[colorTableLength + 1] = pTmp + colorTableLength + 1;
    *pCodeTable->content[colorTableLength+1] = 0;//EOI code的内容无作用
    pCodeTable->length[colorTableLength+1] = 1;

    pCodeTable->count = colorTableLength+2;

	//为CodeTable的非color code也非Clear code也非EOI code的code的Content预分配空间
	//假设：每个code的Content的长度为512(MAX_CODE_CONTENT_LENGHTH)
	//TODO: 去除这个假设！否则如果有code的的Content的长度超过512时，解析出错
	{
		unsigned char* pTmp = malloc(MAX_CODE_CONTENT_LENGHTH * (4098 - (colorTableLength + 2))*sizeof(unsigned char));
		for (i = colorTableLength + 2; i < 4098; i++)
		{
			pCodeTable->content[i] = pTmp + (i - (colorTableLength + 2)) * MAX_CODE_CONTENT_LENGHTH;
		}
		//pTmp == pCodeTable->content[colorTableLength + 2]
		//释放时
		//free(pCodeTable->content[colorTableLength + 2]);即可
	}
}

//重置code table为初始状态
void ResetCodeTable( CodeTable* pCodeTable, unsigned short colorTableLength )
{
	pCodeTable->count = colorTableLength+2;
}

//释放CodeTable分配的内存
void FreeCodeTable(CodeTable* pCodeTable, unsigned int colorTableLength)
{
	unsigned int i = 0;
	free(pCodeTable->content[0]);
	for (i = 0; i < colorTableLength+2; ++i)
	{
		pCodeTable->content[i] = NULL;
		pCodeTable->length[i] = 0;
	}
	free(pCodeTable->content[colorTableLength + 2]);
	pCodeTable->content[colorTableLength + 2] = NULL;
	free(pCodeTable->content);
	pCodeTable->content = NULL;
	free(pCodeTable->length);
	pCodeTable->length = NULL;
}

//向code table添加code，返回添加的code值
unsigned int AppendCode( CodeTable* pCodeTable,
    const unsigned char* content, const int length )
{
    int i=0;
	assert(length <= MAX_CODE_CONTENT_LENGHTH);
    for (i=0;i<length;++i)
    {
        pCodeTable->content[pCodeTable->count][i] = content[i];
    }
    pCodeTable->length[pCodeTable->count] = length;
    ++pCodeTable->count;
	return pCodeTable->count;
}

int CodeInTable(CodeTable* pCodeTable, unsigned int code)
{
	//code从0开始计数
	//CodeTable::count记录的是code总数，CodeTable中最后一个code的值就是CodeTable::count-1
	if (code>=pCodeTable->count)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

void GetCodeContent(CodeTable* pCodeTable, unsigned int code, unsigned char** pContent, unsigned int *pLength)
{
	assert(CodeInTable(pCodeTable, code)==1);
	*pContent = pCodeTable->content[code];
	*pLength = pCodeTable->length[code];
}

void CodeTableToString( const CodeTable* pCodeTable, char* stringBuffer )
{
    unsigned int i=0,j=0;

    sprintf(stringBuffer, "Code table含%d个code:\n", pCodeTable->count);
    for (i=0; i<pCodeTable->count; ++i)
    {
        sprintf(stringBuffer+strlen(stringBuffer),
            "    #%d ", i);
        for (j=0; j<pCodeTable->length[i]; ++j)
        {
            sprintf(stringBuffer+strlen(stringBuffer),
                "%d,", pCodeTable->content[i][j]);
        }
        stringBuffer[strlen(stringBuffer)-1] = '\n';

    }
}

unsigned int GetCode( unsigned char* CodeData, unsigned int bitOffset, unsigned int codeLength )
{
    unsigned int byteOffset = bitOffset / 8;
    unsigned int offset = bitOffset % 8;
    unsigned char* current = CodeData + byteOffset;
    unsigned char bitPart[3] = {0};
    unsigned char bitPartLength[3] = {0};
    unsigned int code = 0;

    //检测code的范围是否超过offset所在字节的下1字节的范围
    if (16-offset < codeLength)
	{
        //要在offset所在字节获取的bit范围为
        //[offset, 7]
        bitPart[0] = GetBitRangeFromByte(*current, offset, 7);
        //获取的bit长度为7-offset+1
        bitPartLength[0] = 8 - offset;
        //要在offset所在字节的下一个字节获取的bit范围为
        //[0,7](所有位)
        bitPart[1] = *(current+1);
        //获取的bit长度为8
        bitPartLength[1] = 8;
        //要在offset所在字节的下第二个字节获取的bit范围为
        //[0,codeLength-8-(8-offset)-1]
        //即
        //[0,codeLength+offset-17]
        //获取的bit长度为codeLength+offset-17 + 1
        bitPart[2] = GetBitRangeFromByte(*(current+2), 0, codeLength+offset-17);
        bitPartLength[2] = codeLength+offset-16;
        //合并所有bit段
        code = CombineByte(bitPart[0], bitPartLength[0], bitPart[1], bitPartLength[1]);
        code = CombineByte(code, bitPartLength[0] + bitPartLength[1], bitPart[2], bitPartLength[2]);
    }
    //检测code的范围是否超过offset所在字节的范围
    else if (8-offset < codeLength)
    {
        //要在offset所在字节获取的bit范围为
        //[offset, 7]
        bitPart[0] = GetBitRangeFromByte(*current, offset, 7);
        //获取的bit长度为7-offset+1
        bitPartLength[0] = 8 - offset;
        //要在offset所在字节的下一个字节获取的bit范围为
        //[0,codeLength-(8-offset)-1]
        //即
        //[0,codeLength+offset-9]
        bitPart[1] = GetBitRangeFromByte(*(current+1), 0, codeLength+offset-9);
        //获取的bit长度为codeLength+offset-9+1
        bitPartLength[1] = codeLength+offset-9+1;
        code = CombineByte(bitPart[0], bitPartLength[0], bitPart[1], bitPartLength[1]);
    }
    else//code的范围不超过offset所在字节的范围
    {
        //获取[offset,offset+codeLength-1]的bit，即为code
        code = GetBitRangeFromByte(*current, offset, offset+codeLength-1);
    }
    return code;
}
#pragma endregion
