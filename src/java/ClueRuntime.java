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

class ClueRuntime
{
	public static void main(String[] argv)
	{
		ClueLibC.ClueMemory stack = new ClueLibC.ClueMemory(4096); 
		ClueProgram.run(0, stack, 0, 0, null);
	}
}
