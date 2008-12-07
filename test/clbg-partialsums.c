/* Hacked CLBG benchmark.
 *
 * This file is available under the Revised BSD open source license. To get
 * the full license text, see:
 * http://shootout.alioth.debian.org/gp4/miscfile.php?file=license&title=revised%20BSD%20license
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

/*
** The Computer Language Shootout
** http://shootout.alioth.debian.org/
** contributed by Mike Pall
** de-optimized by Isaac Gouy
**
** compile with:
**   gcc -O3 -fomit-frame-pointer -ffast-math -o partialsums partialsums.c -lm
**   Adding -march=<yourcpu> may help, too.
**   On a P4/K8 or later try adding: --march=<yourcpu> -mfpmath=sse -msse2
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "clbg.h"

int clbgmain(int argc, const char **argv)
{
  int k, n = atoi(argv[1]);
  double sum, a;

/*
** Yes, I (Mike Pall) tried using a double as a primary or secondary loop variable.
** But the x86 ABI requires a cleared x87 FPU stack before every call
** (e.g. to sin()) which nullifies any performance gains.
**
** Combining all loops does not pay off because the x87 FPU has to shuffle
** stack slots and/or runs out of registers. This may not be entirely true
** for SSE2 with fully inlined FPU code (-ffast-math required). Dito for
** other CPUs with a register-based FPU and a sane FP ABI.
**
** Auto vectorization may be a bit easier with separate loops, too.
*/

#define kd ((double)k)

  sum = 0.0;
  for (k = 0; k <= n; k++) sum += pow(2.0/3.0, kd);
  printf("%.9f\t(2/3)^k\n", sum);

  sum = 0.0;
  for (k = 1 ; k <= n; k++) sum += 1/sqrt(kd);  /* aka pow(kd, -0.5) */
  printf("%.9f\tk^-0.5\n", sum);

  sum = 0.0;
  for (k = 1; k <= n; k++) sum += 1.0/(kd*(kd+1.0));
  printf("%.9f\t1/k(k+1)\n", sum);

  sum = 0.0;
  for (k = 1; k <= n; k++) {
    double sk = sin(kd);
    sum += 1.0/(kd*kd*kd*sk*sk);
  }
  printf("%.9f\tFlint Hills\n", sum);

  sum = 0.0;
  for (k = 1; k <= n; k++) {
    double ck = cos(kd);
    sum += 1.0/(kd*kd*kd*ck*ck);
  }
  printf("%.9f\tCookson Hills\n", sum);

  sum = 0.0;
  for (k = 1; k <= n; k++) sum += 1.0/kd;
  printf("%.9f\tHarmonic\n", sum);

  sum = 0.0;
  for (k = 1; k <= n; k++) sum += 1.0/(kd*kd);
  printf("%.9f\tRiemann Zeta\n", sum);

  sum = 0.0; a = -1.0;
  for (k = 1; k <= n; k++) sum += (a = -a)/kd;
  printf("%.9f\tAlternating Harmonic\n", sum);

  sum = 0.0;  a = -1.0;
  for (k = 1; k <= n; k++) sum += (a = -a)/(2.0*kd - 1.0);
  printf("%.9f\tGregory\n", sum);

  return 0;
}

