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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <rtl-sdr.h>
#include "acarsx.h"

#define RTLRATE 12500
#define RTLMULT 80
#define RTLINRATE (RTLRATE*RTLMULT)

static const float fir[RTLMULT] = {
	3.1623e-04, 9.4868e-04, 1.5811e-03, 2.2136e-03, 2.8460e-03,
	3.4785e-03, 4.1109e-03, 4.7434e-03, 5.3758e-03, 6.0083e-03,
	6.6407e-03, 7.2732e-03, 7.9056e-03, 8.5381e-03, 9.1705e-03,
	9.8030e-03, 1.0435e-02, 1.1068e-02, 1.1700e-02, 1.2333e-02,
	1.2965e-02, 1.3598e-02, 1.4230e-02, 1.4863e-02, 1.5495e-02,
	1.6128e-02, 1.6760e-02, 1.7392e-02, 1.8025e-02, 1.8657e-02,
	1.9290e-02, 1.9922e-02, 2.0555e-02, 2.1187e-02, 2.1820e-02,
	2.2452e-02, 2.3084e-02, 2.3717e-02, 2.4349e-02, 2.4982e-02,
	2.4982e-02, 2.4349e-02, 2.3717e-02, 2.3084e-02, 2.2452e-02,
	2.1820e-02, 2.1187e-02, 2.0555e-02, 1.9922e-02, 1.9290e-02,
	1.8657e-02, 1.8025e-02, 1.7392e-02, 1.6760e-02, 1.6128e-02,
	1.5495e-02, 1.4863e-02, 1.4230e-02, 1.3598e-02, 1.2965e-02,
	1.2333e-02, 1.1700e-02, 1.1068e-02, 1.0435e-02, 9.8030e-03,
	9.1705e-03, 8.5381e-03, 7.9056e-03, 7.2732e-03, 6.6407e-03,
	6.0083e-03, 5.3758e-03, 4.7434e-03, 4.1109e-03, 3.4785e-03,
	2.8460e-03, 2.2136e-03, 1.5811e-03, 9.4868e-04, 3.1623e-04
};

static rtlsdr_dev_t *dev = NULL;

#define RTLOUTBUFSZ 1024
#define RTLINBUFSZ (RTLOUTBUFSZ*RTLMULT*2)

static unsigned int choose_fc(unsigned int *Fd, unsigned int nbch)
{
	int n;
	int ne;
	int Fc;
	do {
		ne = 0;
		for (n = 0; n < nbch - 1; n++) {
			if (Fd[n] > Fd[n + 1]) {
				unsigned int t;
				t = Fd[n + 1];
				Fd[n + 1] = Fd[n];
				Fd[n] = t;
				ne = 1;
			}
		}
	} while (ne);

	if ((Fd[nbch - 1] - Fd[0]) > RTLINRATE - 4 * RTLRATE) {
		fprintf(stderr, "Frequencies to far apart\n");
		return -1;
	}

	for (Fc = Fd[nbch - 1] + 2 * RTLRATE; Fc > Fd[0] - 2 * RTLRATE; Fc--) {
		for (n = 0; n < nbch; n++) {
			if (abs(Fc - Fd[n]) > RTLINRATE / 2 - 2 * RTLRATE)
				break;
			if (abs(Fc - Fd[n]) < 2 * RTLRATE)
				break;
			if (n > 0 && Fc - Fd[n - 1] == Fd[n] - Fc)
				break;
		}
		if (n == nbch)
			break;
	}

	return Fc;
}

int nearest_gain(int target_gain)
{
	int i, err1, err2, count, close_gain;
	int *gains;
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = malloc(sizeof(int) * count);
	count = rtlsdr_get_tuner_gains(dev, gains);
	close_gain = gains[0];
	for (i = 0; i < count; i++) {
		err1 = abs(target_gain - close_gain);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			close_gain = gains[i];
		}
	}
	free(gains);
	if (verbose)
		fprintf(stderr, "Tuner gain : %f\n", (float)close_gain / 10.0);
	return close_gain;
}

int init_rtl(char **argv, int optind)
{
	int r, n;
	int dev_index;
	char *argF;
	unsigned int Fc;
	unsigned int Fd[4];
	struct sched_param schparam;

	n = rtlsdr_get_device_count();
	if (!n) {
		fprintf(stderr, "No supported devices found.\n");
		exit(1);
	}

	if (argv[optind] == NULL) {
		fprintf(stderr, "Need device index (ex: 0) after -r\n");
		exit(1);
	}
	dev_index = atoi(argv[optind++]);

	if (verbose)
		fprintf(stderr, "Using device %d: %s\n",
			dev_index, rtlsdr_get_device_name(dev_index));

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device\n");
		return r;
	}

	rtlsdr_set_tuner_gain_mode(dev, 1);
	r = rtlsdr_set_tuner_gain(dev, nearest_gain(gain));
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to set gain.\n");

	if (ppm != 0) {
		r = rtlsdr_set_freq_correction(dev, ppm);
		if (r < 0)
			fprintf(stderr,
				"WARNING: Failed to set freq. correction\n");
	}

	nbch = 0;
	while ((argF = argv[optind]) && nbch < MAXNBCHANNELS) {
		Fd[nbch] =
		    ((int)(1000000 * atof(argF) + RTLRATE / 2) / RTLRATE) *
		    RTLRATE;
		optind++;
		if (Fd[nbch] < 118000000 || Fd[nbch] > 138000000) {
			fprintf(stderr, "WARNING: Invalid frequency %d\n",
				Fd[nbch]);
			continue;
		}
		channel[nbch].Fr = (float)Fd[nbch];
		nbch++;
	};
	if (nbch >= MAXNBCHANNELS)
		fprintf(stderr,
			"WARNING: too much frequencies, taking only the %d firsts\n",
			MAXNBCHANNELS);

	if (nbch == 0) {
		fprintf(stderr, "Need a least one  frequencies\n");
		return 1;
	}

	Fc = choose_fc(Fd, nbch);
	if (Fc == 0)
		return 1;

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int ind;
		float AMFreq;

		ch->Infs = RTLRATE;

		ch->cwf = malloc(RTLMULT * sizeof(float));
		ch->swf = malloc(RTLMULT * sizeof(float));
		AMFreq = (ch->Fr - (float)Fc) / (float)(RTLINRATE) * 2.0 * M_PI;

		for (ind = 0; ind < RTLMULT; ind++) {
			sincosf(AMFreq * ind, &(ch->swf[ind]), &(ch->cwf[ind]));
			ch->swf[ind] *= fir[ind];
			ch->cwf[ind] *= fir[ind];
		}
	}

	if (verbose)
		fprintf(stderr, "Set center freq. to %dHz\n", (int)Fc);

	r = rtlsdr_set_center_freq(dev, Fc);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	}

	r = rtlsdr_set_sample_rate(dev, RTLINRATE);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	}

	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");
	}

	schparam.sched_priority = 20;
	sched_setscheduler(0, SCHED_FIFO, &schparam);

	return 0;
}

static void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx)
{
	int r, n;

	if (nread == 0) {
		fprintf(stderr, "Error: Failed to read.\n");
	}
	if (nread != RTLINBUFSZ) {
		fprintf(stderr, "warning: partial read\n");
	}

	for (n = 0; n < nbch; n++) {
		channel_t *ch = &(channel[n]);
		int i;
		float DI, DQ;
		float *swf, *cwf;
		float out[2];

		swf = ch->swf;
		cwf = ch->cwf;
		for (i = 0; i < RTLINBUFSZ;) {
			int ind;
			float V;

			DI = DQ = 0;
			for (ind = 0; ind < RTLMULT; ind++) {
				float I, Q;
				float sp, cp;

				I = (float)rtlinbuff[i] - (float)127.5;
				i++;
				Q = (float)rtlinbuff[i] - (float)127.5;
				i++;

				sp = swf[ind];
				cp = cwf[ind];
				DI += cp * I + sp * Q;
				DQ += -sp * I + cp * Q;
			}
			V = hypot(DI, DQ);
			demod_msk(V, ch);

		}
	}
}

int run_rtl_sample(void)
{
	int r;

	r = rtlsdr_read_async(dev, in_callback, NULL, 32, RTLINBUFSZ);
	return r;
}
