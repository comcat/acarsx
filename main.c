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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <sched.h>
#include "acarsx.h"

channel_t channel[MAXNBCHANNELS];
unsigned int nbch;

int inmode = 0;
int verbose = 0;
int outtype = 2;
int airflt = 0;
int gain = 1000;
int ppm = 0;

char *Rawaddr = NULL;
char *logfilename = NULL;

static void usage(void)
{
	fprintf(stderr,
		"Copyright (c) 2014 Jack Tan at Jack's Lab\n\n");
	fprintf(stderr,
		"Usage: acarsx [options] -r <rtl_dev_num> f1 [f2...f6]\n\n");
	fprintf(stderr,
		"<rtl_dev_num> is the rtl dongle number starting from 0\n");
	fprintf(stderr, "f1 and optionaly f2 to f6 are frequencies in Mhz\n");
	fprintf(stderr, "Example: acarsx -r 0 131.450 131.825\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, " -v\t\tverbose output\n");
	fprintf(stderr,
		" -A\t\tdon't display uplink messages\n");
	fprintf(stderr,
		" -o <format>\toutput format: 0, no log; 1, one line by msg; 2, full(default)\n");
	fprintf(stderr,
		" -l <logfile>\tappend log messages to logfile (Default: stdout)\n");
	fprintf(stderr,
		" -n <ip:port>\tsend acars messages to ip:port on UDP\n\n");
	fprintf(stderr,
		" -g <gain>\tset rtl preamp gain in tenth of db (ie. -g 70 for +7db)\n");
	fprintf(stderr, " -p <ppm>\tset rtl ppm frequency correction\n");
	fprintf(stderr, " -h\t\tshow this help message\n\n");
	fprintf(stderr,
		"\nFor any input source, up to 4 channels could be simultanously decoded\n");
	exit(1);
}

static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	sleep(1);
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	int res, n;
	struct sigaction sigact;

	while ((c = getopt(argc, argv, "va:fro:g:Ahp:n:l:c:")) != EOF) {

		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'o':
			outtype = atoi(optarg);
			break;
		case 'r':

			res = init_rtl(argv, optind);
			inmode = 3;
			break;
		case 'g':
			gain = atoi(optarg);
			break;
		case 'p':
			ppm = atoi(optarg);
			break;
		case 'n':
			Rawaddr = optarg;
			break;
		case 'A':
			airflt = 1;
			break;
		case 'l':
			logfilename = optarg;
			break;

		case 'h':
		default:
			usage();
		}
	}

	if (inmode == 0) {
		usage();
	}

	if (res) {
		fprintf(stderr, "Unable to init input\n");
		exit(res);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	for (n = 0; n < nbch; n++) {

		if (channel[n].Infs) {

			channel[n].chn = n;
			channel[n].inmode = inmode;

			res = init_msk(&(channel[n]));
			if (res)
				break;
			res = init_acars(&(channel[n]));
			if (res)
				break;
		}
	}

	if (res) {
		fprintf(stderr, "Unable to init internal decoders\n");
		exit(res);
	}

	res = init_output(logfilename, Rawaddr);
	if (res) {
		fprintf(stderr, "Unable to init output\n");
		exit(res);
	}

	if (verbose)
		fprintf(stderr, "Decoding %d channels\n", nbch);

	/* main decoding  */
	switch (inmode) {
		case 3:
			res = run_rtl_sample();
			break;
		default:
			res = -1;
	}

	sleep(1);
	exit(res);
}
