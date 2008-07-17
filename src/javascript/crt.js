/* crt.js
 * Javascript C runtime.
 *
 * © 2008 David Given.
 * Clue is licensed under the Revised BSD open source license. To get the
 * full license text, see the README file.
 *
 * $Id: build 136 2008-03-22 19:00:08Z dtrg $
 * $HeadURL: https://primemover.svn.sf.net/svnroot/primemover/pm/lib/c.pm $
 * $LastChangedDate: 2007-04-30 22:41:42 +0000 (Mon, 30 Apr 2007) $
 */

var clue_initializer_list = [];

function clue_add_initializer(i)
{
	clue_initializer_list.push(i);
}

function clue_run_initializers()
{
	while (clue_initializer_list.length > 0)
		clue_initializer_list.shift()();
}

function clue_ptrtostring(po, pd)
{
	var s = [];
	for (;;)
	{
		var c = pd[po];
		if (c == 0)
			break;
		else
			s.push(c);
		po++;
	}
	
	return String.fromCharCode.apply(null, s);
}

function clue_newstring(s)
{
	var d = [];
	
	for (var i = 0; i < s.length; i++)
		d[i] = s.charCodeAt(i);
	d[s.length] = 0;
	
	return d;
}
