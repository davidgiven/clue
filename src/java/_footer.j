/* footer.j
 * Footer boilerplate.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

	public static double run(double fp, ClueMemory stack, double argc,
		double argvpd, ClueMemory argvpo)
	{
		args.doubledata[0] = fp;
		args.objectdata[1] = stack;
		args.doubledata[2] = argc;
		args.doubledata[3] = argvpd;
		args.objectdata[4] = argvpo;
		_main.run();
		return args.doubledata[0];
	}
}
