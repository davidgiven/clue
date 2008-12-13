Some notes on performance as I optimise the code generator. These numbers are
NWIPS.

                                                            A    B    C    D
Using Object to store all values, boxing doubles, etc:     81
Change ClueRunnable to a class, not an interface:          77
Making lots of stuff final:                                87
Using ClueMemory instead of Object[] and casting:         100
Making globals public rather than private:                100
Storing arguments in a ClueMemory:                        124  322  152   67
Use longs to store integer variables, not doubles:             167
Use ints to store integer variables, not longs:           123  321
Use ints to store integers in memory, not doubles [1]:    126  309

(Native)                                                         (708)

A: Sun JRE Hotspot Client 1.6.0_10-b33
B: Sun JRE Hotspot Server 1.6.0_10-b33
C: OpenJDK Server IcedTea6 1.3.1 1.6.0_0-b12
D: CACAO 0.99.3+hg

I only realised that the default Sun Java uses -client, which is really slow,
once I was quite a long way into the process.


[1] This *drastically* changed the shape of the benchmarks, making the program
terminate much sooner --- but giving much the same results!


$Id$
$HeadURL$
$LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $

