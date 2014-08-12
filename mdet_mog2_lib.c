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

#include "mdet_mog2_lib.h"

enum {
	FB_COLOR_TRANSPARENT	= 0,
	FB_COLOR_SEMI_TRANSPARENT,
	FB_COLOR_RED,
	FB_COLOR_GREEN,
	FB_COLOR_BLUE,
	FB_COLOR_WHITE,
	FB_COLOR_YELLOW,
	FB_COLOR_CYAN,
};

#define	MDET_Q16_1		( 1 << 16)
#define	MDET_Q16_255		( 255 << 16)

#define	MDET_Q16_ALPHA_INIT	32768

#define	MDET_Q16_Tb		(16 << 16)
#define	MDET_Q16_Tg		( 9 << 16)
#define	MDET_Q16_TB		58982

#define	MDET_Q16_VAR_INIT	(15 << 16)
#define	MDET_Q16_VAR_MIN	(4 << 16)
#define	MDET_Q16_VAR_MAX	(5 * MDET_Q16_VAR_INIT)

#define	MDET_Q16_CT		3277

#define MDET_FOREGROUND		FB_COLOR_WHITE
#define MDET_BACKGROUND		FB_COLOR_TRANSPARENT
#define MDET_OUT_OF_ROI		FB_COLOR_SEMI_TRANSPARENT

static	u32						frame_id;
static	mdet_dimension_t				dimension;
static	u32						max_modes;
static	mdet_roi_info_t					roi;

static	mdet_per_pixel_t				*mog2 = NULL;
static	u8						*foreground = NULL;

inline mdet_q16_t mul_q16(mdet_q16_t qa, mdet_q16_t qb)
{
	u32			r1, r2, r3, r4;
	u32			ah, al, bh, bl;

	ah	= qa >> 16;
	al	= qa & 0xffff;
	bh	= qb >> 16;
	bl	= qb & 0xffff;

	r1	= (ah * bh) << 16;
	r2	= al * bh;
	r3	= ah * bl;
	r4	= (al * bl) >> 16;

	return (r1 + r2 + r3 + r4);
}

/* Assume qa < 1 && qc < 1 */
inline mdet_q16_t div_q16(mdet_q16_t qa, mdet_q16_t qb, mdet_q16_t qc)
{
	u32			al, bh, bl;
	u32			mh, ml, nh, nl;
	u32			t, r1, r2, r3;

	al	= qa & 0xffff;
	bh	= qb >> 16;
	bl	= qb & 0xffff;

	mh	= al * bh;
	ml	= al * bl;
	nh	= ml >> 16;
	nl	= ml & 0xffff;
	t	= mh + nh;

	r1	= t / qc;
	r2	= t % qc;
	t	= (r2 << 16) + nl;
	r3	= t / qc;

	return ((r1 << 16) + r3);
}

int mdet_start(mdet_dimension_t fm_dim, u32 modes, mdet_roi_info_t roi_info, u32 draw)
{
	int			ret = 0;
	u32			i, j, k;
	mdet_roi_t		*p;
	mdet_per_pixel_t	*q1, *q2;

	if (!fm_dim.pitch || !fm_dim.width || !fm_dim.height) {
		printf("MDET ERROR %d: Incorrect Frame Dimension!\n", __LINE__);
		return -1;
	}

	if (fm_dim.pitch < fm_dim.width) {
		printf("MDET ERROR %d: Incorrect Frame Dimension!\n", __LINE__);
		return -1;
	}

	if (fm_dim.width > 1280 || fm_dim.height > 720) {
		printf("MDET ERROR %d: Incorrect Frame Dimension!\n", __LINE__);
		return -1;
	}

	if (modes <= 0 || modes > MDET_MAX_MODES) {
		printf("MDET ERROR %d: Incorrect Number of Modes!\n", __LINE__);
		return -1;
	}

	if (roi_info.num_roi && !roi_info.roi) {
		printf("MDET ERROR %d: Incorrect ROI!\n", __LINE__);
		return -1;
	}

	frame_id		= 0;
	dimension		= fm_dim;
	max_modes		= modes;

	mog2 = (mdet_per_pixel_t *)malloc(dimension.width * dimension.height * sizeof(mdet_per_pixel_t));
	if (mog2 == NULL) {
		printf("MDET ERROR %d: Unable to allocate memory!\n", __LINE__);
		return -1;
	} else {
		memset(mog2, 0, dimension.width * dimension.height * sizeof(mdet_per_pixel_t));
	}

	foreground = (u8 *)malloc(dimension.width * dimension.height);
	if (foreground == NULL) {
		printf("MDET ERROR %d: Unable to allocate memory!\n", __LINE__);
		return -1;
	}

	p	= roi_info.roi;
	for (i = 0; i < roi_info.num_roi; i++, p++) {
		if (p->exclusive) {
			continue;
		}

		if (p->right >= dimension.width || p->left > p->right) {
			printf("MDET ERROR %d: Incorrect ROI!\n", __LINE__);
			mdet_stop();
			return -1;
		}

		if (p->high >= dimension.height || p->low > p->high) {
			printf("MDET ERROR %d: Incorrect ROI!\n", __LINE__);
			mdet_stop();
			return -1;
		}

		q1	= mog2 + p->low * dimension.width;
		for (j = p->low; j <= p->high; j++, q1 += dimension.width) {
			q2 = q1 + p->left;
			for (k = p->left; k <= p->right; k++, q2++) {
				q2->included = 1;
			}
		}
	}

	/* Exclusive will overwrite Inclusive */
	p	= roi_info.roi;
	for (i = 0; i < roi_info.num_roi; i++, p++) {
		if (!p->exclusive) {
			continue;
		}

		if (p->right >= dimension.width || p->left > p->right) {
			printf("MDET ERROR %d: Incorrect ROI!\n", __LINE__);
			mdet_stop();
			return -1;
		}

		if (p->high >= dimension.height || p->low > p->high) {
			printf("MDET ERROR %d: Incorrect ROI!\n", __LINE__);
			mdet_stop();
			return -1;
		}

		q1	= mog2 + p->low * dimension.width;
		for (j = p->low; j <= p->high; j++, q1 += dimension.width) {
			q2 = q1 + p->left;
			for (k = p->left; k <= p->right; k++, q2++) {
				q2->included = 0;
			}
		}
	}

	p	= roi_info.roi;
	for (i = 0; i < roi_info.num_roi; i++, p++) {
		if (p->exclusive) {
			continue;
		}

		p->pixels	= 0;
		q1		= mog2 + p->low * dimension.width;
		for (j = p->low; j <= p->high; j++, q1 += dimension.width) {
			q2 = q1 + p->left;
			for (k = p->left; k <= p->right; k++, q2++) {
				if (q2->included) {
					p->pixels++;
				}
			}
		}
	}

	roi = roi_info;

	return ret;
}

int mdet_stop(void)
{
	if (mog2) {
		free(mog2);
		mog2 = NULL;
	}

	if (foreground) {
		free(foreground);
		foreground = NULL;
	}

	return 0;
}

int mdet_update_frame(const u8 *fm_ptr, u32 T)
{
	int			is_bg, fit_pdf;
	int			i, j, k, m, n, l;
	int			nmodes, mode;
	u8			*fg, *q1, *q2;
	mdet_per_pixel_t	*p;
	mdet_gsm_t		*g, q;
	mdet_q16_t		a, a1, weight, total_weight, prune;
	mdet_q16_t		dist, data, vb, vg, var, d;
	mdet_roi_t		*r;

	if (!fm_ptr || !foreground || !mog2) {
		printf("MDET ERROR %d: Incorrect Pointer!\n", __LINE__);
		return -1;
	}

	if (T <= 1) {
		printf("MDET ERROR %d: Incorrect T!\n", __LINE__);
		return -1;
	}

	if (!frame_id) {
		a		= MDET_Q16_ALPHA_INIT;
		frame_id	= 1;
	} else {
		a		= MDET_Q16_1 / T;
	}

	a1	= MDET_Q16_1 - a;
	prune	= mul_q16(a, MDET_Q16_CT);

	p	= mog2;
	fg	= foreground;
	for (i = 0; i < dimension.height; i++, fm_ptr += dimension.pitch) {
		for (j = 0; j < dimension.width; j++, p++, fg++) {
			if (!p->included) {
				*fg = MDET_OUT_OF_ROI;
				continue;
			}

			is_bg			= 0;
			fit_pdf			= 0;
			data			= fm_ptr[j] << 16;
			total_weight		= 0;

			for (m = 0, g = p->gsm; m < p->num_modes; m++, g++, total_weight += weight) {
				weight	= mul_q16(g->weight, a1);
				if (weight < prune) {
					weight	= 0;
				} else {
					weight	= weight - prune;
				}

				if (!fit_pdf) {
					if (data > g->mean) {
						dist	= data - g->mean;
					} else {
						dist	= g->mean - data;
					}
					dist		= mul_q16(dist, dist);

					var		= g->variance;
					vb		= mul_q16(var, MDET_Q16_Tb);
					vg		= mul_q16(var, MDET_Q16_Tg);

					if (total_weight < MDET_Q16_TB && dist < vb) {
						is_bg	= 1;
					}

					if (dist < vg) {
						fit_pdf		=	1;

						weight		+=	a;
						g->weight	=	weight;

						if (data > g->mean) {
							d		=	div_q16(a, data - g->mean, weight);
							g->mean		+=	d;

							if (g->mean > MDET_Q16_255) {
								g->mean	= MDET_Q16_255;
							}
						} else {
							d		=	div_q16(a, g->mean - data, weight);
							if (d > g->mean) {
								g->mean	=	0;
							} else {
								g->mean	-=	d;
							}
						}

						if (dist > var) {
							d		=	div_q16(a, dist - var, weight);
							var		+=	d;
						} else {
							d		=	div_q16(a, var - dist, weight);
							if (var < d) {
								var	=	0;
							} else {
								var	-=	d;
							}
						}

						if (var < MDET_Q16_VAR_MIN) {
							var	= MDET_Q16_VAR_MIN;
						}
						if (var > MDET_Q16_VAR_MAX) {
							var	= MDET_Q16_VAR_MAX;
						}

						g->variance	= var;

						l	=	m;
						for (n = m - 1; n >= 0; n--) {
							if (p->gsm[n].weight < weight) {
								l	= n;
							} else {
								break;
							}
						}

						if (l < m) {
							q		=	*g;
							for (n = m - 1; n >= l; n--) {
								p->gsm[n + 1]	=	p->gsm[n];
							}
							p->gsm[l]	=	q;
						}

						continue;
					}
				}

				/* Update weight */
				if (weight < prune) {
					weight	=	0;
				}
				g->weight	=	weight;
			}

			/* Normalize weight */
			for (m = 0, g = p->gsm, nmodes = 0; m < p->num_modes; m++, g++, nmodes++) {
				if (!g->weight) {
					break;
				}

				if (g->weight >= MDET_Q16_1) {
					g->weight	=	0xffffffff;
				} else {
					g->weight	<<=	16;
				}
				g->weight		/=	total_weight;
			}

			/* Add new mode */
			if (!fit_pdf) {
				if (nmodes < max_modes) {
					mode = nmodes;
					nmodes++;
				} else {
					mode = max_modes - 1;
				}

				if (nmodes == 1) {
					p->gsm[0].weight = MDET_Q16_1;
				} else {
					p->gsm[mode].weight = a;

					for (m = 0; m < nmodes - 1; m++) {
						p->gsm[m].weight	= mul_q16(p->gsm[m].weight, a1);
					}
				}
				p->gsm[mode].mean		= data;
				p->gsm[mode].variance		= MDET_Q16_VAR_INIT;

				/* Sort weight in descending order */
				l	=	mode;
				for (n = mode - 1; n >= 0; n--) {
					if (p->gsm[n].weight < a) {
						l	= n;
					} else {
						break;
					}
				}
				if (l < mode) {
					q	=	p->gsm[mode];
					for (n = mode - 1; n >= l; n--) {
						p->gsm[n + 1]	=	p->gsm[n];
					}
					p->gsm[l]	=	q;
				}
			}
			p->num_modes	= nmodes;

			if (is_bg) {
				*fg	= MDET_BACKGROUND;
			} else {
				*fg	= MDET_FOREGROUND;
			}

			//asm ("PLD [%0, #128]"::"r" (fm_ptr));
			//asm ("PLD [%0, #128]"::"r" (p));
		}
	}

	r	= roi.roi;
	for (i = 0; i < roi.num_roi; i++, r++) {
		if (r->exclusive) {
			continue;
		}

		r->motion = 0;

		if (!r->pixels) {
			continue;
		}


		q1	= foreground + r->low * dimension.width;
		for (j = r->low; j <= r->high; j++, q1 += dimension.width) {
			q2 = q1 + r->left;
			for (k = r->left; k <= r->right; k++, q2++) {
				if (*q2 == MDET_FOREGROUND) {
					r->motion++;
				}
			}
		}

		if (r->pixels < MDET_Q16_1) {
			r->motion = (r->motion << 16) / r->pixels;
		} else {
			r->motion = (r->motion << 12) / (r->pixels >> 4);
		}
	}

	return 0;
}
