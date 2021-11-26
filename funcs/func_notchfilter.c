/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Naveen Albert
 *
 * Naveen Albert <asterisk@phreaknet.org>
 *
 * mkfilter -- given n, compute recurrence relation
 *  to implement Butterworth, Bessel or Chebyshev filter of order n
 *  A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
 *  September 1992
 *
 * mknotch -- Make IIR notch filter parameters, based upon mkfilter;
 *  A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
 *  September 1992
 *
 * Routines for complex arithmetic
 *
 * Complex math code converted from C++ to C - Naveen Albert, 2021
 * sf_detect from DAHDI source code
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
 * \brief Technology independent notch filter
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
	<function name="NOTCH_FILTER" language="en_US">
		<synopsis>
			Apply a notch filter to a channel.
		</synopsis>
		<syntax>
			<parameter name="frequency" required="true">
				<para>Must be the frequency to attenuate.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="d">
						<para>Destroy the existing notch filter.</para>
					</option>
					<option name="r">
						<para>Apply the notch filter for received frames, rather than transmitted frames.
						Default is both directions.</para>
					</option>
					<option name="t">
						<para>Apply the notch filter for transmitted frames, rather than received frames.
						Default is both directions.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>The NOTCH_FILTER function attenuates a specified frequency using the provided bandwidth.</para>
			<para>For example:</para>
			<para>Set(NOTCH_FILTER(2600,t)=10.0)</para>
			<para>Set(NOTCH_FILTER(1004)=5.0)</para>
			<para>Set(NOTCH_FILTER(2400,r)=15.0)</para>
			<para>Set(NOTCH_FILTER(1004,r)=10.0)</para>
			<para>Set(NOTCH_FILTER(1004,d)=)</para>
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

#define global
#define unless(x)   if(!(x))
#define until(x)    while(!(x))
#undef	PI
#define PI	    3.14159265358979323846
#define TWOPI	    (2.0 * PI)
#define EPS	    1e-10
#define MAXPZ	    512

struct c_complex
{
	double re, im;
};

typedef struct complex complex;

struct complex {
	double re, im;
};

complex cconj(complex z);
complex expj(double);
complex evaluate(complex[], int, complex[], int, complex);
complex complexmult2(complex z1, complex z2);
complex complexdiv2(complex z1, complex z2);
complex complexmult(double a, complex z);
complex complexdiv(complex z, double a);
complex complexadd2(complex z1, complex z2);
complex complexsub2(complex z1, complex z2);
complex complexnegative(complex z);
int complexequals(complex z1, complex z2);

complex cconj(complex z) /* removed inline to compile */
{
	z.im = -z.im;
	return z;
}

complex complexmult2(complex z1, complex z2) /* removed inline to compile */
{
	complex c;
	c.re = z1.re*z2.re - z1.im*z2.im;
	c.im = z1.re*z2.im + z1.im*z2.re;
	return c;
}

complex complexdiv2(complex z1, complex z2)
{
	double mag = (z2.re * z2.re) + (z2.im * z2.im);
	complex c;
	c.re = ((z1.re * z2.re) + (z1.im * z2.im)) / mag;
	c.im = ((z1.im * z2.re) - (z1.re * z2.im)) / mag;
	return c;
}

complex complexmult(double a, complex z) /* removed inline to compile */
{
	z.re *= a; z.im *= a;
	return z;
}

complex complexdiv(complex z, double a) /* removed inline to compile */
{
	z.re = z.re / a;
	z.im = z.im / a;
	return z;
}

complex complexadd2(complex z1, complex z2) /* removed inline to compile */
{
	z1.re += z2.re;
	z1.im += z2.im;
	return z1;
}

complex complexsub2(complex z1, complex z2) /* removed inline to compile */
{
	z1.re -= z2.re;
	z1.im -= z2.im;
	return z1;
}

complex complexnegative(complex z) /* removed inline to compile */
{
	complex c;
	c.re = -z.re;
	c.im = -z.im;
	return c;
}

int complexequals(complex z1, complex z2) /* removed inline to compile */
{
	return (z1.re == z2.re) && (z1.im == z2.im);
}

static complex eval(complex coeffs[], int npz, complex z);

/* evaluate response, substituting for z */
complex evaluate(complex topco[], int nz, complex botco[], int np, complex z)
{
	return complexdiv2(eval(topco, nz, z), eval(botco, np, z));
}

/* evaluate polynomial in z, substituting for z */
static complex eval(complex coeffs[], int npz, complex z)
{
	complex sum;
	sum.re = 0.0;
	sum.im = 0.0;
	for (int i = npz; i >= 0; i--) {
		sum = complexadd2((complexmult2(sum, z)), coeffs[i]);
	}
	return sum;
}

complex expj(double theta)
{
	complex c;
	c.re = cos(theta);
	c.im = sin(theta);
	return c;
}

/* mknotch.cc main */
#define opt_be 0x00001	/* -Be		Bessel characteristic	       */
#define opt_bu 0x00002	/* -Bu		Butterworth characteristic     */
#define opt_ch 0x00004	/* -Ch		Chebyshev characteristic       */
#define opt_re 0x00008	/* -Re		Resonator		       */
#define opt_pi 0x00010	/* -Pi		proportional-integral	       */

#define opt_lp 0x00020	/* -Lp		lowpass			       */
#define opt_hp 0x00040	/* -Hp		highpass		       */
#define opt_bp 0x00080	/* -Bp		bandpass		       */
#define opt_bs 0x00100	/* -Bs		bandstop		       */
#define opt_ap 0x00200	/* -Ap		allpass			       */

#define opt_a  0x00400	/* -a		alpha value		       */
#define opt_l  0x00800	/* -l		just list filter parameters    */
#define opt_o  0x01000	/* -o		order of filter		       */
#define opt_p  0x02000	/* -p		specified poles only	       */
#define opt_w  0x04000	/* -w		don't pre-warp		       */
#define opt_z  0x08000	/* -z		use matched z-transform	       */
#define opt_Z  0x10000	/* -Z		additional zero		       */

typedef struct pzrep pzrep;

struct pzrep
{
	complex poles[MAXPZ], zeros[MAXPZ];
	int numpoles, numzeros;
};

static pzrep zplane;
static double raw_alpha1, raw_alpha2;
static complex dc_gain, fc_gain, hf_gain;
static unsigned int options;
static double qfactor;
static int infq;
static unsigned int polemask;
static double xcoeffs[MAXPZ+1], ycoeffs[MAXPZ+1];

static void compute_notch(void);
static void expandpoly(void), expand(complex[], int, complex[]), multin(complex, int, complex[]);

/* compute Z-plane pole & zero positions for bandpass resonator */
static void compute_bpres(void)
{
	double theta;
    zplane.numpoles = 2;
	zplane.numzeros = 2;
	zplane.zeros[0].re = 1.0;
	zplane.zeros[0].im = 0.0;
	zplane.zeros[1].re = -1.0;
	zplane.zeros[1].im = 0.0;
    theta = TWOPI * raw_alpha1; /* where we want the peak to be */
    if (infq) { /* oscillator */
		complex zp = expj(theta);
		zplane.poles[0] = zp;
		zplane.poles[1] = cconj(zp);
    } else { /* must iterate to find exact pole positions */
		complex topcoeffs[MAXPZ+1];
		double r, thm = theta, th1 = 0.0, th2 = PI;
		int cvg;
		expand(zplane.zeros, zplane.numzeros, topcoeffs);
		r = exp(-theta / (2.0 * qfactor));
		cvg = 0;
		for (int i=0; i < 50 && !cvg; i++) {
			complex botcoeffs[MAXPZ+1];
			complex g;
			double phi;
			complex zp = complexmult(r, expj(thm));
			zplane.poles[0] = zp;
			zplane.poles[1] = cconj(zp);
			expand(zplane.poles, zplane.numpoles, botcoeffs);
			g = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta));
			phi = g.im / g.re; /* approx to atan2 */
			if (phi > 0.0) th2 = thm; else th1 = thm;
			if (fabs(phi) < EPS) cvg = 1;
			thm = 0.5 * (th1+th2);
		}
		unless (cvg) ast_log(LOG_ERROR, "mkfilter: warning: failed to converge\n");
	}
}

static void compute_notch(void)
{ /* compute Z-plane pole & zero positions for bandstop resonator (notch filter) */
	complex zz;
	double theta;
	compute_bpres();		/* iterate to place poles */
	theta = TWOPI * raw_alpha1;
	zz = expj(theta);	/* place zeros exactly */
	zplane.zeros[0] = zz;
	zplane.zeros[1] = cconj(zz);
}

static void expandpoly(void) /* given Z-plane poles & zeros, compute top & bot polynomials in Z, and then recurrence relation */
{
	complex topcoeffs[MAXPZ+1], botcoeffs[MAXPZ+1];
	int i;
	complex c1, c2;
	double theta;
	expand(zplane.zeros, zplane.numzeros, topcoeffs);
	expand(zplane.poles, zplane.numpoles, botcoeffs);
	c1.re = 1.0;
	c1.im = 0.0;
	dc_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, c1);
	theta = TWOPI * 0.5 * (raw_alpha1 + raw_alpha2); /* "jwT" for centre freq. */
	fc_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, expj(theta));
	c2.re = -1.0;
	c2.im = 0.0;
	hf_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, c2);
	for (i = 0; i <= zplane.numzeros; i++) xcoeffs[i] = +(topcoeffs[i].re / botcoeffs[zplane.numpoles].re);
	for (i = 0; i <= zplane.numpoles; i++) ycoeffs[i] = -(botcoeffs[i].re / botcoeffs[zplane.numpoles].re);
}

static void expand(complex pz[], int npz, complex coeffs[])
{ /* compute product of poles or zeros as a polynomial of z */
	int i;
	complex co1;
	co1.re = 1.0;
	co1.im = 0.0;
	coeffs[0] = co1;
	for (i=0; i < npz; i++) {
		complex c0;
		c0.re = 0.0;
		c0.im = 0.0;
		coeffs[i+1] = c0;
	}
	for (i=0; i < npz; i++) {
		multin(pz[i], npz, coeffs);
	}
	/* check computed coeffs of z^k are all real */
	for (i=0; i < npz+1; i++) {
		if (fabs(coeffs[i].im) > EPS) {
			ast_log(LOG_ERROR, "mkfilter: coeff of z^%d (%f) is not real; poles/zeros are not complex conjugates\n", i, fabs(coeffs[i].im));
		}
	}
}

/* multiply factor (z-w) into coeffs */
static void multin(complex w, int npz, complex coeffs[])
{
	complex nw = complexnegative(w);
	for (int i = npz; i >= 1; i--) {
		coeffs[i] = complexadd2((complexmult2(nw, coeffs[i])), coeffs[i-1]);
	}
	coeffs[0] = complexmult2(nw, coeffs[0]);
}

void mknotch(float freq,float bw,long *p1, long *p2, long *p3);
void mknotch(float freq, float bw, long *p1, long *p2, long *p3)
{
#define	NB 14
	float fsh;
    options = opt_re;
    qfactor = freq / bw;
	infq = 0;
    raw_alpha1 = freq / 8000.0;
    polemask = ~0;

    compute_notch();
    expandpoly();

    fsh = (float) (1 << NB);
    *p1 = (long)(xcoeffs[1] * fsh);
    *p2 = (long)(ycoeffs[0] * fsh);
    *p3 = (long)(ycoeffs[1] * fsh);
}

typedef struct {
	long	x1;
	long	x2;
	long	y1;
	long	y2;
	long	e1;
	long	e2;
	int	samps;
} sf_detect_state;

/* return 0 if nothing detected, 1 if lack of tone, 2 if presence of tone */
/* modifies buffer pointed to by 'amp' with notched-out values */
static inline int sf_detect(sf_detect_state *s, short *amp,
                 int samples, long p1, long p2, long p3)
{
	int i,rv = 0;
	long x,y;

/*! Default chunk size */
#define DAHDI_CHUNKSIZE		 8
#define	SF_DETECT_SAMPLES (DAHDI_CHUNKSIZE * 5)
#define	SF_DETECT_MIN_ENERGY 500
/* number of bits to shift left */
#define	NB 14

	/* determine energy level before filtering */
	for (i = 0; i < samples; i++) {
		if (amp[i] < 0) {
			s->e1 -= amp[i];
		} else {
			s->e1 += amp[i];
		}
	}
	/* do 2nd order IIR notch filter at given freq. and calculate
	    energy */
	for (i = 0; i < samples; i++) {
		x = amp[i] << NB;
		y = s->x2 + (p1 * (s->x1 >> NB)) + x;
		y += (p2 * (s->y2 >> NB)) +
		(p3 * (s->y1 >> NB));
		s->x2 = s->x1;
		s->x1 = x;
		s->y2 = s->y1;
		s->y1 = y;
		amp[i] = y >> NB;
		if (amp[i] < 0) {
			s->e2 -= amp[i];
		} else {
			s->e2 += amp[i];
		}
	}
	s->samps += i;
	/* if time to do determination */
	if ((s->samps) >= SF_DETECT_SAMPLES) {
		rv = 1; /* default to no tone */
		/* if enough energy, it is determined to be a tone */
		if (((s->e1 - s->e2) / s->samps) > SF_DETECT_MIN_ENERGY) rv = 2;
		/* reset energy processing variables */
		s->samps = 0;
		s->e1 = s->e2 = 0;
	}
	return(rv);
}

struct notch_information {
	struct ast_audiohook audiohook;
	float frequency;
	float bandwidth;
	unsigned short int tx;
	unsigned short int rx;
	long rxp1;
	long rxp2;
	long rxp3;
	sf_detect_state rd;
	unsigned int flags;
	unsigned short int state;
};

enum notch_flags {
	FLAG_TRANSMIT_DIR = (1 << 1),
	FLAG_RECEIVE_DIR = (1 << 2),
	FLAG_END_FILTER = (1 << 3),
};

AST_APP_OPTIONS(notch_opts, {
	AST_APP_OPTION('t', FLAG_TRANSMIT_DIR),
	AST_APP_OPTION('r', FLAG_RECEIVE_DIR),
	AST_APP_OPTION('d', FLAG_END_FILTER),
});

static void destroy_callback(void *data)
{
	struct notch_information *ni = data;

	/* Destroy the audiohook, and destroy ourselves */
	ast_audiohook_lock(&ni->audiohook);
	ast_audiohook_detach(&ni->audiohook);
	ast_audiohook_unlock(&ni->audiohook);
	ast_audiohook_destroy(&ni->audiohook);
	ast_free(ni);

	return;
}

/*! \brief Static structure for datastore information */
static const struct ast_datastore_info notch_datastore = {
	.type = "notch",
	.destroy = destroy_callback
};

static int notch_callback(struct ast_audiohook *audiohook, struct ast_channel *chan, struct ast_frame *frame, enum ast_audiohook_direction direction)
{
	struct ast_datastore *datastore = NULL;
	struct notch_information *ni = NULL;

	/* If the audiohook is stopping it means the channel is shutting down.... but we let the datastore destroy take care of it */
	if (audiohook->status == AST_AUDIOHOOK_STATUS_DONE)
		return 0;

	/* Grab datastore which contains our gain information */
	if (!(datastore = ast_channel_datastore_find(chan, &notch_datastore, NULL)))
		return 0;

	ni = datastore->data;

	if (frame->frametype == AST_FRAME_VOICE) { /* we're filtering out an in-band frequency */
		int newstate;
		/* Based on direction of frame, and confirm it is applicable */
		if (!(direction == AST_AUDIOHOOK_DIRECTION_READ ? &ni->rx : &ni->tx))
			return 0;
		/* Notch the sample now */
		newstate = sf_detect(&ni->rd, frame->data.ptr, frame->samples, ni->rxp1, ni->rxp2, ni->rxp3);
		/* if ni->state > 0 && ni->state != newstate, a state change has occured on this channel */
		ni->state = newstate;
	}

	return 0;
}

/*! \internal \brief Disable notch filtering on the channel */
static int remove_notch_filter(struct ast_channel *chan)
{
	struct ast_datastore *datastore = NULL;
	struct notch_information *data;
	SCOPED_CHANNELLOCK(chan_lock, chan);

	datastore = ast_channel_datastore_find(chan, &notch_datastore, NULL);
	if (!datastore) {
		ast_log(AST_LOG_WARNING, "Cannot remove NOTCH_FILTER from %s: NOTCH_FILTER not currently enabled\n",
		        ast_channel_name(chan));
		return -1;
	}
	data = datastore->data;

	if (ast_audiohook_remove(chan, &data->audiohook)) {
		ast_log(AST_LOG_WARNING, "Failed to remove NOTCH_FILTER audiohook from channel %s\n", ast_channel_name(chan));
		return -1;
	}

	if (ast_channel_datastore_remove(chan, datastore)) {
		ast_log(AST_LOG_WARNING, "Failed to remove NOTCH_FILTER datastore from channel %s\n",
		        ast_channel_name(chan));
		return -1;
	}
	ast_datastore_free(datastore);

	return 0;
}

static int notch_read(struct ast_channel *chan, const char *cmd, char *data, char *buffer, size_t buflen)
{
	char *parse;
	struct ast_datastore *datastore = NULL;
	struct notch_information *ni = NULL;
	struct ast_flags flags = { 0 };
	
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(frequency);
		AST_APP_ARG(options);
	);

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &notch_datastore, NULL))) {
		ast_channel_unlock(chan);
		return -1; /* function not initiated yet, so nothing to read */
	} else {
		ast_channel_unlock(chan);
		ni = datastore->data;
	}

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(notch_opts, &flags, NULL, args.options);
		ni->flags = flags.flags;
	} else {
		ni->flags = 0;
	}

	if (ast_strlen_zero(args.frequency)) {
		ast_log(LOG_ERROR, "Frequency must be specified for NOTCH_FILTER function\n");
		return -1;
	}
	if (sscanf(args.frequency, "%f", &ni->frequency) != 1) {
		ast_log(LOG_ERROR, "Frequency %s is invalid.\n", args.frequency);
		return -1;
	}

	/* print bandwidth only if matches the direction */
	if ((!ast_test_flag(&flags, FLAG_RECEIVE_DIR) && ni->tx == 1) ||
		(ast_test_flag(&flags, FLAG_RECEIVE_DIR) && ni->rx == 1)) {
		snprintf(buffer, buflen, "%f", ni->bandwidth);
	}

	return 0;
}

static int notch_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	char *parse;
	struct ast_datastore *datastore = NULL;
	struct notch_information *ni = NULL;
	int is_new = 0;
	struct ast_flags flags = { 0 };
	
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(frequency);
		AST_APP_ARG(options);
	);

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &notch_datastore, NULL))) {
		ast_channel_unlock(chan);
		/* Allocate a new datastore to hold the reference to this notch and audiohook information */
		if (!(datastore = ast_datastore_alloc(&notch_datastore, NULL)))
			return 0;
		if (!(ni = ast_calloc(1, sizeof(*ni)))) {
			ast_datastore_free(datastore);
			return 0;
		}
		ast_audiohook_init(&ni->audiohook, AST_AUDIOHOOK_TYPE_MANIPULATE, "Notch Filter", AST_AUDIOHOOK_MANIPULATE_ALL_RATES);
		ni->audiohook.manipulate_callback = notch_callback;
		is_new = 1;
	} else {
		ast_channel_unlock(chan);
		ni = datastore->data;
	}

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(notch_opts, &flags, NULL, args.options);
		ni->flags = flags.flags;
	} else {
		ni->flags = 0;
	}

	if (ast_test_flag(&flags, FLAG_END_FILTER)) {
		return remove_notch_filter(chan);
	}

	if (ast_strlen_zero(args.frequency)) {
		ast_log(LOG_ERROR, "Frequency must be specified for NOTCH_FILTER function\n");
		return -1;
	}

	if (sscanf(args.frequency, "%f", &ni->frequency) != 1) {
		ast_log(LOG_ERROR, "Frequency %s is invalid.\n", args.frequency);
		return -1;
	}

	if (ast_strlen_zero(value)) {
		ast_log(LOG_ERROR, "Bandwidth not specified.\n");
		return -1;
	}

	ni->bandwidth = atof(value); /* notch filter bandwidth, typically 10.0 */

	if (ni->bandwidth == 0.0) {
		ast_log(LOG_ERROR, "Bandwidth %s is invalid.\n", value);
		return -1;
	}

	ni->tx = 0;
	ni->rx = 0;
	if (ast_strlen_zero(args.options) || ast_test_flag(&flags, FLAG_TRANSMIT_DIR)) {
		ni->tx = 1;
	}
	if (ast_strlen_zero(args.options) || ast_test_flag(&flags, FLAG_RECEIVE_DIR)) {
		ni->rx = 1;
	}

	/* complex math to calculate the right parameters for notch filter */
	mknotch(ni->frequency,ni->bandwidth,&ni->rxp1,&ni->rxp2,&ni->rxp3);
	ast_debug(5, "Notch filter calculations for %f/%f: %ld, %ld, %ld", ni->frequency,ni->bandwidth,ni->rxp1,ni->rxp2,ni->rxp3);

	ni->rd.x1 = 0.0;
	ni->rd.x2 = 0.0;
	ni->rd.y1 = 0.0;
	ni->rd.y2 = 0.0;
	ni->rd.e1 = 0.0;
	ni->rd.e2 = 0.0;
	ni->rd.samps = 0;
	ni->state = 0;

	if (is_new) {
		datastore->data = ni;
		ast_channel_lock(chan);
		ast_channel_datastore_add(chan, datastore);
		ast_channel_unlock(chan);
		ast_audiohook_attach(chan, &ni->audiohook);
	}

	return 0;
}

static struct ast_custom_function notch_function = {
	.name = "NOTCH_FILTER",
	.read = notch_read,
	.write = notch_write,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&notch_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&notch_function);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Technology independent notch filter");
