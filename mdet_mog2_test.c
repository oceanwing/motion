/*
 *
 * History:
 *    2013/12/18 - [Zhenwu Xue] Create
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.  *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <getopt.h>
#include "mdet_mog2_lib.h"

#define MDET_MAX_ROI		32

#define MDET_CHECK_ERROR()	\
	if (ret < 0) {	\
		printf("%s %d: Error!\n", __func__, __LINE__);	\
		return ret;	\
	}

struct option long_options[] = {
	{"verbose",	0,	0,	'v'},
	{"gui",		0,	0,	'g'},
	{"second_buf",	0,	0,	's'},
	{"modes",	1,	0,	'm'},
	{"alphaT",	1,	0,	'T'},
	{"include",	1,	0,	'i'},
	{"exclude",	1,	0,	'e'},
	{0,		0,	0,	 0 },
};

const char *short_options = "vgsm:T:i:e:";

int			g_verbose	= 0;
int			g_show		= 0;
//iav_buf_t		g_buf_type	= IAV_BUF_ME1_SECOND;
u32			g_modes		= 1;
u32			g_T		= MDET_T_DEFAULT;
mdet_roi_t		g_roi[MDET_MAX_ROI];
u32			g_num_roi	= 0;

int parse_parameters(int argc, char **argv)
{
	int		ret;
	int		c;
	mdet_roi_t	*r;

	while (1) {
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c < 0) {
			break;
		}

		switch (c) {
		case 'v':
			g_verbose = 1;
			break;

		case 'g':
			g_show = 1;
			break;

		case 's':
			//g_buf_type = IAV_BUF_SECOND;
			break;

		case 'm':
			sscanf(optarg, "%u", &g_modes);
			if (!g_modes) {
				printf("Modes must be larger than 0!\n");
				g_modes = 1;
			}

		case 'T':
			ret = sscanf(optarg, "%u", &g_T);
			if (!g_T) {
				printf("T must be larger than 0!\n");
				g_T = MDET_T_DEFAULT;
			}
			break;

		case 'i':
			if (g_num_roi >= MDET_MAX_ROI) {
				g_num_roi = MDET_MAX_ROI - 1;
				printf("WARNING: ROI number exceeds maximum %u!\n", MDET_MAX_ROI);
			}
			r = &g_roi[g_num_roi];
			ret = sscanf(optarg, "%u,%u,%u,%u", &r->left, &r->right, &r->low, &r->high);
			if (ret == 4) {
				r->exclusive = 0;
				g_num_roi++;
			}
			break;

		case 'e':
			if (g_num_roi >= MDET_MAX_ROI) {
				g_num_roi = MDET_MAX_ROI - 1;
				printf("WARNING: ROI number exceeds maximum %u!\n", MDET_MAX_ROI);
			}
			r = &g_roi[g_num_roi];
			ret = sscanf(optarg, "%u,%u,%u,%u", &r->left, &r->right, &r->low, &r->high);
			if (ret == 4) {
				r->exclusive = 1;
				g_num_roi++;
			}
			break;

		default:
			printf("Unknown parameter %c!\n", c);
			break;

		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int				ret;
	u32				i;
	unsigned int			w, h, p;
	char				*buf;
	struct timeval			tv_begin, tv_end;
	mdet_dimension_t		d;
	mdet_roi_info_t			roi_info;

	ret = parse_parameters(argc, argv);
	MDET_CHECK_ERROR();

	//ret = init_iav(g_buf_type);
	MDET_CHECK_ERROR();

	//ret = get_iav_buf_size(&p, &w, &h);
	MDET_CHECK_ERROR();
	if (g_verbose) {
		printf("Buffer Size: %d x %d, pitch = %d\n", w, h, p);
	}

	d.pitch		= p;
	d.width		= w;
	d.height	= h;

	if (!g_num_roi) {
		g_roi[0].left		= 0;
		g_roi[0].right		= w - 1;
		g_roi[0].low		= 0;
		g_roi[0].high		= h - 1;
		g_roi[0].exclusive	= 0;

		g_num_roi		= 1;
	}

	roi_info.num_roi= g_num_roi;
	roi_info.roi	= g_roi;

	ret = mdet_start(d, g_modes, roi_info, g_show);
	MDET_CHECK_ERROR();

	while (1) {
		if (g_verbose) {
			gettimeofday(&tv_begin, NULL);
		}

		//buf = get_iav_buf();
		//if (!buf) {
			//ret = -1;
		//}
		MDET_CHECK_ERROR();
		if (g_verbose) {
			gettimeofday(&tv_end, NULL);
		  	printf("Get Buf: %ld ms\n", 1000 * (tv_end.tv_sec - tv_begin.tv_sec) + (tv_end.tv_usec - tv_begin.tv_usec) / 1000);
		}

		if (g_verbose) {
			gettimeofday(&tv_begin, NULL);
		}
		mdet_update_frame((u8 *)buf, g_T);
		if (g_verbose) {
			gettimeofday(&tv_end, NULL);
		  	printf("Mdet: %ld ms\n", 1000 * (tv_end.tv_sec - tv_begin.tv_sec) + (tv_end.tv_usec - tv_begin.tv_usec) / 1000);
			for (i = 0; i < g_num_roi; i++) {
				printf("Motion #%u = %5u / 65536\n", i, g_roi[i].motion);
			}
		}
	}

	//exit_iav();
	mdet_stop();

	return ret;
}
