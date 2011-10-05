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

/****************************************************************************
 *                                PRINTF                                    *
 ****************************************************************************/

/* This is a copy of Ash Searle's public domain sprintf function. See:
 *   http://hexmen.com/blog/2007/03/printf-sprintf/
 * ...for more info.
 */

/**
 * JavaScript printf/sprintf functions.
 *
 * This code is unrestricted: you are free to use it however you like.
 * 
 * The functions should work as expected, performing left or right alignment,
 * truncating strings, outputting numbers with a required precision etc.
 *
 * For complex cases, these functions follow the Perl implementations of
 * (s)printf, allowing arguments to be passed out-of-order, and to set the
 * precision or length of the output based on arguments instead of fixed
 * numbers.
 *
 * See http://perldoc.perl.org/functions/sprintf.html for more information.
 *
 * Implemented:
 * - zero and space-padding
 * - right and left-alignment,
 * - base X prefix (binary, octal and hex)
 * - positive number prefix
 * - (minimum) width
 * - precision / truncation / maximum width
 * - out of order arguments
 *
 * Not implemented (yet):
 * - vector flag
 * - size (bytes, words, long-words etc.)
 * 
 * Will not implement:
 * - %n or %p (no pass-by-reference in JavaScript)
 *
 * @version 2007.04.27
 * @author Ash Searle
 */

function sprintf()
{
	function pad(str, len, chr, leftJustify)
	{
		var padding = (str.length >= len) ? '' : Array(
		        1 + len - str.length >>> 0).join(chr);
		return leftJustify ? str + padding : padding + str;

	}

	function justify(value, prefix, leftJustify, minWidth, zeroPad)
	{
		var diff = minWidth - value.length;
		if (diff > 0)
		{
			if (leftJustify || !zeroPad)
			{
				value = pad(value, minWidth, ' ', leftJustify);
			}
			else
			{
				value = value.slice(0, prefix.length)
				        + pad('', diff, '0', true) + value.slice(prefix.length);
			}
		}
		return value;
	}

	function formatBaseX(value, base, prefix, leftJustify, minWidth, precision,
	        zeroPad)
	{
		// Note: casts negative numbers to positive ones
		var number = value >>> 0;
		prefix = prefix && number &&
		{
		    '2' :'0b',
		    '8' :'0',
		    '16' :'0x'
		}[base] || '';
		value = prefix + pad(number.toString(base), precision || 0, '0', false);
		return justify(value, prefix, leftJustify, minWidth, zeroPad);
	}

	function formatString(value, leftJustify, minWidth, precision, zeroPad)
	{
		if (precision != null)
		{
			value = value.slice(0, precision);
		}
		return justify(value, '', leftJustify, minWidth, zeroPad);
	}

	var a = arguments, i = 0, format = a[i++];
	
	/* Foul hack to remove simple uses of %l, which aren't supported. */
	format.replace("[^%]%l", "%");
	format.replace("^%l", "%");
	
	return format
	        .replace(
	                sprintf.regex,
	                function(substring, valueIndex, flags, minWidth, _,
	                        precision, type)
	                {
		                if (substring == '%%')
			                return '%';

		                // parse flags
		                var leftJustify = false, positivePrefix = '', zeroPad = false, prefixBaseX = false;
		                for ( var j = 0; flags && j < flags.length; j++)
			                switch (flags.charAt(j))
			                {
				                case ' ':
					                positivePrefix = ' ';
					                break;
				                case '+':
					                positivePrefix = '+';
					                break;
				                case '-':
					                leftJustify = true;
					                break;
				                case '0':
					                zeroPad = true;
					                break;
				                case '#':
					                prefixBaseX = true;
					                break;
			                }

		                // parameters may be null, undefined, empty-string or
						// real valued
		                // we want to ignore null, undefined and empty-string
						// values

		                if (!minWidth)
		                {
			                minWidth = 0;
		                }
		                else if (minWidth == '*')
		                {
			                minWidth = +a[i++];
		                }
		                else if (minWidth.charAt(0) == '*')
		                {
			                minWidth = +a[minWidth.slice(1, -1)];
		                }
		                else
		                {
			                minWidth = +minWidth;
		                }

		                // Note: undocumented perl feature:
		                if (minWidth < 0)
		                {
			                minWidth = -minWidth;
			                leftJustify = true;
		                }

		                if (!isFinite(minWidth))
		                {
			                throw new Error(
			                        'sprintf: (minimum-)width must be finite');
		                }

		                if (!precision)
		                {
			                precision = 'fFeE'.indexOf(type) > -1 ? 6
			                        : (type == 'd') ? 0 : void (0);
		                }
		                else if (precision == '*')
		                {
			                precision = +a[i++];
		                }
		                else if (precision.charAt(0) == '*')
		                {
			                precision = +a[precision.slice(1, -1)];
		                }
		                else
		                {
			                precision = +precision;
		                }

		                // grab value using valueIndex if required?
		                var value = valueIndex ? a[valueIndex.slice(0, -1)]
		                        : a[i++];

		                switch (type)
		                {
			                case 's':
				                return formatString(String(value), leftJustify,
				                        minWidth, precision, zeroPad);
			                case 'c':
				                return formatString(
				                        String.fromCharCode(+value),
				                        leftJustify, minWidth, precision,
				                        zeroPad);
			                case 'b':
				                return formatBaseX(value, 2, prefixBaseX,
				                        leftJustify, minWidth, precision,
				                        zeroPad);
			                case 'o':
				                return formatBaseX(value, 8, prefixBaseX,
				                        leftJustify, minWidth, precision,
				                        zeroPad);
			                case 'x':
				                return formatBaseX(value, 16, prefixBaseX,
				                        leftJustify, minWidth, precision,
				                        zeroPad);
			                case 'X':
				                return formatBaseX(value, 16, prefixBaseX,
				                        leftJustify, minWidth, precision,
				                        zeroPad).toUpperCase();
			                case 'u':
				                return formatBaseX(value, 10, prefixBaseX,
				                        leftJustify, minWidth, precision,
				                        zeroPad);
			                case 'i':
			                case 'd':
			                {
				                var number = parseInt(+value);
				                var prefix = number < 0 ? '-' : positivePrefix;
				                value = prefix
				                        + pad(String(Math.abs(number)),
				                                precision, '0', false);
				                return justify(value, prefix, leftJustify,
				                        minWidth, zeroPad);
			                }
			                case 'e':
			                case 'E':
			                case 'f':
			                case 'F':
			                case 'g':
			                case 'G':
			                {
				                var number = +value;
				                var prefix = number < 0 ? '-' : positivePrefix;
				                var method =
				                [
				                        'toExponential', 'toFixed',
				                        'toPrecision'
				                ]['efg'.indexOf(type.toLowerCase())];
				                var textTransform =
				                [
				                        'toString', 'toUpperCase'
				                ]['eEfFgG'.indexOf(type) % 2];
				                value = prefix
				                        + Math.abs(number)[method](precision);
				                return justify(value, prefix, leftJustify,
				                        minWidth, zeroPad)[textTransform]();
			                }
			                default:
				                return substring;
		                }
	                });
}
sprintf.regex = /%%|%(\d+\$)?([-+#0 ]*)(\*\d+\$|\*|\d+)?(\.(\*\d+\$|\*|\d+))?([scboxXuidfegEG])/g;


/****************************************************************************
 *                                 STDIO                                    *
 ****************************************************************************/

var clue_output_buffer = "";

var __stdout = 0;

function clue_add_to_output_buffer(s)
{
	clue_output_buffer += s;
}

function clue_flush_output_buffer()
{	
	var strings = clue_output_buffer.split("\n")
	for (var i = 0; i < (strings.length - 1); i++)
		print(strings[i]);
		
	clue_output_buffer = strings[strings.length - 1];
}

function _printf(stackpo, stackpd, formatpo, formatpd)
{
	var format = clue_ptrtostring(formatpo, formatpd);
	var outargs = [format];
	
	var numargs = arguments.length;
	var i = 4;
	while (i < numargs)
	{
		var thisarg = arguments[i];
		i++;
		if (i < (numargs-1))
		{
			var nextarg = arguments[i];
			if (!nextarg || (typeof(nextarg) == "object"))
			{
				// If the next argument is nil or a table, then we must
				// be looking at a register pair representing a pointer.
				thisarg = clue_ptrtostring(thisarg, nextarg);
				i++;
			}
		}
		
		outargs.push(thisarg);
	}

	clue_add_to_output_buffer(sprintf.apply(null, outargs));
	clue_flush_output_buffer();
	return 1
}

function _putc(sp, stack, c, fppo, fppd)
{
	clue_add_to_output_buffer(String.fromCharCode(c));
	if (c == '\n')
		clue_flush_output_buffer();
}

function _atoi(sp, stack, po, pd)
{
	var s = clue_ptrtostring(po, pd);
	return s|0;
}

var _atol = _atoi;


/****************************************************************************
 *                                 STRINGS                                  *
 ****************************************************************************/

function _strcpy(sp, stack, destpo, destpd, srcpo, srcpd)
{
	var origdestpo = destpo;
	
	for (;;)
	{
		var c = srcpd[srcpo];
		destpd[destpo] = c;
		
		srcpo++;
		destpo++;
		
		if (!c)
			break;
	}

	return [origdestpo, destpd];
}

function _memset(sp, stack, destpo, destpd, c, n)
{
	for (var offset = 0; offset < (n-1); offset++)
		destpd[destpo + offset] = c;
	
	return [destpo, destpd];
}

/****************************************************************************
 *                                  TIME                                    *
 ****************************************************************************/

function _gettimeofday(sp, stack, tvpo, tvpd, tzpo, tzpd)
{
	var t = new Date().getTime();
	var secs = (t / 1000) | 0;
	var usecs = (t % 1000) * 1000;
	
	tvpd[tvpo+0] = secs;
	tvpd[tvpo+1] = usecs;
	return 0;
}

/****************************************************************************
 *                                 MATHS                                    *
 ****************************************************************************/

function _sin(sp, stack, n)  { return Math.sin(n); }
function _cos(sp, stack, n)  { return Math.cos(n); }
function _atan(sp, stack, n) { return Math.atan(n); }
function _pow(sp, stack, x, y) { return Math.pow(x, y); }
function _log(sp, stack, n)  { return Math.log(n); }
function _exp(sp, stack, n)  { return Math.exp(n); }
function _sqrt(sp, stack, n) { return Math.sqrt(n); }

/****************************************************************************
 *                                 MEMORY                                   *
 ****************************************************************************/

function _malloc(sp, stack, size)
{
	return [0, []];
}

function _calloc(sp, stack, size1, size2)
{
	var d = [];
	for (var i = 0; i < (size1*size2); i++)
		d[i] = 0;
	return [0, d];
}

function _free(sp, stack, po, pd)
{
}

function _realloc(sp, stack, po, pd, size)
{
	return [po, pd];
}
