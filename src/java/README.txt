Some notes on performance as I optimise the code generator. These numbers are
NWIPS.

Using Object to store all values, boxing doubles, etc:               81
Change ClueRunnable to a class, not an interface:                    77
Making lots of stuff final:                                          87
Using ClueMemory instead of Object[] and casting:                   100
Making globals public rather than private:                          100
Storing arguments in a ClueMemory:                                  124
(Native)                                                            (708)

This is all using the following Java engine:

java version "1.6.0_10"
Java(TM) SE Runtime Environment (build 1.6.0_10-b33)
Java HotSpot(TM) Client VM (build 11.0-b15, mixed mode, sharing)



$Id$
$HeadURL$
$LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $

