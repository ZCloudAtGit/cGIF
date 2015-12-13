#include "CppUnitTest.h"
extern "C"
{
#include "cGIF.h"
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace cGIFTest
{
	TEST_CLASS(EncodeTest)
	{
	public:

		TEST_METHOD(Test_WriteLCD)
		{
			//Arrange
			LogicalScreenDescriptor LSD = { 0 };
			LSD.GCTFlag = 1;
			LSD.ColorResolution = 7;
			LSD.SortFlag = 0;
			LSD.SizeOfGCT = 6;
			LogicalScreenDescriptor* pLSD = &LSD;
			unsigned char byteBuf = 0;

			//Act
			byteBuf |= (0x80 & (pLSD->GCTFlag << 7));
			byteBuf |= (0x70 & (pLSD->ColorResolution << 4));
			byteBuf |= (0x08 & (pLSD->SortFlag << 3));
			byteBuf |= (0x07 & pLSD->SizeOfGCT);

			//Assert
			Assert::IsTrue(pLSD->GCTFlag == ((byteBuf & 0x80) >> 7));
			Assert::IsTrue(pLSD->ColorResolution == ((byteBuf & 0x70) >> 4));
			Assert::IsTrue(pLSD->SortFlag == ((byteBuf & 0x08) >> 3));
			Assert::IsTrue(pLSD->SizeOfGCT == (byteBuf & 0x07));
		}

		TEST_METHOD(Test_WriteGCE)
		{
			//Arrange
			GraphicsControlExtension GCE;
			GCE.Reserved = 0;
			GCE.DisposalMethod = 1;
			GCE.UserInputFlag = 0;
			GCE.TransparentColorFlag = 1;
			GraphicsControlExtension* pGCE = &GCE;
			unsigned char byteBuf = 0;

			//Act
			byteBuf |= (0x70 & (pGCE->Reserved << 5));
			byteBuf |= (0x1c & (pGCE->DisposalMethod << 2));
			byteBuf |= (0x02 & (pGCE->UserInputFlag << 1));
			byteBuf |= (0x01 & pGCE->TransparentColorFlag);

			//Assert
			Assert::IsTrue(pGCE->Reserved == ((byteBuf & 0x70) >> 5));
			Assert::IsTrue(pGCE->DisposalMethod == ((byteBuf & 0x1c) >> 2));
			Assert::IsTrue(pGCE->UserInputFlag == ((byteBuf & 0x02) >> 1));
			Assert::IsTrue(pGCE->TransparentColorFlag == (byteBuf & 0x01));
		}

		TEST_METHOD(Test_WriteImageDesc)
		{
			//Arrange
			ImageDescriptor ImgDesc;
			ImgDesc.LocalColorTableFlag = 1;
			ImgDesc.InterlaceFlag = 1;
			ImgDesc.SortFlag = 0;
			ImgDesc.Reserved = 0;
			ImgDesc.SizeOfLocalColorTable = 7;
			ImageDescriptor* pImgDesc = &ImgDesc;
			unsigned char byteBuf = 0;

			//Act
			byteBuf |= (0x80 & (pImgDesc->LocalColorTableFlag << 7));
			byteBuf |= (0x40 & (pImgDesc->InterlaceFlag << 6));
			byteBuf |= (0x20 & (pImgDesc->SortFlag << 5));
			byteBuf |= (0x18 & (pImgDesc->SortFlag << 3));
			byteBuf |= (0x07 & pImgDesc->SizeOfLocalColorTable);

			//Assert
			Assert::IsTrue(pImgDesc->LocalColorTableFlag == (byteBuf & 0x80) >> 7);
			Assert::IsTrue(pImgDesc->InterlaceFlag == (byteBuf & 0x40) >> 6);
			Assert::IsTrue(pImgDesc->SortFlag == (byteBuf & 0x20) >> 5);
			Assert::IsTrue(pImgDesc->Reserved == (byteBuf & 0x18) >> 3);
			Assert::IsTrue(pImgDesc->SizeOfLocalColorTable == (byteBuf & 0x07));
		}
	};
}