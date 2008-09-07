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
 * The Computer Language Benchmarks Game
 * http://shootout.alioth.debian.org/

 * contributed by bearophile, Jan 24 2006
 * modified by wolfjb, Feb 28 2007
 * modified by alex burlyga, may 16, 2008
 */
#include <stdio.h>
#include <stdlib.h>

static inline int ack(register int x, register int y) __attribute__((const));
static inline int fib(int n) __attribute__((const));
static inline double fibFP(double n) __attribute__((const));
static inline int tak(int x, int y, int z) __attribute__((const));
static inline double takFP(double x, double y, double z) __attribute__((const));

static inline int ack(register int x, register int y){
  if (x == 0) {
    return y + 1;
  }

  return ack(x - 1, ((y | 0) ? ack(x, y - 1) : 1));
}

static inline int fib(register int n) {
  if (n < 2) {
    return 1;
  }
  return fib(n - 2) + fib(n - 1);
}

static inline double fibFP(double n) {
  if (n < 2.0) {
    return 1.0;
  }
  return fibFP(n - 2.0) + fibFP(n - 1.0);
}

static inline int tak(register int x, register int y, register int z) {
  if (y < x) {
    return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y));
  }
  return z;
}

static inline double takFP(double x, double y, double z) {
    if (y < x)
        return takFP( takFP(x-1.0, y, z), takFP(y-1.0, z, x), takFP(z-1.0, x, y) );
    return z;
}

int
main(int argc, char ** argv) {
  int n = 3;

  if(argc > 1) {
    n = atoi(argv[1]) - 1;
  }

  printf("Ack(3,%d): %d\n", n + 1, ack(3, n+1));
  printf("Fib(%.1f): %.1f\n", 28.0 + n, fibFP(28.0+n));
  printf("Tak(%d,%d,%d): %d\n", 3 * n, 2 * n, n, tak(3*n, 2*n, n));
  printf("Fib(3): %d\n", fib(3));
  printf("Tak(3.0,2.0,1.0): %.1f\n", takFP(3.0, 2.0, 1.0));

  return 0;
}
