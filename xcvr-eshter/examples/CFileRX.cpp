/*
 * Copyright (C) 2015 SensorLab, Jozef Stefan Institute
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 * Written by Tomaz Solc, tomaz.solc@ijs.si
 */
#include <stdlib.h>
#include <unistd.h>

#include "SpectrumSensor.hpp"

const int max_nsamples_run = 25000;

const char* device = "/dev/ttyUSB0";
int samp_rate = 1000000;
VESNA::hz_t freq = -1;
int nsamples = 0;

int sample_cnt = 0;

const char* outp;
FILE* out = NULL;

bool cb(boost::shared_ptr<const VESNA::SweepConfig> sc, const VESNA::TimestampedData* samples)
{
	std::vector<VESNA::data_t>::const_iterator i = samples->data.begin();
	for(; i != samples->data.end(); ++i) {
		uint16_t v = *i;
		fwrite(&v, sizeof(v), 1, out);

		sample_cnt++;
		if(nsamples > 0 && sample_cnt >= nsamples) return false;
	}

	return true;
}

void help(const char* cmd)
{
	fprintf(stderr, "eshter_rx_cfile: signal sample recorder for VESNA SNE-ESHTER receiver\n"
			"\n"
			"Usage: %s -f FREQ [options] output_filename\n"
			"\n"
			"Options:\n"
			"  -h            show this help message and exit\n"
			"  -a DEVICE     device to use [default=/dev/ttyUSB0]\n"
			"  -r SAMP_RATE  set sample rate [default=1000000]\n"
			"  -f FREQ       set frequency\n"
			"  -N NSAMPLES   number of samples to collect [default=inf]\n"
			"\n"
			"Output file contains baseband samples in unsigned 16 bit integer format.\n", cmd);
}

int cmdline(int argc, char** argv)
{
	int opt;
	while((opt = getopt(argc, argv, "ha:r:f:N:")) != -1) {
		switch(opt) {
			case 'h':
				help(argv[0]);
				return 0;
			case 'a':
				device = optarg;
				break;
			case 'r':
				samp_rate = atoi(optarg);
				break;
			case 'f':
				char* endptr;
				freq = strtol(optarg, &endptr, 10);
				if(endptr == optarg) {
					fprintf(stderr, "invalid frequency: %s\n", optarg);
					return 1;
				}
				break;
			case 'N':
				nsamples = atoi(optarg);
				break;

		}
	}

	if(optind >= argc) {
		fprintf(stderr, "please specify output filename after options\n");
		return 1;
	}

	outp = argv[optind];

	return -1;
}

int main(int argc, char** argv)
{
	int r = cmdline(argc, argv);
	if(r != -1) {
		return r;
	}

	if(freq < 0) {
		fprintf(stderr, "please specify frequency with -f\n");
		return 1;
	}

	VESNA::SpectrumSensor ss(device);
	boost::shared_ptr<VESNA::ConfigList> cl = ss.get_config_list();

	int config_id;
	VESNA::DeviceConfig* c;

	for(config_id = 0; config_id < cl->get_config_num(); config_id++) {
		c = cl->get_config(0, config_id);
		if(c->bw == samp_rate/2 && c->covers(freq, freq)) {
			break;
		}
	}

	if(config_id >= cl->get_config_num()) {
		fprintf(stderr, "unsupported configuration (samp_rate=%d freq=%lld)\n",
				samp_rate, freq);
		return 1;
	}

	int nsamples_run = nsamples;
	if(nsamples_run > max_nsamples_run || nsamples < 1) {
		nsamples_run = max_nsamples_run;
	}

	out = fopen(outp, "wb");

	boost::shared_ptr<VESNA::SweepConfig> sc = c->get_sample_config(freq, nsamples_run);

	ss.sample_run(sc, cb);
}
