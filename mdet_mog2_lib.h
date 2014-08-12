/*
 *
 * History:
 *    2013/12/18 - [Zhenwu Xue] Create
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 *
 */

#ifndef __MDET_MOG2_H__
#define __MDET_MOG2_H__

#include "basetypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MDET_MAX_MODES		5

#define	MDET_T_DEFAULT		200

typedef u32			mdet_q16_t;

typedef struct {
	u32			pitch;
	u32			width;
	u32			height;
} mdet_dimension_t;

typedef struct {
	mdet_q16_t		weight;
	mdet_q16_t		mean;
	mdet_q16_t		variance;
} mdet_gsm_t;

typedef struct {
	int			included;
	int			num_modes;
	mdet_gsm_t		gsm[MDET_MAX_MODES];
} mdet_per_pixel_t;

typedef struct {
	/* App Input */
	u32			left;
	u32			right;
	u32			low;
	u32			high;
	u32			exclusive;

	/* Middle */
	u32			pixels;

	/* Library Result */
	mdet_q16_t		motion;
} mdet_roi_t;

typedef struct {
	u32			num_roi;
	mdet_roi_t		*roi;
} mdet_roi_info_t;

#ifdef __cplusplus
extern "C" {
#endif

int mdet_start(mdet_dimension_t fm_dim, u32 modes, mdet_roi_info_t roi_info, u32 draw);
int mdet_update_frame(const u8 *fm_ptr, u32 T);
int mdet_stop(void);

#ifdef __cplusplus
}
#endif

#endif

