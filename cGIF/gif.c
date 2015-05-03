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

#pragma region �����ṹ��������

#pragma region SubBlock������������
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

//��ImageData�е�Subblock��������Ϊһ����
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
	//��ӡ����subblock
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

#pragma region ��ѹ����غ�������
//��code���ݿ�����ȡcode����
cGif_Error ExtractCodeArray(unsigned char* CodeData, unsigned int CodeDataByteSize,
	unsigned int LZWMinCodeSize,
	unsigned int** ppCodeArray, unsigned int* pCodeArrayCount)
{
	unsigned int codeLength = LZWMinCodeSize + 1;//��ʼCode����
	unsigned int offset = 0;
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
	unsigned int count = 0;
#endif
	unsigned int n = 0;
	unsigned int code = 0;

	unsigned int ClearCode = 1 << LZWMinCodeSize;	//Clear Code
	unsigned int EOICode = ClearCode + 1;		//EOI Code

#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
	printf("Code����:\n");
#endif
	while (code != EOICode)
	{
		//��ȡ1��code
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
			printf("����Clear code(#%03x)������code����Ϊclear code�ĳ���\n", code);
#endif
			codeLength = LZWMinCodeSize + 1;
			n = 1;//����(�������clear code����)
			continue;
		}

		//����´ζ�ȡcodeʱ���Ƿ���Ҫ���ӳ���
		if (n == 1 << (codeLength - 1))
		{
			if (codeLength == 12)//����Ϊ12ʱ
			{
#ifdef Dump_Processing_Procedure_of_Extracting_Code_Sequence
				printf("code���Ƚ�����12��code���Ƚ��̶�Ϊ12ֱ����ȡ��bit����Ϊ12��clear code(#%03x)\n", ClearCode);
#endif
			}
			else
			{
				++codeLength;//���ӳ���
				n = 0;//����
			}
		}
	}
	//����Ƿ���������1�ֽڣ����ǵ������и�λ��0��������������һ��code(EOI code)����ʱoffset���п��ܲ���codedataĩβ��
	//offset==(CodeDataByteSize-1)*8ʱ��offsetָ�����һ�ֽڵĵ�1λ����Ҳ�ǲ��Եģ���Ϊ��1λ��Ԥ������һ�ζ�ȡ��code�ĵ�1λ
	if (offset <= (CodeDataByteSize - 1) * 8)
	{
		//printf("����ͼ������δ��ȡ��ɣ���������EOI code(#%d)\n", EOICode);
		return cGif_Encounter_EOI_Before_ImageData_Completed;
	}
	//printf("��ȡ����EOI code��code���н���\n");
	return cGif_Success;
}

//��code���з���Ϊ��ɫ����
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
	//������ɫ�������ڴ��С
	*ppColorIndexArray = (unsigned char*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned char));

	InitialColorTableLength = 2 << (pImgData->LZWMinCodeSize - 1);
	InitCodeTable(&codeTable, InitialColorTableLength);
	ColorIndexArrayCount = 0;

	assert(CodeArray[0] == ClearCode);//��һ��code������Clear code
	assert(CodeInTable(&codeTable, CodeArray[1]) == 1);//�ڶ���code������code table��
	//���ڶ���code��content���ӵ���ɫ����������
	code = CodeArray[1];
	//��ȡcode��Content
	GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
	//��content���ӵ���ɫ����������
	for (j = 0; j < CodeContentLength; ++j)
	{
		(*ppColorIndexArray)[ColorIndexArrayCount] = CodeContent[j];
		++ColorIndexArrayCount;
	}

	for (i = 2; i < CodeArrayCount; ++i)//������һ��Clear Code�͵ڶ���code
	{
		unsigned char k = 0;
		unsigned int newcode = 0;
		unsigned int newcodeContentLength = 0;

		code = CodeArray[i];
		if (code == ClearCode)
		{
#ifdef Dump_Processing_Procedure_of_Translating_Code_Sequence
			printf("����Clear Code(#%d)������code table\n", code);
#endif
			ResetCodeTable(&codeTable, InitialColorTableLength);
			//��Clear Code����Ǹ�code��content���ӵ���ɫ����������
			++i;
			code = CodeArray[i];
			//Clear Code����Ǹ�code������code table��
			assert(CodeInTable(&codeTable, CodeArray[1]) == 1);
			//��ȡcode��Content
			GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
			//��content���ӵ���ɫ����������
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
			printf("����EOI Code(#%d)��code��������\n", code);
#endif
			break;
		}

		//�ж�code�Ƿ���code table��
		if (CodeInTable(&codeTable, code) > 0)//��
		{
			//��ȡcode��Content
			GetCodeContent(&codeTable, code, &CodeContent, &CodeContentLength);
			//��content���ӵ���ɫ����������
			for (j = 0; j < CodeContentLength; ++j)
			{
				(*ppColorIndexArray)[ColorIndexArrayCount] = CodeContent[j];
				++ColorIndexArrayCount;
			}
			//������code
			k = CodeContent[0];	//code��Content�ĵ�һ����ɫ����ֵ
			//��ȡ��һ��code��Content
			GetCodeContent(&codeTable, CodeArray[i - 1], &CodeContent, &CodeContentLength);
			newcodeContentLength = CodeContentLength + 1;
			assert(newcodeContentLength <= MAX_CODE_CONTENT_LENGHTH);
			memcpy(newcodeContent, CodeContent, CodeContentLength*sizeof(CodeContent[0]));
			newcodeContent[newcodeContentLength - 1] = k;//�����ں���
		}
		else//����
		{
			//��ȡ��һ��code��Content
			GetCodeContent(&codeTable, CodeArray[i - 1], &CodeContent, &CodeContentLength);
			//������code
			k = CodeContent[0];
			newcodeContentLength = CodeContentLength + 1;
			assert(newcodeContentLength <= MAX_CODE_CONTENT_LENGHTH);
			memcpy(newcodeContent, CodeContent, CodeContentLength*sizeof(CodeContent[0]));
			newcodeContent[newcodeContentLength - 1] = k;//�����ں���
			//��content���ӵ���ɫ����������
			for (j = 0; j < newcodeContentLength; ++j)
			{
				(*ppColorIndexArray)[ColorIndexArrayCount] = newcodeContent[j];
				++ColorIndexArrayCount;
			}
		}
		//�����ɵ���code��ӵ�code table
		newcode = AppendCode(&codeTable, newcodeContent, newcodeContentLength);
#ifdef Dump_Processing_Procedure_of_Translating_Code_Sequence
		printf("����code #%d (", newcode, codeTable.count);
		for (j = 0; j < newcodeContentLength - 1; ++j)
		{
			printf("%d,", newcodeContent[j]);
		}
		printf("%d", newcodeContent[j]);
		printf(")����%d��code\n", codeTable.count);
#endif
	}

	//����Ϊ����ģ���ԭ��˳��
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

	//��ӡ��ɫ��������
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

//��ͼ�����ݽ��н�ѹ��
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
	//1.�ϲ�����sub blocks�������ݸ��Ƶ��������������ڴ�M��
	begin = GetTickCount();
	CombineSubBlocks(pImgData, &CodeData, &CodeDataByteSize);
	end = GetTickCount();
	printf("CombineSubBlocks: %dms\n", max(0, end - begin));
	//���Եķ�����Ҫ���ٴ�С���ڴ�洢code���У�
	//����Ĺ�����һ��code��ʾһ����ɫ����ʱ������ڴ��С
	CodeArray =
		(unsigned int*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned int));

	//2.���ڴ�M�л�ȡcode����
	begin = GetTickCount();
	ExtractCodeArray(CodeData, CodeDataByteSize, pImgData->LZWMinCodeSize, &CodeArray,
		&CodeArrayCount);
	free(CodeData);
	end = GetTickCount();
	printf("ExtractCodeArray: %dms\n", max(0, end - begin));

	//3.����codeΪ��ɫ�������У���ѹ����
	begin = GetTickCount();
	TranslateCodeArray(out, pImgData, pImgDesc, CodeArray, CodeArrayCount);
	end = GetTickCount();
	printf("TranslateCodeArray: %dms\n", max(0, end - begin));
#else
	//1.�ϲ�����sub blocks�������ݸ��Ƶ��������������ڴ�M��
	CombineSubBlocks(pImgData, &CodeData, &CodeDataByteSize);
	//���Եķ�����Ҫ���ٴ�С���ڴ�洢code���У�
	//����Ĺ�����һ��code��ʾһ����ɫ����ʱ������ڴ��С
	CodeArray =
		(unsigned int*)malloc(pImgDesc->Width*pImgDesc->Height*sizeof(unsigned int));

	//2.���ڴ�M�л�ȡcode����
	error = ExtractCodeArray(CodeData, CodeDataByteSize, pImgData->LZWMinCodeSize,
		&CodeArray, &CodeArrayCount);
	free(CodeData);
	if (error != cGif_Success)
	{
		return error;
	}

	//3.����codeΪ��ɫ�������У���ѹ����
	error = TranslateCodeArray(out, pImgData, pImgDesc, CodeArray, CodeArrayCount);
	if (error != cGif_Success)
	{
		return error;
	}
#endif
	return error;
}
#pragma endregion

#pragma region �����ṹ��ȡ��������
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
		if (pImgData->SubBlockCount == 256)//�Ѿ�����256��SubBlock�ˣ�������
		{
			pImgData->SubBlock = realloc(pImgData->SubBlock, 1024 * sizeof(unsigned char*));
			pImgData->SubBlockSize = realloc(pImgData->SubBlockSize, 1024 * sizeof(unsigned char));
		}
		else if (pImgData->SubBlockCount == 1024)//�Ѿ�����1024��SubBlock�ˣ�������
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

#pragma region �����ṹ��ӡ��������
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

#pragma region Code table��غ�������
//Code��������
void InitCodeTable( CodeTable* pCodeTable, unsigned short colorTableLength )
{
    unsigned int i=0;
	unsigned char* pTmp = NULL;

	pCodeTable->content = malloc(4098 * sizeof(unsigned char*));
	pCodeTable->length = malloc(4098 * sizeof(unsigned));
	pTmp = malloc((colorTableLength + 2)*sizeof(unsigned char));
    //����Color code
    /*
     *Color codes:
     *#0, #1, ..., #(2^i-1), ..., #(2^mincodeSize-1)
    */
    for (i=0; i<colorTableLength; ++i)
    {
        pCodeTable->content[i] = pTmp + i;
        *pCodeTable->content[i] = i;
        pCodeTable->length[i] = 1;
        //Color code��content����value��ͬ��ֻ��һ�����֣����Գ���Ϊ1
        //��Color code (code #3, content 3, length 1)
    }
    //����Clear code
	pCodeTable->content[colorTableLength] = pTmp + colorTableLength;
    *pCodeTable->content[colorTableLength] = 0;//Clear code������������
    pCodeTable->length[colorTableLength] = 1;
    //����EOI code
	pCodeTable->content[colorTableLength + 1] = pTmp + colorTableLength + 1;
    *pCodeTable->content[colorTableLength+1] = 0;//EOI code������������
    pCodeTable->length[colorTableLength+1] = 1;

    pCodeTable->count = colorTableLength+2;

	//ΪCodeTable�ķ�color codeҲ��Clear codeҲ��EOI code��code��ContentԤ����ռ�
	//���裺ÿ��code��Content�ĳ���Ϊ512(MAX_CODE_CONTENT_LENGHTH)
	//TODO: ȥ��������裡���������code�ĵ�Content�ĳ��ȳ���512ʱ����������
	{
		unsigned char* pTmp = malloc(MAX_CODE_CONTENT_LENGHTH * (4098 - (colorTableLength + 2))*sizeof(unsigned char));
		for (i = colorTableLength + 2; i < 4098; i++)
		{
			pCodeTable->content[i] = pTmp + (i - (colorTableLength + 2)) * MAX_CODE_CONTENT_LENGHTH;
		}
		//pTmp == pCodeTable->content[colorTableLength + 2]
		//�ͷ�ʱ
		//free(pCodeTable->content[colorTableLength + 2]);����
	}
}

//����code tableΪ��ʼ״̬
void ResetCodeTable( CodeTable* pCodeTable, unsigned short colorTableLength )
{
	pCodeTable->count = colorTableLength+2;
}

//�ͷ�CodeTable������ڴ�
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

//��code table���code��������ӵ�codeֵ
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
	//code��0��ʼ����
	//CodeTable::count��¼����code������CodeTable�����һ��code��ֵ����CodeTable::count-1
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

    sprintf(stringBuffer, "Code table��%d��code:\n", pCodeTable->count);
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

    //���code�ķ�Χ�Ƿ񳬹�offset�����ֽڵ���1�ֽڵķ�Χ
    if (16-offset < codeLength)
	{
        //Ҫ��offset�����ֽڻ�ȡ��bit��ΧΪ
        //[offset, 7]
        bitPart[0] = GetBitRangeFromByte(*current, offset, 7);
        //��ȡ��bit����Ϊ7-offset+1
        bitPartLength[0] = 8 - offset;
        //Ҫ��offset�����ֽڵ���һ���ֽڻ�ȡ��bit��ΧΪ
        //[0,7](����λ)
        bitPart[1] = *(current+1);
        //��ȡ��bit����Ϊ8
        bitPartLength[1] = 8;
        //Ҫ��offset�����ֽڵ��µڶ����ֽڻ�ȡ��bit��ΧΪ
        //[0,codeLength-8-(8-offset)-1]
        //��
        //[0,codeLength+offset-17]
        //��ȡ��bit����ΪcodeLength+offset-17 + 1
        bitPart[2] = GetBitRangeFromByte(*(current+2), 0, codeLength+offset-17);
        bitPartLength[2] = codeLength+offset-16;
        //�ϲ�����bit��
        code = CombineByte(bitPart[0], bitPartLength[0], bitPart[1], bitPartLength[1]);
        code = CombineByte(code, bitPartLength[0] + bitPartLength[1], bitPart[2], bitPartLength[2]);
    }
    //���code�ķ�Χ�Ƿ񳬹�offset�����ֽڵķ�Χ
    else if (8-offset < codeLength)
    {
        //Ҫ��offset�����ֽڻ�ȡ��bit��ΧΪ
        //[offset, 7]
        bitPart[0] = GetBitRangeFromByte(*current, offset, 7);
        //��ȡ��bit����Ϊ7-offset+1
        bitPartLength[0] = 8 - offset;
        //Ҫ��offset�����ֽڵ���һ���ֽڻ�ȡ��bit��ΧΪ
        //[0,codeLength-(8-offset)-1]
        //��
        //[0,codeLength+offset-9]
        bitPart[1] = GetBitRangeFromByte(*(current+1), 0, codeLength+offset-9);
        //��ȡ��bit����ΪcodeLength+offset-9+1
        bitPartLength[1] = codeLength+offset-9+1;
        code = CombineByte(bitPart[0], bitPartLength[0], bitPart[1], bitPartLength[1]);
    }
    else//code�ķ�Χ������offset�����ֽڵķ�Χ
    {
        //��ȡ[offset,offset+codeLength-1]��bit����Ϊcode
        code = GetBitRangeFromByte(*current, offset, offset+codeLength-1);
    }
    return code;
}
#pragma endregion
