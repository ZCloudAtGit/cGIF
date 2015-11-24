/*
(c) 2014-2015 Zou Wei
Website: http:\\zwcloud.net
Email: zwcloud@yeah.net

This code is licensed under the Apache 2.0 license. See Licence.
*/

#include <stdio.h>
#include <assert.h>

unsigned int GetBitFromByte(unsigned char theByte, int index)
{
    static unsigned char bitmask[8] =
    {
        1, //00000001
        2, //00000010
        4, //00000100
        8, //00001000
        16,//00010000
        32,//00100000
        64,//01000000
        128//10000000
    };
    return theByte & bitmask[index] >> index;
}

//从字节中取出连续的位，得到其值
//theByte 操作的字节
//start 低位
//end 高位
//均从0开始计数
unsigned char GetBitRangeFromByte(unsigned char theByte, int start, int end)
{
    //确认下表无误
    // (dummy)表示占位用，实际计算不使用，因为不方便rshift得到mask后的值
    static unsigned char bitmask[8][8] =
    {
        {
            1,  //00000001,0->0
            3,  //00000011,0->1
            7,  //00000111,0->2
            15, //00001111,0->3
            31, //00011111,0->4
            63, //00111111,0->5
            127,//01111111,0->6
            255,//11111111,0->7
        },
        {
            3,  //00000011,1->0(dummy)
            2,  //00000010,1->1
            6,  //00000110,1->2
            14, //00001110,1->3
            30, //00011110,1->4
            62, //00111110,1->5
            126,//01111110,1->6
            254,//11111110,1->7
        },
        {
            7,  //00000111,2->0(dummy)
            6,  //00000110,2->1(dummy)
            4,  //00000100,2->2
            12, //00001100,2->3
            28, //00011100,2->4
            60, //00111100,2->5
            124,//01111100,2->6
            252,//11111100,2->7
        },
        {
            15, //00001111,3->0(dummy)
            14, //00001110,3->1(dummy)
            12, //00001100,3->2(dummy)
            8,  //00001000,3->3
            24, //00011000,3->4
            56, //00111000,3->5
            120,//01111000,3->6
            248,//11111000,3->7
        },
        {
            31, //00011111,4->0(dummy)
            30, //00011110,4->1(dummy)
            28, //00011100,4->2(dummy)
            24, //00011000,4->3(dummy)
            16, //00010000,4->4
            48, //00110000,4->5
            112,//01110000,4->6
            240,//11110000,4->7
        },
        {
            63, //00111111,5->0(dummy)
            62, //00111110,5->1(dummy)
            60, //00111100,5->2(dummy)
            56, //00111000,5->3(dummy)
            48, //00110000,5->4(dummy)
            32, //00100000,5->5
            96, //01100000,5->6
            224,//11100000,5->7
        },
        {
            127,//01111111,6->0(dummy)
            126,//01111110,6->1(dummy)
            124,//01111100,6->2(dummy)
            120,//01111000,6->3(dummy)
            112,//01110000,6->4(dummy)
            96, //01100000,6->5(dummy)
            64, //01000000,6->6
            192,//11000000,6->7
        },
        {
            255,//11111111,7->0(dummy)
            254,//11111110,7->1(dummy)
            252,//11111100,7->2(dummy)
            248,//11111000,7->3(dummy)
            240,//11110000,7->4(dummy)
            224,//11100000,7->5(dummy)
            192,//11000000,7->6(dummy)
            128,//10000000,7->7
        }
    };

    assert(start<=end);
    assert(start>=0 && start<=7);
    assert(end>=0 && end<=7);
    return (theByte & bitmask[start][end]) >> (start);
}

//使用前请确认你用的是char！
unsigned int CombineByte(
    unsigned int lowByte, unsigned int lowByteLength,
    unsigned int highByte, unsigned int highByteLength )
{
    return (highByte << lowByteLength) + lowByte;
    //returned value length: lowByteLength+highByteLength
}
