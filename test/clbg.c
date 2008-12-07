/* CLBG benchmark driver.
 *
 * This file is available under the Revised BSD open source license. To get
 * the full license text, see:
 * http://shootout.alioth.debian.org/gp4/miscfile.php?file=license&title=revised%20BSD%20license
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "clbg.h"

static double dtime(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (double)now.tv_sec + (double)now.tv_usec/1000000.0;
}

int main(int argc, const char* argv[])
{
	double starttime = dtime();
	int i = clbgmain(argc, argv);
	double elapsed = dtime() - starttime;

	printf("\nELAPSEDTIME=%f\n", elapsed);
	return i;
}

