/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3.0 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "string.h"
#include "pointer.h"
#include "numeric.h"
#include "log.h"

#include "hex.h"
#include "porting.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <map>

#ifndef _WIN32
	#include <iconv.h>
#else
	#define _WIN32_WINNT 0x0501
	#include <windows.h>
#endif

#if defined(_ICONV_H_) && (defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__))
	#define BSD_ICONV_USED
#endif

static bool parseHexColorString(const std::string &value, video::SColor &color);
static bool parseNamedColorString(const std::string &value, video::SColor &color);

#ifndef _WIN32

bool convert(const char *to, const char *from, char *outbuf,
             size_t outbuf_size, char *inbuf, size_t inbuf_size) {
	iconv_t cd = iconv_open(to, from);

#ifdef BSD_ICONV_USED
	const char *inbuf_ptr = inbuf;
#else
	char *inbuf_ptr = inbuf;
#endif

	char *outbuf_ptr = outbuf;

	size_t *inbuf_left_ptr = &inbuf_size;
	size_t *outbuf_left_ptr = &outbuf_size;

	size_t old_size = inbuf_size;
	while (inbuf_size > 0) {
		iconv(cd, &inbuf_ptr, inbuf_left_ptr, &outbuf_ptr, outbuf_left_ptr);
		if (inbuf_size == old_size) {
			iconv_close(cd);
			return false;
		}
		old_size = inbuf_size;
	}

	iconv_close(cd);
	return true;
}

#if defined(__ANDROID__) || defined(__IOS__)
// Android and iOS needs manual caring to support the full character set possible with wchar_t
const char *to = "UTF-32LE";
#else
const char *to = "WCHAR_T";
#endif

std::wstring utf8_to_wide(const std::string &input) {
	size_t inbuf_size = input.length() + 1;
	// maximum possible size, every character is sizeof(wchar_t) bytes
	size_t outbuf_size = (input.length() + 1) * sizeof(wchar_t);

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

#if defined(__ANDROID__) || defined(__IOS__)
	// Android and iOS needs manual caring to support the full character set possible with wchar_t
	SANITY_CHECK(sizeof(wchar_t) == 4);
#endif

	if (!convert(to, "UTF-8", outbuf, outbuf_size, inbuf, inbuf_size)) {
		infostream << "Couldn't convert UTF-8 string 0x" << hex_encode(input)
		           << " into wstring" << std::endl;
		delete[] inbuf;
		delete[] outbuf;
		return L"<invalid UTF-8 string>";
	}
	std::wstring out((wchar_t *)outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}

std::string wide_to_utf8(const std::wstring &input) {
	size_t inbuf_size = (input.length() + 1) * sizeof(wchar_t);
	// maximum possible size: utf-8 encodes codepoints using 1 up to 6 bytes
	size_t outbuf_size = (input.length() + 1) * 6;

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

	if (!convert("UTF-8", to, outbuf, outbuf_size, inbuf, inbuf_size)) {
		infostream << "Couldn't convert wstring 0x" << hex_encode(inbuf, inbuf_size)
			<< " into UTF-8 string" << std::endl;
		delete[] inbuf;
		delete[] outbuf;
		return "<invalid wstring>";
	}
	std::string out(outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}

#else // _WIN32

std::wstring utf8_to_wide(const std::string &input) {
	size_t outbuf_size = input.size() + 1;
	wchar_t *outbuf = new wchar_t[outbuf_size];
	memset(outbuf, 0, outbuf_size * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.size(),
		outbuf, outbuf_size);
	std::wstring out(outbuf);
	delete[] outbuf;
	return out;
}

std::string wide_to_utf8(const std::wstring &input) {
	size_t outbuf_size = (input.size() + 1) * 6;
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);
	WideCharToMultiByte(CP_UTF8, 0, input.c_str(), input.size(),
		outbuf, outbuf_size, NULL, NULL);
	std::string out(outbuf);
	delete[] outbuf;
	return out;
}

#endif // _WIN32

// You must free the returned string!
// The returned string is allocated using new
wchar_t *utf8_to_wide_c(const char *str) {
	std::wstring ret = utf8_to_wide(std::string(str));
	size_t len = ret.length();
	wchar_t *ret_c = new wchar_t[len + 1];
	memset(ret_c, 0, (len + 1) * sizeof(wchar_t));
	memcpy(ret_c, ret.c_str(), len * sizeof(wchar_t));
	return ret_c;
}

// You must free the returned string!
// The returned string is allocated using new
wchar_t *narrow_to_wide_c(const char *str) {
	wchar_t *nstr = nullptr;
#if defined(_WIN32)
	int nResult = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) str, -1, 0, 0);
	if (nResult == 0) {
		errorstream<<"gettext: MultiByteToWideChar returned null"<<std::endl;
	} else {
		nstr = new wchar_t[nResult];
		MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) str, -1, (WCHAR *) nstr, nResult);
	}
#else
	size_t len = strlen(str);
	nstr = new wchar_t[len + 1];

	std::wstring intermediate = narrow_to_wide(str);
	memset(nstr, 0, (len + 1) * sizeof(wchar_t));
	memcpy(nstr, intermediate.c_str(), len * sizeof(wchar_t));
#endif

	return nstr;
}

std::wstring narrow_to_wide(const std::string &mbs) {
	size_t wcl = mbs.size();
#ifdef __ANDROID__
	std::wstring retval = L"";
	for (unsigned int i = 0; i < wcl; i++) {
		if (((unsigned char) mbs[i] > 31) &&
		    ((unsigned char) mbs[i] < 127))
			retval += (unsigned char) mbs[i];
		else if (mbs[i] == '\n')
			// handle newline
			retval += L'\n';
	}
	return retval;
#else // not Android
	Buffer<wchar_t> wcs(wcl + 1);
	size_t len = mbstowcs(*wcs, mbs.c_str(), wcl);
	if (len == (size_t)(-1))
		return L"<invalid multibyte string>";
	wcs[len] = 0;
	return *wcs;
#endif
}


std::string wide_to_narrow(const std::wstring &wcs) {
	size_t mbl = wcs.size() * 4;
	SharedBuffer<char> mbs(mbl+1);
	size_t len = wcstombs(*mbs, wcs.c_str(), mbl);
	if (len == (size_t)(-1))
		return "Character conversion failed!";

	mbs[len] = 0;
	return *mbs;
}


std::string urlencode(const std::string &str) {
	// Encodes non-unreserved URI characters by a percent sign
	// followed by two hex digits. See RFC 3986, section 2.3.
	static const char url_hex_chars[] = "0123456789ABCDEF";
	std::ostringstream oss(std::ios::binary);
	for (unsigned char c : str) {
		if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
			oss << c;
		} else {
			oss << "%"
			    << url_hex_chars[(c & 0xf0) >> 4]
			    << url_hex_chars[c & 0x0f];
		}
	}
	return oss.str();
}

std::string urldecode(const std::string &str) {
	// Inverse of urlencode
	std::ostringstream oss(std::ios::binary);
	for (u32 i = 0; i < str.size(); i++) {
		unsigned char highvalue, lowvalue;
		if (str[i] == '%' &&
		    hex_digit_decode(str[i+1], highvalue) &&
		    hex_digit_decode(str[i+2], lowvalue)) {
			oss << (char) ((highvalue << 4) | lowvalue);
			i += 2;
		} else {
			oss << str[i];
		}
	}
	return oss.str();
}

u32 readFlagString(std::string str, const FlagDesc *flagdesc, u32 *flagmask) {
	u32 result = 0;
	u32 mask = 0;
	char *s = &str[0];
	char *flagstr;
	char *strpos = nullptr;

	while ((flagstr = strtok_r(s, ",", &strpos))) {
		s = nullptr;

		while (*flagstr == ' ' || *flagstr == '\t')
			flagstr++;

		bool flagset = true;
		if (!strncasecmp(flagstr, "no", 2)) {
			flagset = false;
			flagstr += 2;
		}

		for (int i = 0; flagdesc[i].name; i++) {
			if (!strcasecmp(flagstr, flagdesc[i].name)) {
				mask |= flagdesc[i].flag;
				if (flagset)
					result |= flagdesc[i].flag;
				break;
			}
		}
	}

	if (flagmask)
		*flagmask = mask;

	return result;
}

std::string writeFlagString(u32 flags, const FlagDesc *flagdesc, u32 flagmask) {
	std::string result;

	for (int i = 0; flagdesc[i].name; i++) {
		if (flagmask & flagdesc[i].flag) {
			if (!(flags & flagdesc[i].flag))
				result += "no";

			result += flagdesc[i].name;
			result += ", ";
		}
	}

	size_t len = result.length();
	if (len >= 2)
		result.erase(len - 2, 2);

	return result;
}

size_t mystrlcpy(char *dst, const char *src, size_t size) {
	size_t srclen  = strlen(src) + 1;
	size_t copylen = MYMIN(srclen, size);

	if (copylen > 0) {
		memcpy(dst, src, copylen);
		dst[copylen - 1] = '\0';
	}

	return srclen;
}

char *mystrtok_r(char *s, const char *sep, char **lasts) {
	char *t;

	if (!s)
		s = *lasts;

	while (*s && strchr(sep, *s))
		s++;

	if (!*s)
		return nullptr;

	t = s;
	while (*t) {
		if (strchr(sep, *t)) {
			*t++ = '\0';
			break;
		}
		t++;
	}

	*lasts = t;
	return s;
}

u64 read_seed(const char *str) {
	char *endptr;
	u64 num;

	if (str[0] == '0' && str[1] == 'x')
		num = strtoull(str, &endptr, 16);
	else
		num = strtoull(str, &endptr, 10);

	if (*endptr)
		num = murmur_hash_64_ua(str, (int)strlen(str), 0x1337);

	return num;
}

bool parseColorString(const std::string &value, video::SColor &color, bool quiet) {
	bool success;

	if (value[0] == '#')
		success = parseHexColorString(value, color);
	else
		success = parseNamedColorString(value, color);

	if (!success && !quiet)
		errorstream << "Invalid color: \"" << value << "\"" << std::endl;

	return success;
}

static bool parseHexColorString(const std::string &value, video::SColor &color) {
	unsigned char components[] = { 0x00, 0x00, 0x00, 0xff }; // R,G,B,A

	if (value[0] != '#')
		return false;

	size_t len = value.size();
	bool short_form;

	if (len == 9 || len == 7) // #RRGGBBAA or #RRGGBB
		short_form = false;
	else if (len == 5 || len == 4) // #RGBA or #RGB
		short_form = true;
	else
		return false;

	bool success = true;

	for (size_t pos = 1, cc = 0; pos < len; pos++, cc++) {
		assert(cc < sizeof components / sizeof components[0]);
		if (short_form) {
			unsigned char d;
			if (!hex_digit_decode(value[pos], d)) {
				success = false;
				break;
			}
			components[cc] = (d & 0xf) << 4 | (d & 0xf);
		} else {
			unsigned char d1, d2;
			if (!hex_digit_decode(value[pos], d1) ||
			    !hex_digit_decode(value[pos+1], d2)) {
				success = false;
				break;
			}
			components[cc] = (d1 & 0xf) << 4 | (d2 & 0xf);
			pos++;	// skip the second digit -- it's already used
		}
	}

	if (success) {
		color.setRed(components[0]);
		color.setGreen(components[1]);
		color.setBlue(components[2]);
		color.setAlpha(components[3]);
	}

	return success;
}

struct ColorContainer {
	ColorContainer();
	std::map<const std::string, u32> colors;
};

ColorContainer::ColorContainer() {
	colors["aliceblue"]              = 0xf0f8ff;
	colors["antiquewhite"]           = 0xfaebd7;
	colors["aqua"]                   = 0x00ffff;
	colors["aquamarine"]             = 0x7fffd4;
	colors["azure"]                  = 0xf0ffff;
	colors["beige"]                  = 0xf5f5dc;
	colors["bisque"]                 = 0xffe4c4;
	colors["black"]                  = 00000000;
	colors["blanchedalmond"]         = 0xffebcd;
	colors["blue"]                   = 0x0000ff;
	colors["blueviolet"]             = 0x8a2be2;
	colors["brown"]                  = 0xa52a2a;
	colors["burlywood"]              = 0xdeb887;
	colors["cadetblue"]              = 0x5f9ea0;
	colors["chartreuse"]             = 0x7fff00;
	colors["chocolate"]              = 0xd2691e;
	colors["coral"]                  = 0xff7f50;
	colors["cornflowerblue"]         = 0x6495ed;
	colors["cornsilk"]               = 0xfff8dc;
	colors["crimson"]                = 0xdc143c;
	colors["cyan"]                   = 0x00ffff;
	colors["darkblue"]               = 0x00008b;
	colors["darkcyan"]               = 0x008b8b;
	colors["darkgoldenrod"]          = 0xb8860b;
	colors["darkgray"]               = 0xa9a9a9;
	colors["darkgreen"]              = 0x006400;
	colors["darkgrey"]               = 0xa9a9a9;
	colors["darkkhaki"]              = 0xbdb76b;
	colors["darkmagenta"]            = 0x8b008b;
	colors["darkolivegreen"]         = 0x556b2f;
	colors["darkorange"]             = 0xff8c00;
	colors["darkorchid"]             = 0x9932cc;
	colors["darkred"]                = 0x8b0000;
	colors["darksalmon"]             = 0xe9967a;
	colors["darkseagreen"]           = 0x8fbc8f;
	colors["darkslateblue"]          = 0x483d8b;
	colors["darkslategray"]          = 0x2f4f4f;
	colors["darkslategrey"]          = 0x2f4f4f;
	colors["darkturquoise"]          = 0x00ced1;
	colors["darkviolet"]             = 0x9400d3;
	colors["deeppink"]               = 0xff1493;
	colors["deepskyblue"]            = 0x00bfff;
	colors["dimgray"]                = 0x696969;
	colors["dimgrey"]                = 0x696969;
	colors["dodgerblue"]             = 0x1e90ff;
	colors["firebrick"]              = 0xb22222;
	colors["floralwhite"]            = 0xfffaf0;
	colors["forestgreen"]            = 0x228b22;
	colors["fuchsia"]                = 0xff00ff;
	colors["gainsboro"]              = 0xdcdcdc;
	colors["ghostwhite"]             = 0xf8f8ff;
	colors["gold"]                   = 0xffd700;
	colors["goldenrod"]              = 0xdaa520;
	colors["gray"]                   = 0x808080;
	colors["green"]                  = 0x008000;
	colors["greenyellow"]            = 0xadff2f;
	colors["grey"]                   = 0x808080;
	colors["honeydew"]               = 0xf0fff0;
	colors["hotpink"]                = 0xff69b4;
	colors["indianred"]              = 0xcd5c5c;
	colors["indigo"]                 = 0x4b0082;
	colors["ivory"]                  = 0xfffff0;
	colors["khaki"]                  = 0xf0e68c;
	colors["lavender"]               = 0xe6e6fa;
	colors["lavenderblush"]          = 0xfff0f5;
	colors["lawngreen"]              = 0x7cfc00;
	colors["lemonchiffon"]           = 0xfffacd;
	colors["lightblue"]              = 0xadd8e6;
	colors["lightcoral"]             = 0xf08080;
	colors["lightcyan"]              = 0xe0ffff;
	colors["lightgoldenrodyellow"]   = 0xfafad2;
	colors["lightgray"]              = 0xd3d3d3;
	colors["lightgreen"]             = 0x90ee90;
	colors["lightgrey"]              = 0xd3d3d3;
	colors["lightpink"]              = 0xffb6c1;
	colors["lightsalmon"]            = 0xffa07a;
	colors["lightseagreen"]          = 0x20b2aa;
	colors["lightskyblue"]           = 0x87cefa;
	colors["lightslategray"]         = 0x778899;
	colors["lightslategrey"]         = 0x778899;
	colors["lightsteelblue"]         = 0xb0c4de;
	colors["lightyellow"]            = 0xffffe0;
	colors["lime"]                   = 0x00ff00;
	colors["limegreen"]              = 0x32cd32;
	colors["linen"]                  = 0xfaf0e6;
	colors["magenta"]                = 0xff00ff;
	colors["maroon"]                 = 0x800000;
	colors["mediumaquamarine"]       = 0x66cdaa;
	colors["mediumblue"]             = 0x0000cd;
	colors["mediumorchid"]           = 0xba55d3;
	colors["mediumpurple"]           = 0x9370db;
	colors["mediumseagreen"]         = 0x3cb371;
	colors["mediumslateblue"]        = 0x7b68ee;
	colors["mediumspringgreen"]      = 0x00fa9a;
	colors["mediumturquoise"]        = 0x48d1cc;
	colors["mediumvioletred"]        = 0xc71585;
	colors["midnightblue"]           = 0x191970;
	colors["mintcream"]              = 0xf5fffa;
	colors["mistyrose"]              = 0xffe4e1;
	colors["moccasin"]               = 0xffe4b5;
	colors["navajowhite"]            = 0xffdead;
	colors["navy"]                   = 0x000080;
	colors["oldlace"]                = 0xfdf5e6;
	colors["olive"]                  = 0x808000;
	colors["olivedrab"]              = 0x6b8e23;
	colors["orange"]                 = 0xffa500;
	colors["orangered"]              = 0xff4500;
	colors["orchid"]                 = 0xda70d6;
	colors["palegoldenrod"]          = 0xeee8aa;
	colors["palegreen"]              = 0x98fb98;
	colors["paleturquoise"]          = 0xafeeee;
	colors["palevioletred"]          = 0xdb7093;
	colors["papayawhip"]             = 0xffefd5;
	colors["peachpuff"]              = 0xffdab9;
	colors["peru"]                   = 0xcd853f;
	colors["pink"]                   = 0xffc0cb;
	colors["plum"]                   = 0xdda0dd;
	colors["powderblue"]             = 0xb0e0e6;
	colors["purple"]                 = 0x800080;
	colors["red"]                    = 0xff0000;
	colors["rosybrown"]              = 0xbc8f8f;
	colors["royalblue"]              = 0x4169e1;
	colors["saddlebrown"]            = 0x8b4513;
	colors["salmon"]                 = 0xfa8072;
	colors["sandybrown"]             = 0xf4a460;
	colors["seagreen"]               = 0x2e8b57;
	colors["seashell"]               = 0xfff5ee;
	colors["sienna"]                 = 0xa0522d;
	colors["silver"]                 = 0xc0c0c0;
	colors["skyblue"]                = 0x87ceeb;
	colors["slateblue"]              = 0x6a5acd;
	colors["slategray"]              = 0x708090;
	colors["slategrey"]              = 0x708090;
	colors["snow"]                   = 0xfffafa;
	colors["springgreen"]            = 0x00ff7f;
	colors["steelblue"]              = 0x4682b4;
	colors["tan"]                    = 0xd2b48c;
	colors["teal"]                   = 0x008080;
	colors["thistle"]                = 0xd8bfd8;
	colors["tomato"]                 = 0xff6347;
	colors["turquoise"]              = 0x40e0d0;
	colors["violet"]                 = 0xee82ee;
	colors["wheat"]                  = 0xf5deb3;
	colors["white"]                  = 0xffffff;
	colors["whitesmoke"]             = 0xf5f5f5;
	colors["yellow"]                 = 0xffff00;
	colors["yellowgreen"]            = 0x9acd32;

}

static const ColorContainer named_colors;

static bool parseNamedColorString(const std::string &value, video::SColor &color) {
	std::string color_name;
	std::string alpha_string;

	/* If the string has a # in it, assume this is the start of a specified
	 * alpha value (if it isn't the string is invalid and the error will be
	 * caught later on, either because the color name won't be found or the
	 * alpha value will fail conversion)
	 */
	size_t alpha_pos = value.find('#');
	if (alpha_pos != std::string::npos) {
		color_name = value.substr(0, alpha_pos);
		alpha_string = value.substr(alpha_pos + 1);
	} else {
		color_name = value;
	}

	color_name = lowercase(value);

	std::map<const std::string, unsigned>::const_iterator it;
	it = named_colors.colors.find(color_name);
	if (it == named_colors.colors.end())
		return false;

	u32 color_temp = it->second;

	/* An empty string for alpha is ok (none of the color table entries
	 * have an alpha value either). Color strings without an alpha specified
	 * are interpreted as fully opaque
	 *
	 * For named colors the supplied alpha string (representing a hex value)
	 * must be exactly two digits. For example:  colorname#08
	 */
	if (!alpha_string.empty()) {
		if (alpha_string.length() != 2)
			return false;

		unsigned char d1, d2;
		if (!hex_digit_decode(alpha_string.at(0), d1)
		    || !hex_digit_decode(alpha_string.at(1), d2))
			return false;
		color_temp |= ((d1 & 0xf) << 4 | (d2 & 0xf)) << 24;
	} else {
		color_temp |= 0xff << 24;  // Fully opaque
	}

	color = video::SColor(color_temp);

	return true;
}

void str_replace(std::string &str, char from, char to) {
	std::replace(str.begin(), str.end(), from, to);
}
