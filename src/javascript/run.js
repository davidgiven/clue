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

/* Some setup. */

var fs = require("fs");
var sys = require("sys");

function print(s)
{
	sys.print(s);
	sys.print("\n");
}

function quit()
{
	process.exit();
}

/* Node doesn't have an include() function. And eval() has to be called from
 * the top level or it doesn't work. WTF? */

function load(filename)
{
	return fs.readFileSync(filename, "utf-8");
}

/* Load the Clue crt and libc. */

eval(load("src/javascript/crt.js"));
eval(load("src/javascript/libc.js"));

/* Load the Clue compiled program. */

var argv = process.argv;
var argc = 3;
while (argv[argc] != "--")
{
	if (!argv[argc])
	{
		print("Command line did not contain a --\n");
		quit();
	}

	eval(load(argv[argc]));
	argc++;
}

/* And run it. */

{
	var cargs = [];
	
	for (var i = argc+1; i < argv.length; i++)
	{
		var v = clue_newstring(argv[i]);
		cargs.push(0);
		cargs.push(v);
	}

    clue_run_initializers();
    _main(0, [], argv.length - argc, 0, cargs);
    quit();
}
