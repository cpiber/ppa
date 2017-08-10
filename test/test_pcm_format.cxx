/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "test_pcm_all.hxx"
#include "test_pcm_util.hxx"
#include "pcm/PcmFormat.hxx"
#include "pcm/PcmDither.hxx"
#include "pcm/PcmUtils.hxx"
#include "pcm/PcmBuffer.hxx"
#include "pcm/SampleFormat.hxx"

void
PcmFormatTest::TestFormat8to16()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int8_t, N>();

	PcmBuffer buffer;

	PcmDither dither;
	auto d = pcm_convert_to_16(buffer, dither, SampleFormat::S8, src);
	CPPUNIT_ASSERT_EQUAL(N, d.size);

	for (size_t i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(int(src[i]), d[i] >> 8);
}

void
PcmFormatTest::TestFormat16to24()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer;

	auto d = pcm_convert_to_24(buffer, SampleFormat::S16, src);
	CPPUNIT_ASSERT_EQUAL(N, d.size);

	for (size_t i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(int(src[i]), d[i] >> 8);
}

void
PcmFormatTest::TestFormat16to32()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer;

	auto d = pcm_convert_to_32(buffer, SampleFormat::S16, src);
	CPPUNIT_ASSERT_EQUAL(N, d.size);

	for (size_t i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(int(src[i]), d[i] >> 16);
}

void
PcmFormatTest::TestFormatFloat()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer1, buffer2;

	auto f = pcm_convert_to_float(buffer1, SampleFormat::S16, src);
	CPPUNIT_ASSERT_EQUAL(N, f.size);

	for (size_t i = 0; i != f.size; ++i) {
		CPPUNIT_ASSERT(f[i] >= -1.);
		CPPUNIT_ASSERT(f[i] <= 1.);
	}

	PcmDither dither;

	auto d = pcm_convert_to_16(buffer2, dither,
				   SampleFormat::FLOAT,
				   f.ToVoid());
	CPPUNIT_ASSERT_EQUAL(N, d.size);

	for (size_t i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(src[i], d[i]);

	/* check if clamping works */
	float *writable = const_cast<float *>(f.data);
	*writable++ = 1.01;
	*writable++ = 10;
	*writable++ = -1.01;
	*writable++ = -10;

	d = pcm_convert_to_16(buffer2, dither,
			      SampleFormat::FLOAT,
			      f.ToVoid());
	CPPUNIT_ASSERT_EQUAL(N, d.size);

	CPPUNIT_ASSERT_EQUAL(32767, int(d[0]));
	CPPUNIT_ASSERT_EQUAL(32767, int(d[1]));
	CPPUNIT_ASSERT_EQUAL(-32768, int(d[2]));
	CPPUNIT_ASSERT_EQUAL(-32768, int(d[3]));

	for (size_t i = 4; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(src[i], d[i]);
}