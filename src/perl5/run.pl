# Perl5 runtime loader
#
# © 2008 David Given.
# Clue is licensed under the Revised BSD open source license. To get the
# full license text, see the README file.
#
# $Id$
# $HeadURL$
# $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

sub include
{
	my ($file) = @_;
	my $return;
	
	unless ($return = do $file) {
		die "couldn't parse $file: $@" if $@;
		die "couldn't do $file: $!"    unless defined $return;
	}
}

include("src/perl5/crt.pl");
include("src/perl5/libc.pl");

# Load the Clue compiled program.

my $argc = 1;
while ($ARGV[$argc] ne "--")
{
#	if (!arguments[argc])
	#{
		#print("Command line did not contain a --");
		#quit();
	#}
	
	include($ARGV[$argc]);
	$argc++;
}

# And run it.

{
	my @cargs = ();
	
	for (my $i = $argc+1; $i <= $#ARGV; $i++)
	{
		my ($vo, $vd) = clue_newstring($ARGV[$i]);
		push @cargs, $vo;
		push @cargs, $vd;
	}
	
    clue_run_initializers();
    $_main->(0, [], $#ARGV - $argc, 0, \@cargs);
}
