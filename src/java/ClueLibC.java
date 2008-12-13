/* ClueLibC.java
 * Java libc and run time system.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

import java.util.Vector;

class ClueLibC
{
	protected static final ClueMemory args = new ClueMemory(64);
	
	protected static final class RegPair
	{
		final int i;
		final ClueMemory o;
		
		public RegPair(int i, ClueMemory o)
		{
			this.i = i;
			this.o = o;
		}
	};
	
	public static final class ClueMemory
	{
		final int length;
		final double[] doubledata;
		final int[] intdata;
		final ClueMemory[] objectdata;
		final ClueRunnable[] functiondata;
		
		public ClueMemory(int size)
		{
			length = size;
			doubledata = new double[size];
			intdata = new int[size];
			objectdata = new ClueMemory[size];
			functiondata = new ClueRunnable[size];
		}
	};
	
	protected static class ClueRunnable
	{
		public void run()
		{
		}
	}
	
	private static final String ptrToString(int formatpo, ClueMemory formatpd)
	{
		int i = (int) formatpo;
		StringBuffer s = new StringBuffer();
		
		for (;;)
		{
			int c = formatpd.intdata[i++];
			if (c == 0)
				break;
			
			s.append((char) c);
		}
		
		return s.toString();
	}
	
	protected static final ClueRunnable _gettimeofday = new ClueRunnable()
	{
		public void run()
		{
			int tvpo = args.intdata[2];
			ClueMemory tvpd = args.objectdata[3];
			
			long t = System.currentTimeMillis();
			tvpd.intdata[tvpo+0] = (int) (t / 1000);
			tvpd.intdata[tvpo+1] = (int) ((t % 1000) * 1000);
			
			args.intdata[0] = 0;
		}
	};
	
	protected static final ClueRunnable _strcpy = new ClueRunnable()
	{
		public void run()
		{
			int destpo = args.intdata[2];
			ClueMemory destpd = args.objectdata[3];
			int srcpo = args.intdata[4];
			ClueMemory srcpd = args.objectdata[5];
			
			for (;;)
			{
				int c = srcpd.intdata[srcpo++];
				destpd.intdata[destpo++] = c;
				
				if (c == 0)
					break;
			}
			
			args.intdata[0] = destpo;
			args.objectdata[1] = destpd;
		}
	};
	
	protected static final ClueRunnable _printf = new ClueRunnable()
	{
		public void run()
		{
			int formatpo = args.intdata[2];
			ClueMemory formatpd = args.objectdata[3];
			String format = ptrToString(formatpo, formatpd);

			Vector<Object> outargs = new Vector<Object>();
			int argindex = 4;
			int i = 0;
			while (i < format.length())
			{
				if (format.charAt(i++) == '%')
				{
					innerloop: for (;;)
					{
						char c = format.charAt(i++);
						switch (c)
						{
							case '%':
								break innerloop;
							
							case '.':
							case '0':
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
							case '8':
							case '9':
								continue innerloop;
								
							case 's':
							{
								int po = args.intdata[argindex++];
								ClueMemory pd = args.objectdata[argindex++];
								outargs.add(ptrToString(po, pd));
								break innerloop;
							}
							
							case 'd':
							case 'u':
							{
								int v = args.intdata[argindex++];
								outargs.add(new Integer(v));
								break innerloop;
							}
							
							case 'f':
							case 'e':
							case 'g':
							{
								double v = args.doubledata[argindex++];
								outargs.add(new Double(v));
								break innerloop;
							}
						}
					}
				}
			}
			
			System.out.printf(format, outargs.toArray());
			args.intdata[0] = 0;
		}
	};
	
	protected static final ClueRunnable _sin = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.sin(args.doubledata[2]);
		}
	};
	
	protected static final ClueRunnable _cos = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.cos(args.doubledata[2]);
		}
	};
	
	protected static final ClueRunnable _atan = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.atan(args.doubledata[2]);
		}
	};
	
	protected static final ClueRunnable _log = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.log(args.doubledata[2]);
		}
	};
	
	protected static final ClueRunnable _exp = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.exp(args.doubledata[2]);
		}
	};
	
	protected static final ClueRunnable _sqrt = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.sqrt(args.doubledata[2]);
		}
	};
}
