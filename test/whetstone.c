/* Whetstone benchmark.
 *
 * This file is, as far as I know, public domain. Please contact me if you
 * believe this is not the case.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

static double loop_time[9];
static double loop_mops[9];
static double loop_mflops[9];
static double mwips;
static char headings[9][18];
static double Check;
static double results[9];
static double TimeUsed;

double dtime(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (double)now.tv_sec + (double)now.tv_usec/1000000.0;
}

void pa(double e[4], double t, double t2)
{
	long j;
	for (j = 0; j < 6; j++)
	{
		e[0] = (e[0] + e[1] + e[2] - e[3]) * t;
		e[1] = (e[0] + e[1] - e[2] + e[3]) * t;
		e[2] = (e[0] - e[1] + e[2] + e[3]) * t;
		e[3] = (-e[0] + e[1] + e[2] + e[3]) / t2;
	}

	return;
}

void po(double e1[4], long j, long k, long l)
{
	e1[j] = e1[k];
	e1[k] = e1[l];
	e1[l] = e1[j];
	return;
}

void p3(double *x, double *y, double *z, double t, double t1, double t2)
{
	*x = *y;
	*y = *z;
	*x = t * (*x + *y);
	*y = t1 * (*x + *y);
	*z = (*x + *y) / t2;
	return;
}

void pout(char title[18], double ops, int type, double checknum, double time,
		int calibrate, int section)
{
	double mops, mflops;

	Check = Check + checknum;
	loop_time[section] = time;
	strcpy(headings[section], title);
	TimeUsed = TimeUsed + time;
	if (calibrate == 1)

	{
		results[section] = checknum;
	}
	if (calibrate == 0)
	{
		printf("%s %24.17f    ", headings[section], results[section]);

		if (type == 1)
		{
			if (time > 0)
			{
				mflops = ops / (1000000L * time);
			}
			else
			{
				mflops = 0;
			}
			loop_mops[section] = 99999;
			loop_mflops[section] = mflops;
			printf(" %9.3f          %9.3f\n", loop_mflops[section],
					loop_time[section]);
		}
		else
		{
			if (time > 0)
			{
				mops = ops / (1000000L * time);
			}
			else
			{
				mops = 0;
			}
			loop_mops[section] = mops;
			loop_mflops[section] = 0;
			printf("           %9.3f%9.3f\n", loop_mops[section],
					loop_time[section]);
		}
	}

	return;
}

void whetstones(long xtra, long x100, int calibrate)
{

	long n1, n2, n3, n4, n5, n6, n7, n8, i, ix, n1mult;
	double x, y, z;
	long j, k, l;
	double e1[4], timea, timeb;

	double t = 0.49999975;
	double t0 = t;
	double t1 = 0.50000025;
	double t2 = 2.0;

	Check = 0.0;

	n1 = 12 * x100;
	n2 = 14 * x100;
	n3 = 345 * x100;
	n4 = 210 * x100;
	n5 = 32 * x100;
	n6 = 899 * x100;
	n7 = 616 * x100;
	n8 = 93 * x100;
	n1mult = 10;

	/* Section 1, Array elements */

	e1[0] = 1.0;
	e1[1] = -1.0;
	e1[2] = -1.0;
	e1[3] = -1.0;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n1 * n1mult; i++)
			{
				e1[0] = (e1[0] + e1[1] + e1[2] - e1[3]) * t;
				e1[1] = (e1[0] + e1[1] - e1[2] + e1[3]) * t;
				e1[2] = (e1[0] - e1[1] + e1[2] + e1[3]) * t;
				e1[3] = (-e1[0] + e1[1] + e1[2] + e1[3]) * t;
			}
			t = 1.0 - t;
		}
		t = t0;
	}
	timeb = (dtime() - timea) / (double) (n1mult);
	pout("N1 floating point\0", (double) (n1 * 16) * (double) (xtra), 1, e1[3],
			timeb, calibrate, 1);

	/* Section 2, Array as parameter */

	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n2; i++)
			{
				pa(e1, t, t2);
			}
			t = 1.0 - t;
		}
		t = t0;
	}
	timeb = dtime() - timea;
	pout("N2 floating point\0", (double) (n2 * 96) * (double) (xtra), 1, e1[3],
			timeb, calibrate, 2);

	/* Section 3, Conditional jumps */
	j = 1;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n3; i++)
			{
				if (j == 1)
					j = 2;
				else
					j = 3;
				if (j > 2)
					j = 0;
				else
					j = 1;
				if (j < 1)
					j = 1;
				else
					j = 0;
			}
		}
	}
	timeb = dtime() - timea;
	pout("N3 if then else  \0", (double) (n3 * 3) * (double) (xtra), 2,
			(double) (j), timeb, calibrate, 3);

	/* Section 4, Integer arithmetic */
	j = 1;
	k = 2;
	l = 3;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n4; i++)
			{
				j = j * (k - j) * (l - k);
				k = l * k - (l - j) * k;
				l = (l - k) * (k + j);
				e1[l - 2] = j + k + l;
				e1[k - 2] = j * k * l;
			}
		}
	}
	timeb = dtime() - timea;
	x = e1[0] + e1[1];
	pout("N4 fixed point   \0", (double) (n4 * 15) * (double) (xtra), 2, x,
			timeb, calibrate, 4);

	/* Section 5, Trig functions */
	x = 0.5;
	y = 0.5;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 1; i < n5; i++)
			{
				x = t * atan(t2 * sin(x) * cos(x) / (cos(x + y) + cos(x - y)
						- 1.0));
				y = t * atan(t2 * sin(y) * cos(y) / (cos(x + y) + cos(x - y)
						- 1.0));
			}
			t = 1.0 - t;
		}
		t = t0;
	}
	timeb = dtime() - timea;
	pout("N5 sin,cos etc.  \0", (double) (n5 * 26) * (double) (xtra), 2, y,
			timeb, calibrate, 5);

	/* Section 6, Procedure calls */
	x = 1.0;
	y = 1.0;
	z = 1.0;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n6; i++)
			{
				p3(&x, &y, &z, t, t1, t2);
			}
		}
	}
	timeb = dtime() - timea;
	pout("N6 floating point\0", (double) (n6 * 6) * (double) (xtra), 1, z,
			timeb, calibrate, 6);

	/* Section 7, Array refrences */
	j = 0;
	k = 1;
	l = 2;
	e1[0] = 1.0;
	e1[1] = 2.0;
	e1[2] = 3.0;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n7; i++)
			{
				po(e1, j, k, l);
			}
		}
	}
	timeb = dtime() - timea;
	pout("N7 assignments   \0", (double) (n7 * 3) * (double) (xtra), 2, e1[2],
			timeb, calibrate, 7);

	/* Section 8, Standard functions */
	x = 0.75;
	timea = dtime();
	{
		for (ix = 0; ix < xtra; ix++)
		{
			for (i = 0; i < n8; i++)
			{
				x = sqrt(exp(log(x) / t1));
			}
		}
	}
	timeb = dtime() - timea;
	pout("N8 exp,sqrt etc. \0", (double) (n8 * 4) * (double) (xtra), 2, x,
			timeb, calibrate, 8);

	return;
}

int main(int argc, const char* argv[])
{
	int count = 10;
	int calibrate = 1;
	long xtra = 1;
	int section;
	long x100 = 100;
	int duration = 100;

	printf("Calibrate\n");
	do
	{
		TimeUsed = 0;

		whetstones(xtra, x100, calibrate);

		printf("%11.2f Seconds %10.0f   Passes (x 100)\n", TimeUsed,
				(double) (xtra));
		calibrate++;
		count--;

		if (TimeUsed > 2.0)
		{
			count = 0;
		}
		else
		{
			xtra = xtra * 5;
		}
	}
	while (count > 0);

	if (TimeUsed > 0)
		xtra = (long) ((double) (duration * xtra) / TimeUsed);
	if (xtra < 1)
		xtra = 1;

	calibrate = 0;

	printf("\nUse %d  passes (x 100)\n", xtra);

	printf("\n          C/C++ Whetstone Benchmark");
	printf("\n");

	printf("\nLoop content                  Result              MFLOPS "
		"     MOPS   Seconds\n\n");

	TimeUsed = 0;
	whetstones(xtra, x100, calibrate);

	printf("\nMWIPS            ");
	if (TimeUsed > 0)
	{
		mwips = (float) (xtra) * (float) (x100) / (10 * TimeUsed);
	}
	else
	{
		mwips = 0;
	}

	printf("%39.3f%19.3f\n\n", mwips, TimeUsed);

	if (Check == 0)
		printf("Wrong answer  ");

	return 0;
}
