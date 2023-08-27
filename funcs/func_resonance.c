/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert
 *
 * Naveen Albert <asterisk@phreaknet.org>
 *
 * Actual filtering logic code from baltrax@hotmail.com (Zxform)
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Resonant low pass filter
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 *
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

/*** DOCUMENTATION
	<function name="RESONANCE_FILTER" language="en_US">
		<synopsis>
			Apply a resonance filter to a channel.
		</synopsis>
		<syntax>
			<parameter name="frequency" required="true">
				<para>Must be the frequency to attenuate.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="r">
						<para>Apply the resonance filter for received frames, rather than transmitted frames.
						Default is both directions.</para>
					</option>
					<option name="t">
						<para>Apply the resonance filter for transmitted frames, rather than received frames.
						Default is both directions.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Applies a resonance filter to a channel in either the receive or transmit direction.</para>
			<para>Specify the frequency at which to operate the filter or empty to remove an existing filter.</para>
			<para>For example:</para>
			<example title="Resonance filter at 3700 Hz in the TX direction">
			same => n,Set(RESONANCE_FILTER(t)=3700)
			</example>
			<example title="Disable filtering at 3700 Hz">
			same => n,Set(RESONANCE_FILTER(t)=)
			</example>
		</description>
	</function>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/audiohook.h"
#include "asterisk/app.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

/*
* ----------------------------------------------------------
* bilinear.c
*
* Perform bilinear transformation on s-domain coefficients
* of 2nd order biquad section.
* First design an analog filter and use s-domain coefficients
* as input to szxform() to convert them to z-domain.
*
* Here's the butterworth polinomials for 2nd, 4th and 6th order sections.
* When we construct a 24 db/oct filter, we take to 2nd order
* sections and compute the coefficients separately for each section.
*
* n Polinomials
* --------------------------------------------------------------------
* 2 s^2 + 1.4142s +1
* 4 (s^2 + 0.765367s + 1) (s^2 + 1.847759s + 1)
* 6 (s^2 + 0.5176387s + 1) (s^2 + 1.414214 + 1) (s^2 + 1.931852s + 1)
*
* Where n is a filter order.
* For n=4, or two second order sections, we have following equasions for each
* 2nd order stage:
*
* (1 / (s^2 + (1/Q) * 0.765367s + 1)) * (1 / (s^2 + (1/Q) * 1.847759s + 1))
*
* Where Q is filter quality factor in the range of
* 1 to 1000. The overall filter Q is a product of all
* 2nd order stages. For example, the 6th order filter
* (3 stages, or biquads) with individual Q of 2 will
* have filter Q = 2 * 2 * 2 = 8.
*
* The nominator part is just 1.
* The denominator coefficients for stage 1 of filter are:
* b2 = 1; b1 = 0.765367; b0 = 1;
* numerator is
* a2 = 0; a1 = 0; a0 = 1;
*
* The denominator coefficients for stage 1 of filter are:
* b2 = 1; b1 = 1.847759; b0 = 1;
* numerator is
* a2 = 0; a1 = 0; a0 = 1;
*
* These coefficients are used directly by the szxform()
* and bilinear() functions. For all stages the numerator
* is the same and the only thing that is different between
* different stages is 1st order coefficient. The rest of
* coefficients are the same for any stage and equal to 1.
*
* Any filter could be constructed using this approach.
*
* References:
* Van Valkenburg, "Analog Filter Design"
* Oxford University Press 1982
* ISBN 0-19-510734-9
*
* C Language Algorithms for Digital Signal Processing
* Paul Embree, Bruce Kimble
* Prentice Hall, 1991
* ISBN 0-13-133406-9
*
* Digital Filter Designer's Handbook
* With C++ Algorithms
* Britton Rorabaugh
* McGraw Hill, 1997
* ISBN 0-07-053806-9
* ----------------------------------------------------------
*/

/*
* ----------------------------------------------------------
* Pre-warp the coefficients of a numerator or denominator.
* Note that a0 is assumed to be 1, so there is no wrapping
* of it.
* ----------------------------------------------------------
*/
static void prewarp(
double *a0, double *a1, double *a2,
double fc, double fs)
{
	double wp, pi;

	pi = 4.0 * atan(1.0);
	wp = 2.0 * fs * tan(pi * fc / fs);

	*a2 = (*a2) / (wp * wp);
	*a1 = (*a1) / wp;
}

/*
* ----------------------------------------------------------
* bilinear()
*
* Transform the numerator and denominator coefficients
* of s-domain biquad section into corresponding
* z-domain coefficients.
*
* Store the 4 IIR coefficients in array pointed by coef
* in following order:
* beta1, beta2	(denominator)
* alpha1, alpha2 (numerator)
*
* Arguments:
* a0-a2 - s-domain numerator coefficients
* b0-b2 - s-domain denominator coefficients
* k - filter gain factor. initially set to 1
* and modified by each biquad section in such
* a way, as to make it the coefficient by
* which to multiply the overall filter gain
* in order to achieve a desired overall filter gain,
* specified in initial value of k.
* fs - sampling rate (Hz)
* coef	- array of z-domain coefficients to be filled in.
*
* Return:
* On return, set coef z-domain coefficients
* ----------------------------------------------------------
*/
static void bilinear(
double a0, double a1, double a2,	/* numerator coefficients */
double b0, double b1, double b2,	/* denominator coefficients */
double *k, /* overall gain factor */
double fs, /* sampling rate */
float *coef /* pointer to 4 iir coefficients */
)
{
	double ad, bd;

	/* alpha (Numerator in s-domain) */
	ad = 4. * a2 * fs * fs + 2. * a1 * fs + a0;
	/* beta (Denominator in s-domain) */
	bd = 4. * b2 * fs * fs + 2. * b1* fs + b0;

	/* update gain constant for this section */
	*k *= ad/bd;

	/* Denominator */
	*coef++ = (2. * b0 - 8. * b2 * fs * fs)
	/ bd; /* beta1 */
	*coef++ = (4. * b2 * fs * fs - 2. * b1 * fs + b0)
	/ bd; /* beta2 */

	/* Nominator */
	*coef++ = (2. * a0 - 8. * a2 * fs * fs)
	/ ad; /* alpha1 */
	*coef = (4. * a2 * fs * fs - 2. * a1 * fs + a0)
	/ ad; /* alpha2 */
}


/*
* ----------------------------------------------------------
* Transform from s to z domain using bilinear transform
* with prewarp.
*
* Arguments:
* For argument description look at bilinear()
*
* coef - pointer to array of floating point coefficients,
* corresponding to output of bilinear transofrm
* (z domain).
*
* Note: frequencies are in Hz.
* ----------------------------------------------------------
*/
static void szxform(
double *a0, double *a1, double *a2, /* numerator coefficients */
double *b0, double *b1, double *b2, /* denominator coefficients */
double fc, /* Filter cutoff frequency */
double fs, /* sampling rate */
double *k, /* overall gain factor */
float *coef) /* pointer to 4 iir coefficients */
{
	/* Calculate a1 and a2 and overwrite the original values */
	prewarp(a0, a1, a2, fc, fs);
	prewarp(b0, b1, b2, fc, fs);
	bilinear(*a0, *a1, *a2, *b0, *b1, *b2, k, fs, coef);
}

/**************************************************************************

FILTER.C - Source code for filter functions

iir_filter IIR filter floats sample by sample (real time)

*************************************************************************/

/* FILTER INFORMATION STRUCTURE FOR FILTER ROUTINES */

typedef struct {
unsigned int length; /* size of filter */
float *history; /* pointer to history in filter */
float *coef; /* pointer to coefficients of filter */
} FILTER;

#define FILTER_SECTIONS 2 /* 2 filter sections for 24 db/oct filter */

typedef struct {
double a0, a1, a2; /* numerator coefficients */
double b0, b1, b2; /* denominator coefficients */
} BIQUAD;

BIQUAD ProtoCoef[FILTER_SECTIONS]; /* Filter prototype coefficients,
1 for each filter section */

/*
* --------------------------------------------------------------------
* 
* iir_filter - Perform IIR filtering sample by sample on floats
* 
* Implements cascaded direct form II second order sections.
* Requires FILTER structure for history and coefficients.
* The length in the filter structure specifies the number of sections.
* The size of the history array is 2*iir->length.
* The size of the coefficient array is 4*iir->length + 1 because
* the first coefficient is the overall scale factor for the filter.
* Returns one output sample for each input sample. Allocates history
* array if not previously allocated.
* 
* float iir_filter(float input,FILTER *iir)
* 
* float input new float input sample
* FILTER *iir pointer to FILTER structure
* 
* Returns float value giving the current output.
* 
* Allocation errors cause an error message and a call to exit.
* --------------------------------------------------------------------
*/
static float iir_filter(
float input, /* new input sample */
FILTER *iir /* pointer to FILTER structure */
)
{
	unsigned int i;
	float *hist1_ptr,*hist2_ptr,*coef_ptr;
	float output,new_hist,history1,history2;

	/* allocate history array if different size than last call */

	if(!iir->history) {
		iir->history = (float *) ast_calloc(2*iir->length,sizeof(float));
		if(!iir->history) {
			printf("\nUnable to allocate history array in iir_filter\n");
			exit(1);
		}
	}

	coef_ptr = iir->coef; /* coefficient pointer */

	hist1_ptr = iir->history; /* first history */
	hist2_ptr = hist1_ptr + 1; /* next history */

	/* 1st number of coefficients array is overall input scale factor,
	* or filter gain */
	output = input * (*coef_ptr++);

	for (i = 0 ; i < iir->length; i++) {
		history1 = *hist1_ptr; /* history values */
		history2 = *hist2_ptr;

		output = output - history1 * (*coef_ptr++);
		new_hist = output - history2 * (*coef_ptr++);	/* poles */

		output = new_hist + history1 * (*coef_ptr++);
		output = output + history2 * (*coef_ptr++); /* zeros */

		*hist2_ptr++ = *hist1_ptr;
		*hist1_ptr++ = new_hist;
		hist1_ptr++;
		hist2_ptr++;
	}

	return(output);
}

static void free_filter(FILTER *iir)
{
	ast_free(iir->coef);
	ast_free(iir->history);
	ast_free(iir);
}

/*
* --------------------------------------------------------------------
* 
* main()
* 
* Example main function to show how to update filter coefficients.
* We create a 4th order filter (24 db/oct roloff), consisting
* of two second order sections.
* --------------------------------------------------------------------
*/
static FILTER *create_filter(int freq)
{
	FILTER *iir;
	float	*coef;
	double fs, fc; /* Sampling frequency, cutoff frequency */
	double Q; /* Resonance > 1.0 < 1000 */
	unsigned nInd;
	double a0, a1, a2, b0, b1, b2;
	double k; /* overall gain factor */

	iir = ast_calloc(1, sizeof(*iir));

	if (!iir) {
		ast_log(LOG_WARNING, "Failed to allocate filter\n");
		return NULL;
	}

	/*
	* Setup filter s-domain coefficients
	*/
	/* Section 1 */ 
	ProtoCoef[0].a0 = 1.0;
	ProtoCoef[0].a1 = 0;
	ProtoCoef[0].a2 = 0;
	ProtoCoef[0].b0 = 1.0;
	ProtoCoef[0].b1 = 0.765367;
	ProtoCoef[0].b2 = 1.0;

	/* Section 2 */ 
	ProtoCoef[1].a0 = 1.0;
	ProtoCoef[1].a1 = 0;
	ProtoCoef[1].a2 = 0;
	ProtoCoef[1].b0 = 1.0;
	ProtoCoef[1].b1 = 1.847759;
	ProtoCoef[1].b2 = 1.0;

	iir->length = FILTER_SECTIONS; /* Number of filter sections */

	/*
	* Allocate array of z-domain coefficients for each filter section
	* plus filter gain variable
	*/
	iir->coef = (float *) ast_calloc(4 * iir->length + 1, sizeof(float));
	if (!iir->coef) {
		ast_free(iir);
		ast_log(LOG_WARNING, "Unable to allocate coef array, exiting\n");
		return NULL;
	}

	k = 1.0; /* Set overall filter gain */
	coef = iir->coef + 1; /* Skip k, or gain */

	Q = 1; /* Resonance */
	fc = freq; /* Filter cutoff (Hz) */
	fs = 8000; /* Sampling frequency (Hz) */

	/*
	* Compute z-domain coefficients for each biquad section
	* for new Cutoff Frequency and Resonance
	*/
	for (nInd = 0; nInd < iir->length; nInd++) {
		a0 = ProtoCoef[nInd].a0;
		a1 = ProtoCoef[nInd].a1;
		a2 = ProtoCoef[nInd].a2;

		b0 = ProtoCoef[nInd].b0;
		b1 = ProtoCoef[nInd].b1 / Q; /* Divide by resonance or Q */
		b2 = ProtoCoef[nInd].b2;
		szxform(&a0, &a1, &a2, &b0, &b1, &b2, fc, fs, &k, coef);
		coef += 4; /* Point to next filter section */
	}

	/* Update overall filter gain in coef array */
	iir->coef[0] = k;

	/* Display filter coefficients */
	for (nInd = 0; nInd < (iir->length * 4 + 1); nInd++)
		ast_debug(1, "C[%d] = %15.10f\n", nInd, iir->coef[nInd]);
	/*
	* To process audio samples, call function iir_filter()
	* for each audio sample
	*/
	return iir;
}

struct resonance_information {
	struct ast_audiohook audiohook;
	int frequency;
	unsigned short int tx:1;
	unsigned short int rx:1;
	FILTER *iir;
};

enum resonance_flags {
	FLAG_TRANSMIT_DIR = (1 << 1),
	FLAG_RECEIVE_DIR = (1 << 2),
	FLAG_END_FILTER = (1 << 3),
};

AST_APP_OPTIONS(resonance_opts, {
	AST_APP_OPTION('t', FLAG_TRANSMIT_DIR),
	AST_APP_OPTION('r', FLAG_RECEIVE_DIR),
	AST_APP_OPTION('d', FLAG_END_FILTER),
});

static void destroy_callback(void *data)
{
	struct resonance_information *ni = data;

	/* Destroy the audiohook, and destroy ourselves */
	ast_audiohook_lock(&ni->audiohook);
	ast_audiohook_detach(&ni->audiohook);
	ast_audiohook_unlock(&ni->audiohook);
	ast_audiohook_destroy(&ni->audiohook);
	free_filter(ni->iir);
	ast_free(ni);

	return;
}

/*! \brief Static structure for datastore information */
static const struct ast_datastore_info resonance_datastore = {
	.type = "resonance",
	.destroy = destroy_callback
};

static inline void resonance_filter(FILTER *iir, short *amp, int samples)
{
	int i;
	for (i = 0; i < samples; i++) {
		amp[i] = iir_filter(amp[i], iir);
	}
}

static int resonance_callback(struct ast_audiohook *audiohook, struct ast_channel *chan, struct ast_frame *frame, enum ast_audiohook_direction direction)
{
	struct ast_datastore *datastore = NULL;
	struct resonance_information *ni = NULL;

	/* If the audiohook is stopping it means the channel is shutting down.... but we let the datastore destroy take care of it */
	if (audiohook->status == AST_AUDIOHOOK_STATUS_DONE) {
		return 0;
	}

	/* Grab datastore which contains our gain information */
	if (!(datastore = ast_channel_datastore_find(chan, &resonance_datastore, NULL))) {
		return 0;
	}

	ni = datastore->data;

	if (frame->frametype == AST_FRAME_VOICE) { /* we're filtering out an in-band frequency */
		/* Based on direction of frame, and confirm it is applicable */
		if (!(direction == AST_AUDIOHOOK_DIRECTION_READ ? ni->rx : ni->tx)) {
			return 0;
		}
		/* Filter the sample now */
		resonance_filter(ni->iir, frame->data.ptr, frame->samples);
	}

	return 0;
}

/*! \internal \brief Disable resonance filtering on the channel */
static int remove_resonance_filter(struct ast_channel *chan)
{
	struct ast_datastore *datastore = NULL;
	struct resonance_information *data;
	SCOPED_CHANNELLOCK(chan_lock, chan);

	datastore = ast_channel_datastore_find(chan, &resonance_datastore, NULL);
	if (!datastore) {
		ast_log(AST_LOG_WARNING, "Cannot remove RESONANCE_FILTER from %s: RESONANCE_FILTER not currently enabled\n",
		        ast_channel_name(chan));
		return -1;
	}
	data = datastore->data;

	if (ast_audiohook_remove(chan, &data->audiohook)) {
		ast_log(AST_LOG_WARNING, "Failed to remove RESONANCE_FILTER audiohook from channel %s\n", ast_channel_name(chan));
		return -1;
	}

	if (ast_channel_datastore_remove(chan, datastore)) {
		ast_log(AST_LOG_WARNING, "Failed to remove RESONANCE_FILTER datastore from channel %s\n",
		        ast_channel_name(chan));
		return -1;
	}
	ast_datastore_free(datastore);

	return 0;
}

static int resonance_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	char *parse;
	struct ast_datastore *datastore = NULL;
	struct resonance_information *ni = NULL;
	int is_new = 0;
	struct ast_flags flags = { 0 };
	int freq;
	
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(frequency);
		AST_APP_ARG(options);
	);

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}

	parse = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(value)) {
		return remove_resonance_filter(chan);
	}

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(resonance_opts, &flags, NULL, args.options);
	}

	freq = atoi(value);

	if (freq <= 0 || freq > 8000) {
		ast_log(LOG_WARNING, "Invalid frequency: %d\n", freq);
		return -1;
	}

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &resonance_datastore, NULL))) {
		ast_channel_unlock(chan);
		/* Allocate a new datastore to hold the reference to this resonance and audiohook information */
		if (!(datastore = ast_datastore_alloc(&resonance_datastore, NULL)))
			return 0;
		if (!(ni = ast_calloc(1, sizeof(*ni)))) {
			ast_datastore_free(datastore);
			return 0;
		}
		ast_audiohook_init(&ni->audiohook, AST_AUDIOHOOK_TYPE_MANIPULATE, "Resonance Filter", AST_AUDIOHOOK_MANIPULATE_ALL_RATES);
		ni->audiohook.manipulate_callback = resonance_callback;
		is_new = 1;
	} else {
		ast_channel_unlock(chan);
		ni = datastore->data;
	}

	ni->frequency = freq;
	ni->tx = 0;
	ni->rx = 0;
	if (ast_strlen_zero(args.options) || ast_test_flag(&flags, FLAG_TRANSMIT_DIR)) {
		ni->tx = 1;
	}
	if (ast_strlen_zero(args.options) || ast_test_flag(&flags, FLAG_RECEIVE_DIR)) {
		ni->rx = 1;
	}

	ni->iir = create_filter(ni->frequency);
	if (!ni->iir) {
		ast_log(LOG_WARNING, "Failed to properly set up resonance filter\n");
		remove_resonance_filter(chan);
		return -1;
	}

	if (is_new) {
		datastore->data = ni;
		ast_channel_lock(chan);
		ast_channel_datastore_add(chan, datastore);
		ast_channel_unlock(chan);
		ast_audiohook_attach(chan, &ni->audiohook);
	}

	return 0;
}

static struct ast_custom_function resonance_function = {
	.name = "RESONANCE_FILTER",
	.write = resonance_write,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&resonance_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&resonance_function);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Resonant low pass filter");
