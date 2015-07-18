/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#pragma once
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#define MAX_CODE_CONTENT_LENGHTH 512 //Max length of the content of a code
//TODO
//ȥ��������裡
//���������code�ĵ�Content�ĳ��ȳ���MAX_CODE_CONTENT_LENGHTHʱ����������

/*
cGIF������
*/
typedef enum _error
{
	cGif_Success,
	cGif_File_Not_Exist,
	cGif_GCT_Not_Exist,
	cGif_Unknown_Introducer_Or_Label,
	//cGif_Next_Not_Clear_Or_EOI,//Deferred
	cGif_Encounter_EOI_Before_ImageData_Completed,
	cGif_Too_Many_Frames
} cGif_Error;

typedef struct _Flag
{
	unsigned char backeground : 1;
	unsigned char transparent : 1;
} cGif_Flag;

#pragma region �����ṹ����
/* the header */
struct _Header
{
	char Signature[3];
	char Version[3];
};
typedef struct _Header Header;

struct _LogicalScreenDescriptor
{
	/* Logical Screen Descriptor */
	unsigned short LogicalScreenWidth;
	unsigned short LogicalScreenHeight;
	unsigned int GCTFlag : 1;	//GCT Flag
	unsigned int ColorResolution : 3;
	unsigned int SortFlag : 1;
	unsigned int SizeOfGCT : 3;
	unsigned char BackgroundColorIndex;
	unsigned char PixelAspectRatio;
};
typedef struct _LogicalScreenDescriptor LogicalScreenDescriptor;

/* Color
	����Global color table
		Local color table
*/
struct _Color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
};
typedef struct _Color Color;

/* Graphics Control Extension */
struct _GraphicsControlExtension
{
	unsigned char Introducer;
	unsigned char Label;
	unsigned char BlockSize;
	int Reserved : 3;
	int DisposalMethod : 3;
	unsigned int UserInputFlag : 1;
	unsigned int TransparentColorFlag : 1;
	unsigned short DelayTime;
	unsigned char TransparentColorIndex;
	unsigned char BlockTerminator;
};
typedef struct _GraphicsControlExtension GraphicsControlExtension;

/*
* Image Data
*/

struct _ColorTable
{
    unsigned int ColorCount;
    Color* vColor;
};
typedef struct _ColorTable ColorTable;

/* Image Descriptor */
struct _ImageDescriptor
{
	unsigned char Separator;		//����0x2C
	unsigned short LeftPosition;	//ͼ������ص�λ��
	unsigned short TopPosition;		//ͼ���ϱ��ص�λ��
	unsigned short Width;			//ͼ����
	unsigned short Height;			//ͼ��߶�
	unsigned int LocalColorTableFlag : 1;	//Local color table�Ƿ����
	unsigned int InterlaceFlag : 1;			//ͼ���Ƿ��ǽ����
	unsigned int SortFlag : 1;				//Local color table�Ƿ��������
	unsigned int Reserved : 2;				//����λ
	unsigned int SizeOfLocalColorTable : 3;	//Local color table�Ĵ�С
};
typedef struct _ImageDescriptor ImageDescriptor;

struct _ApplicationExtension
{
    unsigned char Introducer;
    unsigned char Label;
    unsigned char BlockSize;
    unsigned char Identifier[8];
    unsigned char AuthCode[3];
    unsigned int SubBlockCount;//<=256
    unsigned char* SubBlock[256];
    unsigned char SubBlockSize[256];
	unsigned char BlockTerminator;
};
typedef struct _ApplicationExtension ApplicationExtension;

struct _CommentExtension
{
    unsigned char Introducer;
    unsigned char Label;
    unsigned int SubBlockCount;//<=256
    unsigned char* SubBlock[256];
    unsigned char SubBlockSize[256];
    unsigned char BlockTerminator;
};
typedef struct _CommentExtension CommentExtension;

struct _ImageData
{
    unsigned char LZWMinCodeSize;
    unsigned int SubBlockCount;//<=4096
    unsigned char** SubBlock;
    unsigned char* SubBlockSize;
    unsigned char BlockTerminator;
};
typedef struct _ImageData ImageData;

#pragma endregion

typedef struct _cGif_State_Global
{
	LogicalScreenDescriptor lsd;		//�߼���Ļ������
	ColorTable gct;						//ȫ��Color table
	ApplicationExtension ae;			//Ӧ�ó��������չ(���ڵ�ͼ���н����ڡ�NETSCAPE2.0��������չ)
	cGif_Error error;					//����
} cGif_State_Global;

typedef struct _cGif_State_Frame
{
	ColorTable lct;						//����Color table
	GraphicsControlExtension gce;		//ͼ�������չ
	ImageDescriptor imgDesc;			//ͼ��������
} cGif_State_Frame;

#pragma region �����ṹ������������

#pragma region SubBlock������������
//Release allocated memory for Sub Data
void ReleaseSubBlock(ImageData* pImgData, ApplicationExtension* pAppExt, CommentExtension* pCE);

//��ImageData�е�Subblock��������Ϊһ����
void CombineSubBlocks(ImageData* pImgData, unsigned char** pCodeData, unsigned int* pCodeDataByteSize);
#pragma endregion

#pragma region ��ѹ����غ�������
//��code���ݿ�����ȡcode����
cGif_Error ExtractCodeArray(unsigned char* CodeData, unsigned int CodeDataByteSize,
	unsigned int LZWMinCodeSize,
	unsigned int** ppCodeArray, unsigned int* pCodeArrayCount);

//��code���з���Ϊ��ɫ����
cGif_Error TranslateCodeArray(unsigned char** ppColorIndexArray,
	ImageData* pImgData, ImageDescriptor* pImgDesc, unsigned int* CodeArray, unsigned int CodeArrayCount);

//��ͼ�����ݽ��н�ѹ��
cGif_Error DecompressImageData(unsigned char** out,
	ImageData* pImgData, ImageDescriptor* pImgDesc);
#pragma endregion

#pragma region �����ṹ��ȡ��������
//Read Header
void ReadHeader(Header* pHeader, FILE* file);

//Read Logical Screen Descriptor
void ReadLCD(LogicalScreenDescriptor* pLSD, FILE* file);

//Read Glocal Color Table
void ReadGCT(LogicalScreenDescriptor* pLSD, ColorTable* pGCT, FILE* file);

//Read Graphics Control Extension
void ReadGCE(GraphicsControlExtension* pGCE, FILE* file);

//Read Image Descriptor
void ReadImageDesc(ImageDescriptor* pImgDesc, FILE* file);

//Read Local Color Table
void ReadLCT(ImageDescriptor* pImgDesc, ColorTable* pLCT, FILE* file);

//Read Plain Text Extension
void ReadPTE(FILE* file);

void ReadImageData(ImageData* pImgData, FILE* file);

//Read Graphic Rendering Block
void ReadGRB(ImageData* pImgData, cGif_State_Frame* state, FILE* file);

//Read Application Extension
void ReadAE(ApplicationExtension* pAppExt, FILE* file);

//Read Comment Extension
void ReadCE(CommentExtension* pCommentExt, FILE* file);
//Skip Comment Extension
void SkipCE(FILE* file);
#pragma endregion

#pragma endregion

#pragma region �����ṹ��ӡ��������
void HeaderToString(Header* pHeader, char* buffer);
void LSDToString(LogicalScreenDescriptor* pLSD, char* buffer);
void ImageDescriptorToString(ImageDescriptor* pDesc, char* buffer);
void GCEToString(GraphicsControlExtension* pGCE, char* buffer);
void AEToString(ApplicationExtension* pAE, char* buffer);
void CEToString(CommentExtension* pCE, char* stringBuffer);
void ImageDataToString(ImageData* pImgData, char* stringBuffer);
void GCTToString(const ColorTable* GCT, char* stringBuffer );
void LCTToString(const ColorTable* LCT, char* stringBuffer);
#pragma endregion

#pragma region Code table���弰��غ�������
//Code table����
//value
//  code #i, i����code��value
//content
//  ��ά����
//      content[i]��ʾcode #i��content
//      content[i][j]��ʾcode #i��content�ĸ�λ���j����
//length
//  һά����
//      length[i]��ʾcode #i��content����
//count
//  ��ʾcode table�е�code����
//
//���7��code��(code #7, content 112, length 3)
//  ��CodeTable�еı�ʾ����
//      content[7] = {1,1,2}; length[7] = 3;
//  ���ߣ�ͬ���ģ�
//      content[7][0] = 1;
//      content[7][1] = 1;
//      content[7][2] = 2;
//      length[7] = 3;
//
//value�ķ�ΧΪ[2^(2-1),2^(12-1)]��[2, 4096]
//      ������Clear Code �� EOI Code����[4, 4098]
//content(��ɫ��������)�ĳ��ȵķ�ΧΪ[1,INT_MAX]
//content������(��ɫ����ֵ)�ķ�ΧΪ[0,255]
struct _CodeTable
{
	unsigned char** content;//4098, 2 << (12-1) + 2;//4096 + 2
	unsigned* length;
    unsigned int count;
};
typedef struct _CodeTable CodeTable;

void InitCodeTable(CodeTable* pCodeTable, unsigned short mincodeSize);
unsigned int AppendCode(CodeTable* pCodeTable,
    const unsigned char* content, const int length);
int CodeInTable(CodeTable* pCodeTable, unsigned int code);
void GetCodeContent(CodeTable* pCodeTable, unsigned int code, unsigned char** pContent, unsigned int *pLength);
void CodeTableToString(const CodeTable* pCodeTable, char* stringBuffer);
unsigned int GetCode( unsigned char* CodeData, unsigned int bitOffset,
    unsigned int codeLength );
void FreeCodeTable(CodeTable* pCodeTable, unsigned int InitialColorTableLength);
void ResetCodeTable(CodeTable* pCodeTable, unsigned short colorTableLength);
#pragma endregion