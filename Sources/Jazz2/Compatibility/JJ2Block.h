﻿#pragma once

#include "../../Common.h"
#include "../../nCine/IO/IFileStream.h"

using namespace nCine;

namespace Jazz2::Compatibility
{
	class JJ2Block
	{
	public:
		JJ2Block(const std::unique_ptr<IFileStream>& s, int32_t length, int32_t uncompressedLength = 0);

		void SeekTo(int32_t offset);
		void DiscardBytes(int32_t length);

		bool ReadBool();
		uint8_t ReadByte();
		int16_t ReadInt16();
		uint16_t ReadUInt16();
		int32_t ReadInt32();
		uint32_t ReadUInt32();
		int32_t ReadUint7bitEncoded();
		float ReadFloat();
		float ReadFloatEncoded();
		void ReadRawBytes(uint8_t* dst, int32_t length);
		StringView ReadString(int32_t length, bool trimToNull);

		bool ReachedEndOfStream()
		{
			return (_offset == INT32_MAX);
		}

	private:
		std::unique_ptr<uint8_t[]> _buffer;
		int32_t _length;
		int32_t _offset;
	};
}