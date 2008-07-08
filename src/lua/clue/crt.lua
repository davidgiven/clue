-- clue/crt.lua
--
-- Clue C runtime library
--
-- © 2008 David Given.
-- Clue is licensed under the Revised BSD open source license. To get the
-- full license text, see the README file.
--
-- $Id: build 136 2008-03-22 19:00:08Z dtrg $

require "bit"

local print = print
local unpack = unpack
local string_char = string.char
local string_byte = string.byte
local string_sub = string.sub
local string_find = string.find
local string_len = string.len
local math_floor = math.floor
local bit_bnot = bit.bnot
local bit_band = bit.band
local bit_bor = bit.bor
local bit_bxor = bit.bxor
local bit_lshift = bit.lshift
local bit_rshift = bit.rshift
local bit_arshift = bit.arshift

local ZERO = string_char(0)
 
module "clue.crt"

local READ_FN = 1
local WRITE_FN = 2
local OFFSET_FN = 3
local TOSTRING_FN = 4

local DATA_I = 1
local OFFSET_I = 2

function ptrread(ptr, offset)
	local data = ptr[DATA_I]
	local baseo = ptr[OFFSET_I]
	return data[offset + baseo]
end

function ptrwrite(ptr, offset, value)
	local data = ptr[DATA_I]
	local baseo = ptr[OFFSET_I]
	data[offset + baseo] = value
end

function ptroffset(ptr, offset)
	local data = ptr[DATA_I]
	local baseo = ptr[OFFSET_I]
	return {
		[DATA_I] = data,
		[OFFSET_I] = baseo + offset
	}
end

function ptrtostring(ptr)
	local data = ptr[DATA_I]
	local offset = ptr[OFFSET_I]
	
	local s = {}
	while true do
		local c = data[offset]
		if (c == 0) then
			break;
		else
			s[#s+1] = c
		end
		offset = offset + 1
	end
	
	return string_char(unpack(s))
end
	
-- Read a value from the stack.

function stackread(stack, o, offset)
	return stack[o + offset]
end

-- Write a value to the stack.

function stackwrite(stack, o, offset, value)
	stack[o + offset] = value
end

-- Construct a new pointer pointing at the stack.
 
function stackoffset(stack, o, offset)
	return {
		[DATA_I] = stack,
		[OFFSET_I] = o + offset
	}
end

-- Construct a pointer to new storage.

function newptr(data)
	return {
		[DATA_I] = data or {},
		[OFFSET_I] = 1
	}
end

-- Construct a pointer to a string.

function newstring(s)
	local len = string_len(s)
	s = {string_byte(s, 1, len)}
	return {
		[DATA_I] = s,
		[OFFSET_I] = 1
	}
end

-- Ditto, but adds a \0 on the end

function newstring0(s)
	return newstring(s..ZERO)
end
 
-- Number operations.

function int(v)
	return math_floor(v)
end

-- Bit operations.

function shl(v, shift)
	return bit_lshift(v, shift)
end

function shr(v, shift)
	return bit_rshift(v, shift)
end

function logor(v1, v2)
	return bit_bor(v1, v2)
end

function logand(v1, v2)
	return bit_band(v1, v2)
end

function logxor(v1, v2)
	return bit_bxor(v1, v2)
end

-- Boolean operations.

function booland(v1, v2)
	v1 = (v1 ~= nil) and (v1 ~= 0)
	v2 = (v2 ~= nil) and (v2 ~= 0) 
	return (v1 and v2) and 1 or 0
end

function boolor(v1, v2)
	v1 = (v1 ~= nil) and (v1 ~= 0)
	v2 = (v2 ~= nil) and (v2 ~= 0) 
	return (v1 or v2) and 1 or 0
end

