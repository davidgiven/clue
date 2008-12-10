Some notes on performance as I optimise the code generator. These numbers are
NWIPS.

Using Object to store all values, boxing doubles, etc:               81
Change ClueRunnable to a class, not an interface:                    77
Making lots of stuff final:                                          87
Using ClueMemory instead of Object[] and casting:                   100
Making globals public rather than private:                          100
Storing arguments in a ClueMemory:                                  124
(Native)                                                            (708)


$Id$
$HeadURL$
$LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $

