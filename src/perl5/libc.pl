# Perl5 C runtime
#
# © 2008 David Given.
# Clue is licensed under the Revised BSD open source license. To get the
# full license text, see the README file.
#
# $Id: cluerun 20 2008-07-16 10:21:58Z dtrg $
# $HeadURL: https://cluecc.svn.sourceforge.net/svnroot/cluecc/clue/cluerun $
# $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

use Time::HiRes qw(gettimeofday);

#############################################################################
#                                 MEMORY                                    #
#############################################################################

$_malloc = sub
{
	return 0, [];
};

$_calloc = sub
{
	my ($stackpo, $stackpd, $s1, $s2) = @_;
	my @d = ();
	
	for $i (0 .. ($s1 * $s2))
	{
		$d[$i] = 0;
	}
	
	return 0, \@d;
};

#############################################################################
#                                 STRINGS                                   #
#############################################################################

$_strcpy = sub
{
	my ($stackpo, $stackpd, $destpo, $destpd, $srcpo, $srcpd) = @_;
	my $origdestpo = $destpo;
	
	my $c;
	do
	{
		$c = $srcpd->[$srcpo];
		$destpd->[$destpo] = $c;
		$srcpo++;
		$destpo++;
	}
	while ($c != 0);
	
	return $origdestpo, $destpd;
};
 	
$_memset = sub
{
	my ($stackpo, $stackpd, $destpo, $destpd, $c, $n) = @_;
	my $origdestpo = $destpo;
	
	while ($n > 0)
	{
		$destpd->[$destpo] = $c;
		$n--;
	}
	
	return $origdestpo, $destpd;
};

#############################################################################
#                                  STDIO                                    #
#############################################################################

$| = 1; # force flush on every write

$_printf = sub
{
	my ($stackpo, $stackpd, $formatpo, $formatpd) = @_;
	my $format = clue_ptr_to_string($formatpo, $formatpd);
	my @outargs = ();
	
	my $numargs = $#_;
	my $i = 4;
	while ($i <= $numargs)
	{
		my $thisarg = $_[$i];
		$i++;
		if ($i < $numargs)
		{
			my $nextarg = $_[$i];
			if (ref $nextarg)
			{
				# If the next argument is NULL or a table, then we must
				# be looking at a register pair representing a pointer.
				$thisarg = clue_ptr_to_string($thisarg, $nextarg);
				$i++;
			}
		}
		
		push @outargs, $thisarg;
	}
	
	print sprintf($format, @outargs);
	
	return 1;
};

$_putc = sub
{
	my ($stackpo, $stackpd, $c, $fppo, $fppd) = @_;
	printf sprintf("%c", $c);
};

$_atoi = sub
{
	my ($stackpo, $stackpd, $srcpo, $srcpd) = @_;
	my $s = clue_ptr_to_string($srcpo, $srcpd);
	return $s | 0;
};

$_atol = $_atoi;
	
#############################################################################
#                                   TIME                                    #
#############################################################################

$_gettimeofday = sub
{
	my ($sp, $stack, $tvpo, $tvpd, $tzpo, $tzpd) = @_;
	my ($secs, $usecs) = gettimeofday;

	$tvpd->[$tvpo+0] = $secs;
	$tvpd->[$tvpo+1] = $usecs;
	return 0;
};

#############################################################################
#                                  MATHS                                    #
#############################################################################

$_sin = sub { return sin($_[2]); };
$_cos = sub { return cos($_[2]); };
$_atan = sub { return atan2($_[2], 1); };
$_log = sub { return log($_[2]); };
$_exp = sub { return exp($_[2]); };
$_sqrt = sub { return sqrt($_[2]); };
$_pow = sub { return ($_[2] ** $_[3]); };
