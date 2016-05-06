/*
 * http_global.h -- Definitions for things that will be used throughout
 *                  the library.
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 16-Aug-1998
 *
 * Copyright (C) 1998 Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef HTTP_GLOBAL_H
#define HTTP_GLOBAL_H

#define HTTP_TRANS_ERR           -1
#define HTTP_TRANS_NOT_DONE       1
#define HTTP_TRANS_NEXT		2
#define HTTP_TRANS_DONE           3

#define HTTP_TRANS_SYNC           0		// ͬ��
#define HTTP_TRANS_ASYNC          1		// �첽

/*2015.10.14  Add by wolf-lone*/
#define GHTTP_DEBUG		0
#if GHTTP_DEBUG
#include <stdio.h>
#define ghttpDebug(fmt, ...)	printf("[%s-%d] "fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define ghttpDebug(fmt, ...)
#endif

#define GHTTP_ERROR_TRANS			"http trans error"
#define GHTTP_ERROR_TRANS_SEND			"http trans error: send"
#define GHTTP_ERROR_TRANS_READ_HEAD			"http trans error: read head"
#define GHTTP_ERROR_TRANS_READ_BODY			"http trans error: read body"
#define GHTTP_ERROR_TRANS_CHUNK_LENGTH		"http trans error: invalid chunk length number"

#endif /* GHTTP_GLOBAL_H */

