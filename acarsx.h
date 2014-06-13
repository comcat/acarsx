/*
 * Copyright (c) 2014 Thierry Leconte (f4dwv)
 * Copyright (c) 2014 Jack Tan(BH1RBH) at Jack's Lab
 * 
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License version 2
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <time.h>
#include <pthread.h>

#define MAXNBCHANNELS 6

typedef float sample_t;

typedef struct mskblk_s {
	struct mskblk_s *prev;
	int chn;
	time_t t;
	int len;
	int lvl, err;
	unsigned char txt[250];
	unsigned char crc[2];
} msgblk_t;

typedef struct {
	int chn;
	int inmode;
	int Infs;

	float Fr;
	float *swf;
	float *cwf;

	float MskPhi;
	float MskFreq, MskDf;
	float Mska, MskKa;
	float Mskdc, Mskdcf;
	float MskClk;
	unsigned int MskS;

	sample_t *I, *Q;
	float *h;
	int flen, idx;

	unsigned char outbits;
	int nbits;

	enum { WSYN, SYN2, SOH1, TXT, CRC1, CRC2, END } Acarsstate;
	msgblk_t *blk;

} channel_t;

extern channel_t channel[MAXNBCHANNELS];
extern unsigned int nbch;
extern unsigned long wrktot;
extern unsigned long wrkmask;
extern pthread_mutex_t datamtx;
extern pthread_cond_t datawcd;

extern int inpmode;
extern int verbose;
extern int outtype;
extern int airflt;
extern int gain;
extern int ppm;

extern int init_output(char *, char *);

extern int init_rtl(char **argv, int optind);
extern int run_rtl_sample(void);

extern int init_msk(channel_t *);
extern void demod_msk(float in, channel_t *);

extern int init_acars(channel_t *);
extern void decode_acars(channel_t *);

extern void outputmsg(const msgblk_t *);
