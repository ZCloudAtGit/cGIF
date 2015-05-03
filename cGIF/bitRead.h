/*
(c) 2014-2015 cloud
Website: http:\\cloud.s2.jutuo.net
Email: zwcloud@yeah.net

This code is licensed under the GPL license. See Licence.
*/

#pragma once

unsigned char GetBitRangeFromByte(unsigned char theByte, int start, int end);
unsigned int CombineByte(unsigned int lowByte, unsigned int lowByteLength,
	unsigned int highByte, unsigned int highByteLength);