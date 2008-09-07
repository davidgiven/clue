# Perl5 C runtime
#
# © 2008 David Given.
# Clue is licensed under the Revised BSD open source license. To get the
# full license text, see the README file.
#
# $Id$
# $HeadURL$
# $LastChangedDate: 2008-07-16 11:21:58 +0100 (Wed, 16 Jul 2008) $

my @clue_initializer_list = ();

sub clue_add_initializer
{
	my ($fn) = (@_);
	push @clue_initializer_list, $fn; 
}

sub clue_run_initializers
{
	grep $_->(), @clue_initializer_list;
	@clue_initializer_list = [];
}

sub clue_newstring
{
	my ($s) = @_;
	my @d = split(//, $s);
	
	for my $c (@d)
	{
		$c = ord($c);
	}
	
	$d[length($s)] = 0;
	return 0, \@d;
}

sub clue_ptr_to_string
{
	my ($po, $pd) = @_;
	my @s = ();

	for (;;)
	{
		my $c = $pd->[$po];
		if ($c == 0)
		{
			last;
		}
		else
		{
			push @s, chr($c);
		}
		$po++;
	}
	
	return join("", @s);
}
