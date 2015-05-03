#include "CppUnitTest.h"
extern "C"
{
#include "cGIF.h"
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace cGIFTest
{		
	TEST_CLASS(AppendFrameTest)
	{
	public:
		TEST_METHOD(Test_cGif_expand_frames)
		{
			//Arrange
			unsigned k = 0;
			unsigned frameCount = 0;
			cGif_State_Frame* frameState = nullptr;
			unsigned* frameTimeInMS = nullptr;
			unsigned char** indexes = nullptr;
			unsigned* image_position_x = nullptr;
			unsigned* image_position_y = nullptr;
			unsigned* imageWidth = nullptr;
			unsigned* imageHeight = nullptr;
			unsigned char** palette = nullptr;
			unsigned char* palette_color_count = nullptr;
			unsigned char* transparent_color_index = nullptr;
			char* flag = nullptr;

			//Act
			_cGif_expand_frames(
				&k,
				frameCount,
				&indexes,
				&frameState);

			frameCount += 2;//now 2 frame

			//Assert
			Assert::IsTrue(k == 16);

			//Act
			_cGif_expand_frames(
				&k,
				frameCount,
				&indexes,
				&frameState);

			//Assert
			Assert::IsTrue(k == 16);

			frameCount += 14;//now 16 frame

			//Act
			_cGif_expand_frames(
				&k,
				frameCount,
				&indexes,
				&frameState);

			//Assert
			Assert::IsTrue(k == 32);

		}

	};
}