/* libc.js
 * Javascript libc.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id$
 * $HeadURL$
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

/* Load the Clue crt and libc. */

load("src/javascript/crt.js");
load("src/javascript/libc.js");

/* Load the Clue compiled program. */

var argc = 1;
while (arguments[argc] != "--")
{
	if (!arguments[argc])
	{
		print("Command line did not contain a --");
		quit();
	}
	
	load(arguments[argc]);
	argc++;
}

/* And run it. */

{
	var cargs = [];
	
	for (var i = argc+1; i < arguments.length; i++)
	{
		var v = clue_newstring(arguments[i]);
		cargs.push(0);
		cargs.push(v);
	}
	
    clue_run_initializers();
    _main(0, [], arguments.length - argc, 0, cargs);
    quit();
}
