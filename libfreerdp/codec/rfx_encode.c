/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Encode
 *
 * Copyright 2011 Vic Lee
 * Copyright 2011 Norbert Federa <norbert.federa@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/collections.h>

#include <freerdp/primitives.h>

#include "rfx_types.h"
#include "rfx_rlgr.h"
#include "rfx_differential.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"

#include "rfx_encode.h"

#define MINMAX(_v, _l, _h) ((_v) < (_l) ? (_l) : ((_v) > (_h) ? (_h) : (_v)))

static void rfx_encode_format_rgb(const BYTE* rgb_data, int width, int height, int rowstride,
                                  UINT32 pixel_format, const BYTE* palette, INT16* r_buf,
                                  INT16* g_buf, INT16* b_buf)
{
	int x = 0;
	int y = 0;
	int x_exceed = 0;
	int y_exceed = 0;
	const BYTE* src = NULL;
	const UINT32* src_32 = NULL;
	const UINT16* src_16 = NULL;
	INT16 r = 0;
	INT16 g = 0;
	INT16 b = 0;
	INT16* r_last = NULL;
	INT16* g_last = NULL;
	INT16* b_last = NULL;
	x_exceed = 64 - width;
	y_exceed = 64 - height;

	for (y = 0; y < height; y++)
	{
		src = rgb_data + y * rowstride;

		switch (pixel_format)
		{
			case PIXEL_FORMAT_BGRX32:
			case PIXEL_FORMAT_BGRA32:
				for (x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
					src++;
				}

				break;

			case PIXEL_FORMAT_XBGR32:
			case PIXEL_FORMAT_ABGR32:
				for (x = 0; x < width; x++)
				{
					src++;
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_RGBX32:
			case PIXEL_FORMAT_RGBA32:
				for (x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
					src++;
				}

				break;

			case PIXEL_FORMAT_XRGB32:
			case PIXEL_FORMAT_ARGB32:
				for (x = 0; x < width; x++)
				{
					src++;
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_BGR24:
				for (x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_RGB24:
				for (x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_BGR16:
				for (x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					*g_buf++ = (INT16)((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					*r_buf++ = (INT16)((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					src += 2;
				}

				break;

			case PIXEL_FORMAT_RGB16:
				for (x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					*g_buf++ = (INT16)((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					*b_buf++ = (INT16)((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					src += 2;
				}

				break;

			case PIXEL_FORMAT_RGB8:
				if (!palette)
					break;

				for (x = 0; x < width; x++)
				{
					int shift = 0;
					BYTE idx = 0;
					shift = (7 - (x % 8));
					idx = ((*src) >> shift) & 1;
					idx |= (((*(src + 1)) >> shift) & 1) << 1;
					idx |= (((*(src + 2)) >> shift) & 1) << 2;
					idx |= (((*(src + 3)) >> shift) & 1) << 3;
					idx *= 3;
					*r_buf++ = (INT16)palette[idx];
					*g_buf++ = (INT16)palette[idx + 1];
					*b_buf++ = (INT16)palette[idx + 2];

					if (shift == 0)
						src += 4;
				}

				break;

			case PIXEL_FORMAT_A4:
				if (!palette)
					break;

				for (x = 0; x < width; x++)
				{
					int idx = (*src) * 3;
					*r_buf++ = (INT16)palette[idx];
					*g_buf++ = (INT16)palette[idx + 1];
					*b_buf++ = (INT16)palette[idx + 2];
					src++;
				}

				break;

			default:
				break;
		}

		/* Fill the horizontal region outside of 64x64 tile size with the right-most pixel for best
		 * quality */
		if (x_exceed > 0)
		{
			r = *(r_buf - 1);
			g = *(g_buf - 1);
			b = *(b_buf - 1);

			for (x = 0; x < x_exceed; x++)
			{
				*r_buf++ = r;
				*g_buf++ = g;
				*b_buf++ = b;
			}
		}
	}

	/* Fill the vertical region outside of 64x64 tile size with the last line. */
	if (y_exceed > 0)
	{
		r_last = r_buf - 64;
		g_last = g_buf - 64;
		b_last = b_buf - 64;

		while (y_exceed > 0)
		{
			CopyMemory(r_buf, r_last, 64 * sizeof(INT16));
			CopyMemory(g_buf, g_last, 64 * sizeof(INT16));
			CopyMemory(b_buf, b_last, 64 * sizeof(INT16));
			r_buf += 64;
			g_buf += 64;
			b_buf += 64;
			y_exceed--;
		}
	}
}

/* rfx_encode_rgb_to_ycbcr code now resides in the primitives library. */

static void rfx_encode_component(RFX_CONTEXT* context, const UINT32* quantization_values,
                                 INT16* data, BYTE* buffer, int buffer_size, int* size)
{
	INT16* dwt_buffer = NULL;
	dwt_buffer = BufferPool_Take(context->priv->BufferPool, -1); /* dwt_buffer */
	PROFILER_ENTER(context->priv->prof_rfx_encode_component)
	PROFILER_ENTER(context->priv->prof_rfx_dwt_2d_encode)
	context->dwt_2d_encode(data, dwt_buffer);
	PROFILER_EXIT(context->priv->prof_rfx_dwt_2d_encode)
	PROFILER_ENTER(context->priv->prof_rfx_quantization_encode)
	context->quantization_encode(data, quantization_values);
	PROFILER_EXIT(context->priv->prof_rfx_quantization_encode)
	PROFILER_ENTER(context->priv->prof_rfx_differential_encode)
	rfx_differential_encode(data + 4032, 64);
	PROFILER_EXIT(context->priv->prof_rfx_differential_encode)
	PROFILER_ENTER(context->priv->prof_rfx_rlgr_encode)
	*size = context->rlgr_encode(context->mode, data, 4096, buffer, buffer_size);
	PROFILER_EXIT(context->priv->prof_rfx_rlgr_encode)
	PROFILER_EXIT(context->priv->prof_rfx_encode_component)
	BufferPool_Return(context->priv->BufferPool, dwt_buffer);
}

void rfx_encode_rgb(RFX_CONTEXT* context, RFX_TILE* tile)
{
	union
	{
		const INT16** cpv;
		INT16** pv;
	} cnv;
	BYTE* pBuffer = NULL;
	INT16* pSrcDst[3];
	int YLen = 0;
	int CbLen = 0;
	int CrLen = 0;
	UINT32* YQuant = NULL;
	UINT32* CbQuant = NULL;
	UINT32* CrQuant = NULL;
	primitives_t* prims = primitives_get();
	static const prim_size_t roi_64x64 = { 64, 64 };

	if (!(pBuffer = (BYTE*)BufferPool_Take(context->priv->BufferPool, -1)))
		return;

	YLen = CbLen = CrLen = 0;
	YQuant = context->quants + (tile->quantIdxY * 10);
	CbQuant = context->quants + (tile->quantIdxCb * 10);
	CrQuant = context->quants + (tile->quantIdxCr * 10);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* y_r_buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* cb_g_buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* cr_b_buffer */
	PROFILER_ENTER(context->priv->prof_rfx_encode_rgb)
	PROFILER_ENTER(context->priv->prof_rfx_encode_format_rgb)
	rfx_encode_format_rgb(tile->data, tile->width, tile->height, tile->scanline,
	                      context->pixel_format, context->palette, pSrcDst[0], pSrcDst[1],
	                      pSrcDst[2]);
	PROFILER_EXIT(context->priv->prof_rfx_encode_format_rgb)
	PROFILER_ENTER(context->priv->prof_rfx_rgb_to_ycbcr)

	cnv.pv = pSrcDst;
	prims->RGBToYCbCr_16s16s_P3P3(cnv.cpv, 64 * sizeof(INT16), pSrcDst, 64 * sizeof(INT16),
	                              &roi_64x64);
	PROFILER_EXIT(context->priv->prof_rfx_rgb_to_ycbcr)
	/**
	 * We need to clear the buffers as the RLGR encoder expects it to be initialized to zero.
	 * This allows simplifying and improving the performance of the encoding process.
	 */
	ZeroMemory(tile->YData, 4096);
	ZeroMemory(tile->CbData, 4096);
	ZeroMemory(tile->CrData, 4096);
	rfx_encode_component(context, YQuant, pSrcDst[0], tile->YData, 4096, &YLen);
	rfx_encode_component(context, CbQuant, pSrcDst[1], tile->CbData, 4096, &CbLen);
	rfx_encode_component(context, CrQuant, pSrcDst[2], tile->CrData, 4096, &CrLen);
	tile->YLen = (UINT16)YLen;
	tile->CbLen = (UINT16)CbLen;
	tile->CrLen = (UINT16)CrLen;
	PROFILER_EXIT(context->priv->prof_rfx_encode_rgb)
	BufferPool_Return(context->priv->BufferPool, pBuffer);
}
