/* Hacked CLBG benchmark.
 *
 * This file is available under the Revised BSD open source license. To get
 * the full license text, see:
 * http://shootout.alioth.debian.org/gp4/miscfile.php?file=license&title=revised%20BSD%20license
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

/*
 * The Computer Lannguage Shootout
 * http://shootout.alioth.debian.org/
 * Contributed by Heiner Marxen
 *
 * "fannkuch"	for C gcc
 *
 * $Id: fannkuch-icc.code,v 1.20 2008-04-30 21:57:33 igouy-guest Exp $
 */

#include <stdio.h>
#include <stdlib.h>

#define Int	int
#define Aint	int

    static long
fannkuch( int n )
{
    Aint*	perm;
    Aint*	perm1;
    Aint*	count;
    long	flips;
    long	flipsMax;
    Int		r;
    Int		i;
    Int		k;
    Int		didpr;
    const Int	n1	= n - 1;

    if( n < 1 ) return 0;

    perm  = calloc(n, sizeof(*perm ));
    perm1 = calloc(n, sizeof(*perm1));
    count = calloc(n, sizeof(*count));

    for( i=0 ; i<n ; ++i ) perm1[i] = i;	/* initial (trivial) permu */

    r = n; didpr = 0; flipsMax = 0;
    for(;;) {
	if( didpr < 30 ) {
	    for( i=0 ; i<n ; ++i ) printf("%d", (int)(1+perm1[i]));
	    printf("\n");
	    ++didpr;
	}
	for( ; r!=1 ; --r ) {
	    count[r-1] = r;
	}

#define XCH(x,y)	{ Aint t_mp; t_mp=(x); (x)=(y); (y)=t_mp; }

	if( ! (perm1[0]==0 || perm1[n1]==n1) ) {
	    flips = 0;
	    for( i=1 ; i<n ; ++i ) {	/* perm = perm1 */
		perm[i] = perm1[i];
	    }
	    k = perm1[0];		/* cache perm[0] in k */
	    do {			/* k!=0 ==> k>0 */
		Int	j;
		for( i=1, j=k-1 ; i<j ; ++i, --j ) {
		    XCH(perm[i], perm[j])
		}
		++flips;
		/*
		 * Now exchange k (caching perm[0]) and perm[k]... with care!
		 * XCH(k, perm[k]) does NOT work!
		 */
		j=perm[k]; perm[k]=k ; k=j;
	    }while( k );
	    if( flipsMax < flips ) {
		flipsMax = flips;
	    }
	}

	for(;;) {
	    if( r == n ) {
		return flipsMax;
	    }
	    /* rotate down perm[0..r] by one */
	    {
		Int	perm0 = perm1[0];
		i = 0;
		while( i < r ) {
		    k = i+1;
		    perm1[i] = perm1[k];
		    i = k;
		}
		perm1[r] = perm0;
	    }
	    if( (count[r] -= 1) > 0 ) {
		break;
	    }
	    ++r;
	}
    }
}

    int
main( int argc, char* argv[] )
{
    int		n = (argc>1) ? atoi(argv[1]) : 0;

    printf("Pfannkuchen(%d) = %d\n", n, fannkuch(n));
    return 0;
}
