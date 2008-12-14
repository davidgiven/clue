/* ClueRuntime.java
 * Startup code.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

import java.lang.System;
import java.util.Vector;

class ClueRuntime
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
		final ClueMemory[] objectdata;
		final ClueRunnable[] functiondata;
		
		public ClueMemory(int size)
		{
			length = size;
			doubledata = new double[size];
			objectdata = new ClueMemory[size];
			functiondata = new ClueRunnable[size];
		}
		
		int intOf(int index)
		{
			return (int) doubledata[index];
		}
		
		ClueMemory objectOf(int index)
		{
			return objectdata[index];
		}
	};
	
	protected static class ClueRunnable
	{
		public void run()
		{
		}
	}

	private static final ClueMemory stringToPtr(String s)
	{
		ClueMemory pd = new ClueMemory(s.length() + 1);
		
		for (int i=0; i<s.length(); i++)
			pd.doubledata[i] = (double) s.charAt(i);
		pd.doubledata[s.length()] = 0;
			
		return pd;
	}
	
	private static final String ptrToString(int formatpo, ClueMemory formatpd)
	{
		int i = (int) formatpo;
		StringBuffer s = new StringBuffer();
		
		for (;;)
		{
			int c = formatpd.intOf(i++);
			if (c == 0)
				break;
			
			s.append((char) c);
		}
		
		return s.toString();
	}
	
	protected static final ClueRunnable _malloc = new ClueRunnable()
	{
		public void run()
		{
			int size = args.intOf(2);
			
			args.doubledata[0] = 0;
			args.objectdata[1] = new ClueMemory(size); 
		}
	};
	
	protected static final ClueRunnable _calloc = new ClueRunnable()
	{
		public void run()
		{
			int x = args.intOf(2);
			int y = args.intOf(3);
			
			args.doubledata[0] = 0;
			args.objectdata[1] = new ClueMemory(x*y); 
		}
	};
	
	protected static final ClueRunnable _free = new ClueRunnable()
	{
		public void run()
		{
			/* Yes, intentionally a noop. */
		}
	};
	
	protected static final ClueRunnable _memset = new ClueRunnable()
	{
		public void run()
		{
			int po = args.intOf(2);
			ClueMemory pd = args.objectOf(3);
			int c = args.intOf(4);
			int n = args.intOf(5);
			
			while (n > 0)
			{
				pd.doubledata[po] = c;
				pd.objectdata[po] = null;
				pd.functiondata[po] = null;
				po++;
				n--;
			}
			
			args.doubledata[0] = args.intOf(2);
			args.objectdata[1] = args.objectOf(3);
		}
	};
	
	protected static final ClueRunnable _gettimeofday = new ClueRunnable()
	{
		public void run()
		{
			int tvpo = args.intOf(2);
			ClueMemory tvpd = args.objectdata[3];
			
			long t = System.currentTimeMillis();
			tvpd.doubledata[tvpo+0] = (int) (t / 1000);
			tvpd.doubledata[tvpo+1] = (int) ((t % 1000) * 1000);
			
			args.doubledata[0] = 0;
		}
	};
	
	protected static final ClueRunnable _strcpy = new ClueRunnable()
	{
		public void run()
		{
			int destpo = args.intOf(2);
			ClueMemory destpd = args.objectdata[3];
			int srcpo = args.intOf(4);
			ClueMemory srcpd = args.objectdata[5];
			
			for (;;)
			{
				int c = srcpd.intOf(srcpo++);
				destpd.doubledata[destpo++] = c;
				
				if (c == 0)
					break;
			}
			
			args.doubledata[0] = destpo;
			args.objectdata[1] = destpd;
		}
	};
	
	protected static final ClueRunnable _printf = new ClueRunnable()
	{
		public void run()
		{
			int formatpo = args.intOf(2);
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
								int po = args.intOf(argindex++);
								ClueMemory pd = args.objectdata[argindex++];
								outargs.add(ptrToString(po, pd));
								break innerloop;
							}
							
							case 'i':
							case 'u':
								/* Replace unknown integer format chars with %d. */
								format = format.substring(0, i-1) + 'd' +
									format.substring(i);
								/* ...then fall through. */
							case 'd':
							case 'o':
							case 'x':
							case 'X':
							{
								int v = args.intOf(argindex++);
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
			args.doubledata[0] = 0;
		}
	};
	
	protected static final ClueRunnable _atol = new ClueRunnable()
	{
		public void run()
		{
			int formatpo = args.intOf(2);
			ClueMemory formatpd = args.objectdata[3];
			String s = ptrToString(formatpo, formatpd);

			args.doubledata[0] = Integer.valueOf(s);
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

	protected static final ClueRunnable _pow = new ClueRunnable()
	{
		public void run()
		{
			args.doubledata[0] = Math.pow(args.doubledata[2],
					args.doubledata[3]);
		}
	};

	// --- Main runtime ---
	
	public static void main(String[] argv)
	{
		ClueMemory argvobj = new ClueMemory((argv.length+1) * 2);
		
		int i = 0;
		while (i < argv.length)
		{
			argvobj.doubledata[i*2 + 0] = 0;
			argvobj.objectdata[i*2 + 1] = stringToPtr(argv[i]);
			i++;
		}
		argvobj.doubledata[i*2 + 0] = 0;
		argvobj.objectdata[i*2 + 1] = null;
		
		args.doubledata[0] = 0;
		args.objectdata[1] = new ClueMemory(4096);
		args.doubledata[2] = argv.length;
		args.doubledata[3] = 0;
		args.objectdata[4] = argvobj;
			
		ClueProgram.runMain();
	}
}
