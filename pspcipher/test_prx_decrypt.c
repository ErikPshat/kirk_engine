/*
 * This file is part of pspcipher.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "utils.h"
#include "kirk_engine.h"
#include "pspcipher.h"
#include "psp_headers.h"

static int kirk7(u8* prx, u32 size, u32 scramble_code, u32 use_polling)
{
	int ret;

	((u32 *) prx)[0] = 5;
	((u32 *) prx)[1] = 0;
	((u32 *) prx)[2] = 0;
	((u32 *) prx)[3] = scramble_code;
	((u32 *) prx)[4] = size;

	/* ignore use_polling */
	ret = sceUtilsBufferCopyWithRange (prx, size + 20, prx, size + 20, 7);

	return ret;
}

static void prx_xor_key_into(u8 *dstbuf, u32 size, u8 *srcbuf, u8 *xor_key)
{
	u32 i;

	i = 0;

	while (i < size) {
		dstbuf[i] = srcbuf[i] ^ xor_key[i];
		++i;
	}
}

static void prx_xor_key_large(u8 *buf, u32 size, u8 *xor_key)
{
	u32 i;

	i = 0;

	while (i < size) {
		buf[i] = buf[i] ^ xor_key[i];
		++i;
	}
}

static void prx_xor_key(u8 *buf, u32 size, u8 *xor_key1, u8 *xor_key2)
{
	u32 i;

	i =0;
	while (i < size) {
		if (xor_key2 != NULL) {
			buf[i] = buf[i] ^ xor_key2[i&0xf];
		}

		buf[i] = buf[i] ^ xor_key1[i&0xf];
		++i;
	}
}

static void prx_xor_key_single(u8 *buf, u32 size, u8 *xor_key)
{
	return prx_xor_key(buf, size, xor_key, NULL);
}

static u8 buf1[0x150];
static u8 buf2[0x150];
static u8 buf3[0x90];
static u8 buf4[0xb4];
static u8 buf5[0x20];

int uprx_decrypt(user_decryptor *pBlock)
{
	int i;
	if (pBlock == NULL)
		return -1;

	if (pBlock->prx == NULL || pBlock->newsize == NULL)
		return -2;

	if (pBlock->size < 0x160)
		return -202;

	u32 b_0xd4 = 0;

	memset(buf1, 0, sizeof(buf1));
	memset(buf2, 0, sizeof(buf2));
	memset(buf3, 0, sizeof(buf3));
	memset(buf4, 0, sizeof(buf4));
	memset(buf5, 0, sizeof(buf5));

	memcpy(buf1, pBlock->prx, 0x150);

	/** tag mismatched */
	if (memcmp(buf1 + 0xd0, pBlock->tag, 4))
		return -45;

	int ret = -1;

	if (pBlock->type == 3) {
		// zero check: 0xd4 +0x18
	} else if (pBlock->type == 2) {
		// zero check: 0xd4 +0x58
	} else if (pBlock->type == 5) {
		// zero check: 0xd5 +0x57
		b_0xd4 = buf1[0xd4];
	} else if (pBlock->type == 6) {
		// zero check: 0xd4 +0x38
	} else if (pBlock->type == 7) {
		// zero check: 0xd4 +0x38
	}

	u32 elf_size_comp =  *(u32*)(buf1+0xb0);
	*pBlock->newsize = elf_size_comp;

	if (pBlock->size - 50 < elf_size_comp)
		return -206;

	// type 2,3,4,5,6,7
	for (i=0; i<9; i++) {
		memcpy(buf2 + 0x14 + (i << 4), pBlock->key, 0x10);
		buf2[0x14+ (i<<4)] = i;
	}
	if ((ret = kirk7(buf2, 0x90, pBlock->code, pBlock->use_polling)) < 0) {
		return ret;
	}

	memcpy(buf3, buf2, 0x90);

	memcpy(buf2       , buf1 +  0xd0, 0x5C);
	memcpy(buf2 + 0x5c, buf1 + 0x140, 0x10);
	memcpy(buf2 + 0x6c, buf1 + 0x12c, 0x14);
	memcpy(buf2 + 0x80, buf1 +  0x80, 0x30);
	memcpy(buf2 + 0xb0, buf1 +  0xc0, 0x10);
	memcpy(buf2 + 0xc0, buf1 +  0xb0, 0x10);
	memcpy(buf2 + 0xd0, buf1        , 0x80);

	memcpy(buf4 + 0x14, buf2 + 0x5c, 0x60);

	if (kirk7(buf4, 0x60, pBlock->code, pBlock->use_polling) < 0) {
		return -5;
	}
	memcpy(buf2 + 0x5c, buf4, 0x60);

	memcpy(buf4, buf2+0x6c, 0x14);
	memcpy(buf2+0x70, buf2+0x5C, 0x10);

	if (pBlock->type == 6) {
		memcpy(buf5, buf2+0x3C, 0x20);
		memcpy(buf2+0x50, buf5, 0x20);
		memset(buf2+0x18, 0, 0x38);
	} else {
		memset(buf2+0x18, 0, 0x58);
	}

	memcpy(buf2+0x04, buf2, 0x04);
	*((u32*)buf2) = 0x014C;
	memcpy(buf2+0x08, buf3, 0x10);	

	/* SHA1 */
	ret = sceUtilsBufferCopyWithRange (buf2, 0x150, buf2, 0x150, 0xB);
	if (ret != 0) {
		return -6;
	}
	if (memcmp(buf2, buf4, 0x14)) {
		return -8;
	}

	prx_xor_key_large(buf2+0x80, 0x40, buf3+0x10);

	if (kirk7(buf2+0x6c, 0x40, pBlock->code, pBlock->use_polling) < 0) {
		return -7;
	}

	prx_xor_key_into(pBlock->prx+0x40, 0x40, buf2+0x6c, buf3+0x50);

	if (pBlock->type == 6) {
		memcpy(pBlock->prx+0x80, buf5, 0x20);
		memset(pBlock->prx+0xa0, 0, 0x10);
		((u8*)pBlock->prx)[0xa4] = 1;
		((u8*)pBlock->prx)[0xa0] = 1;
	} else {
		memset(pBlock->prx+0x80, 0, 0x30);
		((u8*)pBlock->prx)[0xa0] = 1;
	}

	memcpy(pBlock->prx+0xb0, buf2+0xc0, 0x10);
	memset(pBlock->prx+0xc0, 0, 0x10);
	memcpy(pBlock->prx+0xd0, buf2+0xd0, 0x80);

	// The real decryption
	ret = sceUtilsBufferCopyWithRange(pBlock->prx, pBlock->size, pBlock->prx+0x40, pBlock->size-0x40, 0x1);
	if (ret != 0) {
		return -9;
	}

	return 0;
}

