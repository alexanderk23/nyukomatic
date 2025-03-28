#include "TextEditor.h"

static bool TokenizeLuaStyleString(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	bool is_single_quote = false;
	bool is_double_quotes = false;
	bool is_double_square_brackets = false;

	switch (*p)
	{
	case '\'':
		is_single_quote = true;
		break;
	case '"':
		is_double_quotes = true;
		break;
	case '[':
		p++;
		if (p < in_end && *(p) == '[')
			is_double_square_brackets = true;
		break;
	}

	if (is_single_quote || is_double_quotes || is_double_square_brackets)
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if ((is_single_quote && *p == '\'') || (is_double_quotes && *p == '"') || (is_double_square_brackets && *p == ']' && p + 1 < in_end && *(p + 1) == ']'))
			{
				out_begin = in_begin;

				if (is_double_square_brackets)
					out_end = p + 2;
				else
					out_end = p + 1;

				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && (is_single_quote || is_double_quotes))
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeLuaStyleIdentifier(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;
		return true;
	}

	return false;
}

static bool TokenizeLuaStyleNumber(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}

		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}
	}

	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool TokenizeLuaStylePunctuation(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '#':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "break", "do", "else", "elseif", "end", "false",
			"for", "function", "goto", "if", "in", "local", "nil", "not", "or",
			"repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset",
			"select", "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace",
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable",
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen",
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			"pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			"date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			"reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			"coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};
		for (auto& k : identifiers)
			langDef.mIdentifiers.insert(k);

		langDef.mTokenize = [](const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex& paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeLuaStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeLuaStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeLuaStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeLuaStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "--[[";
		langDef.mCommentEnd = "]]";
		langDef.mSingleLineComment = "--";

		langDef.mCaseSensitive = true;

		langDef.mName = "Lua";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Json()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		langDef.mKeywords.clear();
		langDef.mIdentifiers.clear();

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\"(\\.|[^\"])*\")##", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.\:])##", PaletteIndex::Punctuation));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(false|true)##", PaletteIndex::Keyword));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;

		langDef.mName = "Json";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Z80Asm()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		langDef.mKeywords.clear();
		langDef.mIdentifiers.clear();

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##((?:;|//).*)##", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(^([@\.]?[A-Za-z_][a-zA-Z0-9_\.]*:?))##", PaletteIndex::Identifier)); // label

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\"(\\.|[^\"])*\")##", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##('(\\.|[^'])*')##", PaletteIndex::String));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([\$#][0-9A-Fa-f]+\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([0-9A-Fa-f]+[Hh]\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(0[Xx][0-9A-Fa-f]+\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(0[Bb][01]+\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(%[01]+\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([0-7]+[Oo]\b)##", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(0[Oo][0-7]+\b)##", PaletteIndex::Number));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(?:ad[cd]|and|bit|ccf|cp|cp[di]r?|cpl|daa|dec|[de]i|djnz|ex[adx]?|halt|i[mn]|inc|in[di]r?|ld|ld[di]r?|neg|nop|or|ot[di]r|out|out[di]|pop|push|res|ret[in]|rla?|rlca?|r[lr]d|rra?|rrca?|rst|sbc|scf|set|s[lr]a|s[lr]l|slia|sl1|sub|x?or)\b)##", PaletteIndex::Keyword));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(?:include|incbin|incl|incb|binary|insert|inchob|inctrd|org|align|page|slot|equ|disp|phase|unphase|dephase|ent|textarea|macro|endm|struct|ends|dup|edup|rept|endr|define|if|ifn|endif|ifdef|ifndef|end)\b)##", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(?:db|dw|ds|dm|dd|dz|defb|defw|defs|defm|defd|byte|abyte(c|z)?|word|d24|dword|block|defarray)\b)##", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(?:device|display|savesna|savebin|savehob|savetrd|module|endmodule|lua|endlua|includelua|assert|emptytrd|encoding|export|labelslist|output|shellexec|size)\b)##", PaletteIndex::Preprocessor));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(?:call|jp)(?:\s+(?:n?[cz]|p[eo]|[mp]))?\b)##", PaletteIndex::Keyword));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\bret(?:\s+(?:n?[cz]|p[eo]|[mp]))?\b)##", PaletteIndex::Keyword));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\bjr(?:\s+(?:n?[cz]|[mps]))?\b)##", PaletteIndex::Keyword));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##((?:\baf')|(?:\b(?:af|bc|de|hl|sp|ix|iy)\b))##", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b(a|b|c|d|e|h|l|i|r|xl|lx|xh|hx|ixl|ixh|yl|ly|yh|hy|iyl|iyh)\b)##", PaletteIndex::Preprocessor));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([@\.]?[A-Za-z_][A-Za-z0-9_\.]*\b)##", PaletteIndex::Identifier));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?\b)##", PaletteIndex::Number));
		// langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\b[0-9]+\b)##", PaletteIndex::Number));

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##(\$+)##", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.\:]+)##", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = ";";
		langDef.mPreprocChar = '\x00';
		langDef.mCaseSensitive = false;
		langDef.mName = "Z80Asm";

		inited = true;
	}
	return langDef;
}
