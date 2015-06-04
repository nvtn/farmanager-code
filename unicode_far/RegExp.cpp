/*
RegExp.cpp

Regular expressions
Syntax and semantics are very close to perl
*/
/*
Copyright � 2000 Konstantin Stupnik
Copyright � 2008 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "headers.hpp"
#pragma hdrstop

#include "RegExp.hpp"

#ifdef RE_DEBUG
#include "console.hpp"
#include "strmix.hpp"

#ifdef dpf
#undef dpf
#endif
#define dpf(x) Console().Write(str_printf x)

const wchar_t *ops[]=
{
	L"opNone",
	L"opLineStart",
	L"opLineEnd",
	L"opDataStart",
	L"opDataEnd",
	L"opWordBound",
	L"opNotWordBound",
	L"opType",
	L"opNotType",
	L"opCharAny",
	L"opCharAnyAll",
	L"opSymbol",
	L"opNotSymbol",
	L"opSymbolIgnoreCase",
	L"opNotSymbolIgnoreCase",
	L"opSymbolClass",
	L"opOpenBracket",
	L"opClosingBracket",
	L"opAlternative",
	L"opBackRef",
	L"opNamedBracket",
	L"opNamedBackRef",
	L"opRangesBegin",
	L"opRange",
	L"opMinRange",
	L"opSymbolRange",
	L"opSymbolMinRange",
	L"opNotSymbolRange",
	L"opNotSymbolMinRange",
	L"opAnyRange",
	L"opAnyMinRange",
	L"opTypeRange",
	L"opTypeMinRange",
	L"opNotTypeRange",
	L"opNotTypeMinRange",
	L"opClassRange",
	L"opClassMinRange",
	L"opBracketRange",
	L"opBracketMinRange",
	L"opBackRefRange",
	L"opBackRefMinRange",
	L"opNamedRefRange",
	L"opNamedRefMinRange",
	L"opRangesEnd",
	L"opAssertionsBegin",
	L"opLookAhead",
	L"opNotLookAhead",
	L"opLookBehind",
	L"opNotLookBehind",
	L"opAsserionsEnd",
	L"opNoReturn",
	L"opRegExpEnd",
};

#else
#define dpf(x)
#endif


#define ISDIGIT(c) iswdigit(c)
#define ISSPACE(c) iswspace(c)
#define ISWORD(c)  (IsCharAlphaNumeric(c) || c=='_')
#define ISLOWER(c) IsCharLower(c)
#define ISUPPER(c) IsCharUpper(c)
#define ISALPHA(c) IsCharAlpha(c)
#define TOUPPER(c) ((wchar_t)(intptr_t)CharUpper((LPTSTR)(intptr_t)c))
#define TOLOWER(c) ((wchar_t)(intptr_t)CharLower((LPTSTR)(intptr_t)c))

//! Max brackets depth
enum
{
	MAXDEPTH = 256,
};

// Locale Info bits
enum
{
	//! Digits
	TYPE_DIGITCHAR = 0x01,
	//! space, newlines tab etc
	TYPE_SPACECHAR = 0x02,
	//! alphanumeric and _
	TYPE_WORDCHAR = 0x04,
	//! lo-case symbol
	TYPE_LOWCASE = 0x08,
	//! up-case symbol
	TYPE_UPCASE = 0x10,
	//! letter
	TYPE_ALPHACHAR = 0x20,
};

// Bracket handler actions
enum
{
	//! Matched Closing bracket
	bhMatch = 1,
	//! Bracket rollback
	bhRollBack = 2,
};

enum
{
	RE_CHAR_COUNT = 1 << sizeof(wchar_t)* 8,
};

static bool isType(wchar_t chr,int type)
{
	switch (type)
	{
		case TYPE_DIGITCHAR:return ISDIGIT(chr) != 0;
		case TYPE_SPACECHAR:return ISSPACE(chr) != 0;
		case TYPE_WORDCHAR: return ISWORD(chr) != 0;
		case TYPE_LOWCASE:  return ISLOWER(chr) != 0;
		case TYPE_UPCASE:   return ISUPPER(chr) != 0;
		case TYPE_ALPHACHAR:return ISALPHA(chr) != 0;
	}

	return false;
}

struct RegExp::UniSet
{
	std::bitset<RE_CHAR_COUNT> Bits;
	char types;
	char nottypes;
	char negative;
	UniSet()
	{
		types=0;
		nottypes=0;
		negative=0;
	}
	UniSet(const UniSet& src)
	{
		Bits = src.Bits;
		types=src.types;
		nottypes=src.nottypes;
		negative=src.negative;
	}
	UniSet& operator=(const UniSet& src)
	{
		if (this != &src)
		{
			Bits = src.Bits;
			types=src.types;
			nottypes=src.nottypes;
			negative=src.negative;
		}

		return *this;
	}

	void Reset()
	{
		Bits.reset();
		types=0;
		nottypes=0;
		negative=0;
	}

	struct Setter
	{
		UniSet& set;
		wchar_t idx;
		Setter(UniSet& s,wchar_t chr):set(s),idx(chr)
		{
		}
		void operator=(int val)
		{
			if (val)set.SetBit(idx);
			else set.ClearBit(idx);
		}
		bool operator!()const
		{
			return !set.GetBit(idx);
		}
	};

	bool operator[](wchar_t idx)const
	{
		return GetBit(idx);
	}
	Setter operator[](wchar_t idx)
	{
		return Setter(*this,idx);
	}
	static bool CheckType(int t, wchar_t chr)
	{
		switch (t)
		{
			case TYPE_DIGITCHAR:if (ISDIGIT(chr))return true; else break;
			case TYPE_SPACECHAR:if (ISSPACE(chr))return true; else break;
			case TYPE_WORDCHAR: if (ISWORD(chr)) return true; else break;
			case TYPE_LOWCASE:  if (ISLOWER(chr))return true; else break;
			case TYPE_UPCASE:   if (ISUPPER(chr))return true; else break;
			case TYPE_ALPHACHAR:if (ISALPHA(chr))return true; else break;
		}

		return false;
	}
	bool GetBit(wchar_t chr) const
	{
		if (types)
		{
			int t=TYPE_ALPHACHAR;

			while (t)
			{
				if (types&t)
				{
					if (CheckType(t,chr))
						return !negative;
				}

				t>>=1;
			}
		}

		if (nottypes)
		{
			int t=TYPE_ALPHACHAR;

			while (t)
			{
				if (nottypes&t)
				{
					if (!CheckType(t,chr))
						return !negative;
				}

				t>>=1;
			}
		}

		const bool Set = Bits[chr];
		return negative? !Set : Set;
	}
	void SetBit(wchar_t  chr)
	{
		Bits.set(chr, true);
	}
	void ClearBit(wchar_t  chr)
	{
		Bits.set(chr, false);
	}

};

enum REOp
{
	opLineStart=0x1,        // ^
	opLineEnd,              // $
	opDataStart,            // \A and ^ in single line mode
	opDataEnd,              // \Z and $ in signle line mode

	opWordBound,            // \b
	opNotWordBound,         // \B

	opType,                 // \d\s\w\l\u\e
	opNotType,              // \D\S\W\L\U\E

	opCharAny,              // .
	opCharAnyAll,           // . in single line mode

	opSymbol,               // single char
	opNotSymbol,            // [^c] negative charclass with one char
	opSymbolIgnoreCase,     // symbol with IGNORE_CASE turned on
	opNotSymbolIgnoreCase,  // [^c] with ignore case set.

	opSymbolClass,          // [chars]

	opOpenBracket,          // (

	opClosingBracket,       // )

	opAlternative,          // |

	opBackRef,              // \1

	opNamedBracket,         // (?{name}
	opNamedBackRef,         // \p{name}

	opRangesBegin,          // for op type check

	opRange,                // generic range
	opMinRange,             // generic minimizing range

	opSymbolRange,          // quantifier applied to single char
	opSymbolMinRange,       // minimizing quantifier

	opNotSymbolRange,       // [^x]
	opNotSymbolMinRange,

	opAnyRange,             // .
	opAnyMinRange,

	opTypeRange,            // \w, \d, \s
	opTypeMinRange,

	opNotTypeRange,         // \W, \D, \S
	opNotTypeMinRange,

	opClassRange,           // for char classes
	opClassMinRange,

	opBracketRange,         // for brackets
	opBracketMinRange,

	opBackRefRange,         // for backrefs
	opBackRefMinRange,

	opNamedRefRange,
	opNamedRefMinRange,

	opRangesEnd,            // end of ranges

	opAssertionsBegin,

	opLookAhead,
	opNotLookAhead,

	opLookBehind,
	opNotLookBehind,

	opAsserionsEnd,

	opNoReturn,

	opRegExpEnd
};

struct REOpCode_data
{
	int op;
#ifdef RE_DEBUG
	int    srcpos;
#endif

	struct SBracket
	{
		RegExp::REOpCode* nextalt;
		int index;
		RegExp::REOpCode* pairindex;
	};

	struct SRange
	{
		union
		{
			SBracket bracket;
			int op;
			wchar_t symbol;
			RegExp::UniSet *symbolclass;
			RegExp::REOpCode* nextalt;
			int refindex;
			const wchar_t* refname;
			int type;
		};
		int min,max;
	};

	struct SNamedBracket
	{
		RegExp::REOpCode* nextalt;
		const wchar_t* name;
		RegExp::REOpCode* pairindex;
	};

	struct SAssert
	{
		RegExp::REOpCode* nextalt;
		int length;
		RegExp::REOpCode* pairindex;
	};

	struct SAlternative
	{
		RegExp::REOpCode* nextalt;
		RegExp::REOpCode* endindex;
	};

	union
	{
		SRange range;
		SBracket bracket;
		SNamedBracket nbracket;
		SAssert assert;
		SAlternative alternative;
		wchar_t symbol;
		RegExp::UniSet *symbolclass;
		int refindex;
		const wchar_t* refname;
		int type;
	};
};

struct RegExp::REOpCode: public REOpCode_data
{
	REOpCode()
	{
		ClearStruct(*static_cast<REOpCode_data*>(this));
	}

	~REOpCode()
	{
		switch (op)
		{
		case opSymbolClass:delete symbolclass; break;
		case opClassRange:
		case opClassMinRange:delete range.symbolclass; break;
		case opNamedBracket:delete[] nbracket.name; break;
		case opNamedBackRef:delete[] refname; break;
		}
	}
};

RegExp::RegExp():
	code(),
	slashChar('/'),
	backslashChar('\\'),
	firstptr(std::make_unique<UniSet>()),
	first(*firstptr),
	havefirst(),
	havelookahead(),
	minlength(),
	errorcode(errNotCompiled),
	errorpos(),
	ignorecase(),
	bracketscount(),
	maxbackref(),
	havenamedbrackets(),
	brhandler(nullptr),
	brhdata(nullptr)
{
}

RegExp::~RegExp()
{
}

int RegExp::CalcLength(const wchar_t* src,int srclength)
{
	int length=3;//global brackets
	int brackets[MAXDEPTH];
	int count=0;
	int i,save;
	bracketscount=1;
	int inquote=0;

	for (i=0; i<srclength; i++,length++)
	{
		if (inquote && src[i]!=backslashChar && src[i+1] != L'E')
		{
			continue;
		}

		if (src[i]==backslashChar)
		{
			i++;

			if (src[i] == L'Q')inquote=1;

			if (src[i] == L'E')inquote=0;

			if (src[i] == L'x')
			{
				i++;
				if(isxdigit(src[i]))
				{
					for(int j=1,k=i;j<4;j++)
					{
						if(isxdigit(src[k+j]))
						{
							i++;
						}
						else
						{
							break;
						}
					}
				}
				else return SetError(errSyntax,i);
			}

			if (src[i] == L'p')
			{
				i++;

				if (src[i] != L'{')
					return SetError(errSyntax, i);

				i++;
				int save2 = i;

				while (i < srclength && (ISWORD(src[i]) || ISSPACE(src[i])) && src[i] != L'}')
					i++;

				if (i >= srclength)
					return SetError(errBrackets, save2);

				if (src[i] != L'}' && !(ISWORD(src[i]) || ISSPACE(src[i])))
					return SetError(errSyntax, i);
			}

			continue;
		}

		switch (src[i])
		{
			case L'(':
			{
				brackets[count++]=i;
				if (count >= MAXDEPTH)
					return SetError(errMaxDepth, i);

				if (src[i+1]==L'?')
				{
					i+=2;

					if (src[i] == L'{')
					{
						save = i;
						i++;

						while (i < srclength && (ISWORD(src[i]) || ISSPACE(src[i])) && src[i] != L'}')
							i++;

						if (i >= srclength)
							return SetError(errBrackets, save);

						if (src[i] != L'}' && !(ISWORD(src[i]) || ISSPACE(src[i])))
							return SetError(errSyntax, i);
					}
				}
				else
				{
					bracketscount++;
				}

				break;
			}
			case L')':
			{
				count--;

				if (count < 0)
					return SetError(errBrackets,i);

				break;
			}
			case L'{':
			case L'*':
			case L'+':
			case L'?':
			{
				length--;

				if (src[i] == L'{')
				{
					save=i;

					while (i < srclength && src[i] != L'}')
						++i;

					if (i >= srclength)
						return SetError(errBrackets,save);
				}

				if (src[i+1] == '?')
					++i;

				break;
			}
			case L'[':
			{
				save=i;

				while (i < srclength && src[i] != L']')
					i += (backslashChar == src[i] && src[i+1] ? 2 : 1);

				if (i >= srclength)
					return SetError(errBrackets,save);

				break;
			}
		}
	}

	if (count)
	{
		errorpos=brackets[0];
		errorcode=errBrackets;
		return 0;
	}

	return length;
}

int RegExp::Compile(const wchar_t* src,int options)
{
	int srcstart=0,srclength/*=0*/,relength;

	if (options&OP_CPPMODE)
	{
		slashChar='\\';
		backslashChar='/';
	}
	else
	{
		slashChar='/';
		backslashChar='\\';
	}

	havefirst=0;

	code.clear();

	if (options&OP_PERLSTYLE)
	{
		if (src[0]!=slashChar)return SetError(errSyntax,0);

		srcstart=1;
		srclength=1;

		while (src[srclength] && src[srclength]!=slashChar)
		{
			if (src[srclength]==backslashChar && src[srclength+1])
			{
				srclength++;
			}

			srclength++;
		}

		if (!src[srclength])
		{
			return SetError(errSyntax, srclength);
		}

		int i=srclength+1;
		srclength--;

		while (src[i])
		{
			switch (src[i])
			{
				case 'i':options|=OP_IGNORECASE; break;
				case 's':options|=OP_SINGLELINE; break;
				case 'm':options|=OP_MULTILINE; break;
				case 'x':options|=OP_XTENDEDSYNTAX; break;
				case 'o':options|=OP_OPTIMIZE; break;
				default:return SetError(errOptions,i);
			}

			i++;
		}
	}
	else
	{
		srclength=(int)wcslen(src);
	}

	ignorecase=options&OP_IGNORECASE?1:0;
	relength=CalcLength(src+srcstart,srclength);

	if (!relength)
	{
		return 0;
	}

	code.resize(relength);

	int result = InnerCompile(src, src + srcstart, srclength, options);

	if (!result)
	{
		code.clear();
	}
	else
	{
		errorcode=errNone;
		minlength=0;

		if (options&OP_OPTIMIZE)Optimize();
	}

	return result;
}

static int GetNum(const wchar_t* src,int& i)
{
	int res=0;

	while (ISDIGIT(src[i]))
	{
		res*=10;
		res+=src[i]-'0';
		i++;
	}

	return res;
}

static int CalcPatternLength(const RegExp::REOpCode* from, const RegExp::REOpCode* to)
{
	int len=0;
	int altcnt=0;
	int altlen=-1;

	for (; from <= to; ++from)
	{
		switch (from->op)
		{
				//zero width
			case opLineStart:
			case opLineEnd:
			case opDataStart:
			case opDataEnd:
			case opWordBound:
			case opNotWordBound:continue;
			case opType:
			case opNotType:
			case opCharAny:
			case opCharAnyAll:
			case opSymbol:
			case opNotSymbol:
			case opSymbolIgnoreCase:
			case opNotSymbolIgnoreCase:
			case opSymbolClass:
				len++;
				altcnt++;
				continue;
			case opNamedBracket:
			case opOpenBracket:
			{
				int l=CalcPatternLength(from + 1, from->bracket.pairindex - 1);

				if (l==-1)return -1;

				len+=l;
				altcnt+=l;
				from=from->bracket.pairindex;
				continue;
			}
			case opClosingBracket:
				break;
			case opAlternative:

				if (altlen!=-1 && altcnt!=altlen)return -1;

				altlen=altcnt;
				altcnt=0;
				continue;
			case opBackRef:
			case opNamedBackRef:
				return -1;
			case opRangesBegin:
			case opRange:
			case opMinRange:
			case opSymbolRange:
			case opSymbolMinRange:
			case opNotSymbolRange:
			case opNotSymbolMinRange:
			case opAnyRange:
			case opAnyMinRange:
			case opTypeRange:
			case opTypeMinRange:
			case opNotTypeRange:
			case opNotTypeMinRange:
			case opClassRange:
			case opClassMinRange:

				if (from->range.min!=from->range.max)return -1;

				len+=from->range.min;
				altcnt+=from->range.min;
				continue;
			case opBracketRange:
			case opBracketMinRange:
			{
				if (from->range.min!=from->range.max)return -1;

				int l=CalcPatternLength(from + 1,from->bracket.pairindex - 1);

				if (l==-1)return -1;

				len+=from->range.min*l;
				altcnt+=from->range.min*l;
				from=from->bracket.pairindex;
				continue;
			}
			case opBackRefRange:
			case opBackRefMinRange:
			case opNamedRefRange:
			case opNamedRefMinRange:
				return -1;
			case opRangesEnd:
			case opAssertionsBegin:
			case opLookAhead:
			case opNotLookAhead:
			case opLookBehind:
			case opNotLookBehind:
				from=from->assert.pairindex;
				continue;
			case opAsserionsEnd:
			case opNoReturn:
				continue;
		}
	}

	if (altlen!=-1 && altlen!=altcnt)return -1;

	return altlen==-1?len:altlen;
}

int RegExp::InnerCompile(const wchar_t* const start, const wchar_t* src, int srclength, int options)
{
	REOpCode* brackets[MAXDEPTH];
	// current brackets depth
	// one place reserved for surrounding 'main' brackets
	int brdepth=1;
	// compiling interior of lookbehind
	// used to apply restrictions of lookbehind
	int lookbehind=0;
	// counter of normal brackets
	int brcount=0;
	// counter of closed brackets
	// used to check correctness of back-references
	std::vector<bool> closedbrackets(1);
	// quoting is active
	int inquote=0;
	maxbackref=0;
	UniSet *tmpclass;
	code[0].op=opOpenBracket;
	code[0].bracket.index = 0;
	MatchHash h;
	RegExpMatch m = {};
	int pos=1;
	REOpCode* op;//=code;
	brackets[0]=code.data();
#ifdef RE_DEBUG
	resrc = L"(";
	resrc.append(src, srclength).append(L")\x2190");
#endif
	havelookahead=0;

	for (int i=0; i<srclength; i++)
	{
		op = code.data() + pos;
		pos++;
#ifdef RE_DEBUG
		op->srcpos=i+1;
#endif

		if (inquote && src[i]!=backslashChar)
		{
			op->op=ignorecase?opSymbolIgnoreCase:opSymbol;
			op->symbol=ignorecase?TOLOWER(src[i]):src[i];

			if (ignorecase && TOUPPER(op->symbol)==op->symbol)op->op=opSymbol;

			continue;
		}

		if (src[i]==backslashChar)
		{
			i++;

			if (inquote && src[i]!='E')
			{
				op->op=opSymbol;
				op->symbol=backslashChar;
				op=code.data()+pos;
				pos++;
				op->op=ignorecase?opSymbolIgnoreCase:opSymbol;
				op->symbol=ignorecase?TOLOWER(src[i]):src[i];

				if (ignorecase && TOUPPER(op->symbol)==op->symbol)op->op=opSymbol;

				continue;
			}

			op->op=opType;

			switch (src[i])
			{
				case 'Q':inquote=1; pos--; continue;
				case 'E':inquote=0; pos--; continue;
				case 'b':op->op=opWordBound; continue;
				case 'B':op->op=opNotWordBound; continue;
				case 'D':op->op=opNotType;
				case 'd':op->type=TYPE_DIGITCHAR; continue;
				case 'S':op->op=opNotType;
				case 's':op->type=TYPE_SPACECHAR; continue;
				case 'W':op->op=opNotType;
				case 'w':op->type=TYPE_WORDCHAR; continue;
				case 'U':op->op=opNotType;
				case 'u':op->type=TYPE_UPCASE; continue;
				case 'L':op->op=opNotType;
				case 'l':op->type=TYPE_LOWCASE; continue;
				case 'I':op->op=opNotType;
				case 'i':op->type=TYPE_ALPHACHAR; continue;
				case 'A':op->op=opDataStart; continue;
				case 'Z':op->op=opDataEnd; continue;
				case 'n':op->op=opSymbol; op->symbol='\n'; continue;
				case 'r':op->op=opSymbol; op->symbol='\r'; continue;
				case 't':op->op=opSymbol; op->symbol='\t'; continue;
				case 'f':op->op=opSymbol; op->symbol='\f'; continue;
				case 'e':op->op=opSymbol; op->symbol=27; continue;
				case 'O':op->op=opNoReturn; continue;
				case 'p':
				{
					op->op = opNamedBackRef;
					i++;

					if (src[i] != L'{')return SetError(errSyntax, i + (src - start));

					int len = 0; i++;

					while (src[i + len] != L'}')len++;

					if (len > 0)
					{
						auto Name = new wchar_t[len + 1];
						memcpy(Name, src + i, len*sizeof(wchar_t));
						Name[len] = 0;
						if (!h.count(Name))
						{
							delete[] Name;
							return SetError(errReferenceToUndefinedNamedBracket, i + (src - start));
						}
						op->refname = Name;

						i += len;
					}
					else
					{
						return SetError(errSyntax, i + (src - start));
					}
				} continue;

				case 'x':
				{
					i++;

					if (i >= srclength)return SetError(errSyntax, i + (src - start) - 1);

					if(isxdigit(src[i]))
					{
						int c=TOLOWER(src[i])-'0';

						if (c>9)c-='a'-'0'-10;

						op->op=ignorecase?opSymbolIgnoreCase:opSymbol;
						op->symbol=c;
						for(int j=1,k=i;j<4 && k+j<srclength;j++)
						{
							if(isxdigit(src[k+j]))
							{
								i++;
								c=TOLOWER(src[k+j])-'0';
								if (c>9)c-='a'-'0'-10;
								op->symbol<<=4;
								op->symbol|=c;
							}
							else
							{
								break;
							}
						}
						if (ignorecase)
						{
							op->symbol=TOLOWER(op->symbol);
							if (TOUPPER(op->symbol)==TOLOWER(op->symbol))
							{
								op->op=opSymbol;
							}
						}
					}
					else return SetError(errSyntax, i + (src - start));

					continue;
				}
				default:
				{
					if (ISDIGIT(src[i]))
					{
						int save=i;
						op->op=opBackRef;
						op->refindex=GetNum(src,i); i--;

						if (op->refindex<=0 || op->refindex>brcount || !closedbrackets[op->refindex])
						{
							return SetError(errInvalidBackRef, save + (src - start) - 1);
						}

						if (op->refindex>maxbackref)maxbackref=op->refindex;
					}
					else
					{
						if (options&OP_STRICT && ISALPHA(src[i]))
						{
							return SetError(errInvalidEscape, i + (src - start) - 1);
						}

						op->op=ignorecase?opSymbolIgnoreCase:opSymbol;
						op->symbol=ignorecase?TOLOWER(src[i]):src[i];

						if (TOLOWER(op->symbol)==TOUPPER(op->symbol))
						{
							op->op=opSymbol;
						}
					}
				}
			}

			continue;
		}

		switch (src[i])
		{
			case '.':
			{
				if (options&OP_SINGLELINE)
				{
					op->op=opCharAnyAll;
				}
				else
				{
					op->op=opCharAny;
				}

				continue;
			}
			case '^':
			{
				if (options&OP_MULTILINE)
				{
					op->op=opLineStart;
				}
				else
				{
					op->op=opDataStart;
				}

				continue;
			}
			case '$':
			{
				if (options&OP_MULTILINE)
				{
					op->op=opLineEnd;
				}
				else
				{
					op->op=opDataEnd;
				}

				continue;
			}
			case '|':
			{
				if (brackets[brdepth-1]->op==opAlternative)
				{
					brackets[brdepth-1]->alternative.nextalt=op;
				}
				else
				{
					if (brackets[brdepth-1]->op==opOpenBracket)
					{
						brackets[brdepth-1]->bracket.nextalt=op;
					}
					else
					{
						brackets[brdepth-1]->assert.nextalt=op;
					}
				}

				if (brdepth >= MAXDEPTH)
					return SetError(errMaxDepth, i + (src - start));

				brackets[brdepth++]=op;
				op->op=opAlternative;
				continue;
			}
			case '(':
			{
				op->op=opOpenBracket;

				if (src[i+1]=='?')
				{
					i+=2;

					switch (src[i])
					{
						case ':':op->bracket.index=-1; break;
						case '=':op->op=opLookAhead; havelookahead=1; break;
						case '!':op->op=opNotLookAhead; havelookahead=1; break;
						case '<':
						{
							i++;

							if (src[i]=='=')
							{
								op->op=opLookBehind;
							}
							else if (src[i]=='!')
							{
								op->op=opNotLookBehind;
							}
							else
								return SetError(errSyntax, i + (src - start));

							lookbehind++;
						} break;
						case L'{':
						{
							op->op = opNamedBracket;
							havenamedbrackets = 1;
							int len = 0;
							i++;

							while (src[i + len] != L'}')len++;

							if (len > 0)
							{
								auto Name = new wchar_t[len + 1];
								memcpy(Name, src + i, len*sizeof(wchar_t));
								Name[len] = 0;
								op->nbracket.name = Name;
							}
							else
							{
								op->op = opOpenBracket;
								op->bracket.index = -1;
							}

							i += len;
						} break;
						default:
						{
							return SetError(errSyntax, i + (src - start));
						}
					}
				}
				else
				{
					++brcount;
					closedbrackets.push_back(false);
					op->bracket.index=brcount;
				}

				brackets[brdepth]=op;
				brdepth++;
				continue;
			}
			case ')':
			{
				op->op=opClosingBracket;
				brdepth--;

				while (brackets[brdepth]->op==opAlternative)
				{
					brackets[brdepth]->alternative.endindex=op;
					brdepth--;
				}

				switch (brackets[brdepth]->op)
				{
					case opOpenBracket:
					{
						op->bracket.pairindex=brackets[brdepth];
						brackets[brdepth]->bracket.pairindex=op;
						op->bracket.index=brackets[brdepth]->bracket.index;

						if (op->bracket.index!=-1)
						{
							closedbrackets[op->bracket.index]=true;
						}

						break;
					}
					case opNamedBracket:
					{
						op->nbracket.pairindex = brackets[brdepth];
						brackets[brdepth]->nbracket.pairindex = op;
						op->nbracket.name = brackets[brdepth]->nbracket.name;
						h[op->nbracket.name] = m;
						break;
					}
					case opLookBehind:
					case opNotLookBehind:
					{
						lookbehind--;
						int l=CalcPatternLength(brackets[brdepth] + 1, op - 1);

						if (l == -1)return SetError(errVariableLengthLookBehind, i + (src - start));

						brackets[brdepth]->assert.length=l;
					}// there is no break and this is correct!
					case opLookAhead:
					case opNotLookAhead:
					{
						op->assert.pairindex=brackets[brdepth];
						brackets[brdepth]->assert.pairindex=op;
						break;
					}
				}

				continue;
			}
			case '[':
			{
				i++;
				int negative=0;

				if (src[i]=='^')
				{
					negative=1;
					i++;
				}

				int lastchar=-1;
				int classsize=0;
				op->op=opSymbolClass;
				//op->symbolclass=new wchar_t[32]();
				op->symbolclass=new UniSet();
				tmpclass=op->symbolclass;

				for (; src[i]!=']'; i++)
				{
					if (src[i]==backslashChar)
					{
						i++;
						int isnottype=0;
						int type=0;
						lastchar=-1;

						switch (src[i])
						{
							case 'D':isnottype=1;
							case 'd':type=TYPE_DIGITCHAR; break;
							case 'W':isnottype=1;
							case 'w':type=TYPE_WORDCHAR; break;
							case 'S':isnottype=1;
							case 's':type=TYPE_SPACECHAR; break;
							case 'L':isnottype=1;
							case 'l':type=TYPE_LOWCASE; break;
							case 'U':isnottype=1;
							case 'u':type=TYPE_UPCASE; break;
							case 'I':isnottype=1;
							case 'i':type=TYPE_ALPHACHAR; break;
							case 'n':lastchar='\n'; break;
							case 'r':lastchar='\r'; break;
							case 't':lastchar='\t'; break;
							case 'f':lastchar='\f'; break;
							case 'e':lastchar=27; break;
							case 'x':
							{
								i++;

								if (i >= srclength)return SetError(errSyntax, i + (src - start) - 1);

								if (isxdigit(src[i]))
								{
									int c=TOLOWER(src[i])-'0';

									if (c>9)c-='a'-'0'-10;

									lastchar=c;

									for(int j=1,k=i;j<4 && k+j<srclength;j++)
									{
										if (isxdigit(src[k+j]))
										{
											i++;
											c=TOLOWER(src[k+j])-'0';

											if (c>9)c-='a'-'0'-10;

											lastchar<<=4;
											lastchar|=c;
										}
										else
										{
											break;
										}
									}
									dpf((L"Last char=%c(%02x)\n",lastchar,lastchar));
								}
								else return SetError(errSyntax, i + (src - start));

								break;
							}
							default:
							{
								if (options&OP_STRICT && ISALPHA(src[i]))
								{
									return SetError(errInvalidEscape, i + (src - start) - 1);
								}

								lastchar=src[i];
							}
						}

						if (type)
						{
							if (isnottype)
							{
								tmpclass->nottypes|=type;
							}
							else
							{
								tmpclass->types|=type;
							}
							classsize=257;
							//for(int j=0;j<32;j++)op->symbolclass[j]|=charbits[classindex+j]^isnottype;
							//classsize+=charsizes[classindex>>5];
							//int setbit;
							/*for(int j=0;j<256;j++)
							{
							  setbit=(chartypes[j]^isnottype)&type;
							  if(setbit)
							  {
							    if(ignorecase)
							    {
							      SetBit(op->symbolclass,lc[j]);
							      SetBit(op->symbolclass,uc[j]);
							    }else
							    {
							      SetBit(op->symbolclass,j);
							    }
							    classsize++;
							  }
							}*/
						}
						else
						{
							if (options&OP_IGNORECASE)
							{
								tmpclass->SetBit(TOLOWER(lastchar));
								tmpclass->SetBit(TOUPPER(lastchar));
							}
							else
							{
								tmpclass->SetBit(lastchar);
							}

							classsize++;
						}

						continue;
					}

					if (src[i]=='-')
					{
						if (lastchar!=-1 && src[i+1]!=']')
						{
							int to=src[i+1];

							if (to==backslashChar)
							{
								to=src[i+2];

								if (to=='x')
								{
									i+=2;
									to=TOLOWER(src[i+1]);

									if(isxdigit(to))
									{
										to-='0';

										if (to>9)to-='a'-'0'-10;

										for(int j=1,k=(i+1);j<4 && k+j<srclength;j++)
										{
											int c=TOLOWER(src[k+j]);
											if(isxdigit(c))
											{
												i++;
												c-='0';

												if (c>9)c-='a'-'0'-10;

												to<<=4;
												to|=c;
											}
											else
											{
												break;
											}
										}
									}
									else return SetError(errSyntax, i + (src - start));
								}
								else
								{
									tmpclass->SetBit('-');
									classsize++;
									continue;
								}
							}

							i++;
							dpf((L"from %d to %d\n",lastchar,to));

							for (int j=lastchar; j<=to; j++)
							{
								if (ignorecase)
								{
									tmpclass->SetBit(TOLOWER(j));
									tmpclass->SetBit(TOUPPER(j));
								}
								else
								{
									tmpclass->SetBit(j);
								}

								classsize++;
							}

							continue;
						}
					}

					lastchar=src[i];

					if (ignorecase)
					{
						tmpclass->SetBit(TOLOWER(lastchar));
						tmpclass->SetBit(TOUPPER(lastchar));
					}
					else
					{
						tmpclass->SetBit(lastchar);
					}

					classsize++;
				}

				if (negative && classsize>1)
				{
					tmpclass->negative=negative;
					//for(int j=0;j<32;j++)op->symbolclass[j]^=0xff;
				}

				if (classsize==1)
				{
					delete op->symbolclass;
					op->symbolclass = nullptr;
					tmpclass = nullptr;
					op->op=negative?opNotSymbol:opSymbol;

					if (ignorecase)
					{
						op->op+=2;
						op->symbol=TOLOWER(lastchar);
					}
					else
					{
						op->symbol=lastchar;
					}
				}

				if (tmpclass)tmpclass->negative=negative;
				continue;
			}
			case '+':
			case '*':
			case '?':
			case '{':
			{
				int min=0,max=0;

				switch (src[i])
				{
					case '+':min=1; max=-2; break;
					case '*':min=0; max=-2; break;
					case '?':
					{
						//if(src[i+1]=='?') return SetError(errInvalidQuantifiersCombination,i);
						min=0; max=1;
						break;
					}
					case '{':
					{
						i++;
						int save=i;
						min=GetNum(src,i);
						max=min;

						if (min<0)return SetError(errInvalidRange, save + (src - start));

//            i++;
						if (src[i]==',')
						{
							if (src[i+1]=='}')
							{
								i++;
								max=-2;
							}
							else
							{
								i++;
								max=GetNum(src,i);

//                i++;
								if (max<min)return SetError(errInvalidRange, save + (src - start));
							}
						}

						if (src[i] != '}')return SetError(errInvalidRange, save + (src - start));
					}
				}

				pos--;
				op = code.data() + pos - 1;

				if (min==1 && max==1)continue;

				op->range.min=min;
				op->range.max=max;

				switch (op->op)
				{
					case opLineStart:
					case opLineEnd:
					case opDataStart:
					case opDataEnd:
					case opWordBound:
					case opNotWordBound:
					{
						return SetError(errInvalidQuantifiersCombination, i + (src - start));
//            op->range.op=op->op;
//            op->op=opRange;
//            continue;
					}
					case opCharAny:
					case opCharAnyAll:
					{
						op->range.op=op->op;
						op->op=opAnyRange;
						break;
					}
					case opType:
					{
						op->op=opTypeRange;
						break;
					}
					case opNotType:
					{
						op->op=opNotTypeRange;
						break;
					}
					case opSymbolIgnoreCase:
					case opSymbol:
					{
						op->op=opSymbolRange;
						break;
					}
					case opNotSymbol:
					case opNotSymbolIgnoreCase:
					{
						op->op=opNotSymbolRange;
						break;
					}
					case opSymbolClass:
					{
						op->op=opClassRange;
						break;
					}
					case opBackRef:
					{
						op->op=opBackRefRange;
						break;
					}
					case opNamedBackRef:
					{
						op->op = opNamedRefRange;
						break;
					}
					case opClosingBracket:
					{
						op=op->bracket.pairindex;

						if (op->op != opOpenBracket)return SetError(errInvalidQuantifiersCombination, i + (src - start));

						op->range.min=min;
						op->range.max=max;
						op->op=opBracketRange;
						break;
					}
					default:
					{
						dpf((L"op->=%d\n",op->op));
						return SetError(errInvalidQuantifiersCombination, i + (src - start));
					}
				}//switch(code.op)

				if (src[i+1]=='?')
				{
					op->op++;
					i++;
				}

				continue;
			}// case +*?{
			case ' ':
			case '\t':
			case '\n':
			case '\r':
			{
				if (options&OP_XTENDEDSYNTAX)
				{
					pos--;
					continue;
				}
			}
			default:
			{
				op->op=options&OP_IGNORECASE?opSymbolIgnoreCase:opSymbol;

				if (ignorecase)
				{
					op->symbol=TOLOWER(src[i]);
				}
				else
				{
					op->symbol=src[i];
				}
			}
		}//switch(src[i])
	}//for()

	op = code.data() + pos;
	pos++;
	brdepth--;

	while (brdepth>=0 && brackets[brdepth]->op==opAlternative)
	{
		brackets[brdepth]->alternative.endindex=op;
		brdepth--;
	}

	op->op=opClosingBracket;
	op->bracket.pairindex = code.data();
	code[0].bracket.pairindex=op;
#ifdef RE_DEBUG
	op->srcpos=i;
#endif
	op = code.data() + pos;
	//pos++;
	op->op=opRegExpEnd;
#ifdef RE_DEBUG
	op->srcpos=i+1;
#endif
	return 1;
}

struct RegExp::StateStackItem
{
	StateStackItem():
		op(),
		pos(),
		savestr(),
		startstr(),
		min(),
		cnt(),
		max(),
		forward()
	{
	}

	int op;
	const REOpCode* pos;
	const wchar_t* savestr;
	const wchar_t* startstr;
	int min;
	int cnt;
	int max;
	int forward;
};

inline const RegExp::StateStackItem& FindStateByPos(const std::vector<RegExp::StateStackItem>& stack, RegExp::REOpCode* pos, int op)
{
	return *std::find_if(ALL_CONST_REVERSE_RANGE(stack), [&](CONST_REFERENCE(stack) i){ return i.pos == pos && i.op == op; });
}

inline int RegExp::StrCmp(const wchar_t*& str, const wchar_t* _st, const wchar_t* ed) const
{
	const wchar_t* save=str;

	if (ignorecase)
	{
		while (_st<ed)
		{
			if (TOLOWER(*str)!=TOLOWER(*_st)) {str=save; return 0;}

			str++;
			_st++;
		}
	}
	else
	{
		while (_st<ed)
		{
			if (*str!=*_st) {str=save; return 0;}

			str++;
			_st++;
		}
	}

	return 1;
}

int RegExp::InnerMatch(const wchar_t* const start, const wchar_t* str, const wchar_t* strend, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch, std::vector<StateStackItem>& stack) const
{
	int i,j;
	int minimizing;
	const REOpCode* tmp=nullptr;
	RegExpMatch* m;
	UniSet *cl;
	int inrangebracket=0;

	if (errorcode==errNotCompiled)return 0;

	if (matchcount<maxbackref)return SetError(errNotEnoughMatches,maxbackref);

	if (havenamedbrackets && !hmatch)return SetError(errNoStorageForNB, 0);

	stack.clear();

	errorcode=errNone;

	if (bracketscount<matchcount)matchcount=bracketscount;

	static const RegExpMatch def_match = { -1, -1 };
	std::fill_n(match, matchcount, def_match);

	for(const auto* op = code.data(), *end = op + code.size(); op != end; ++op)
	{
		//dpf(("op:%s,\tpos:%d,\tstr:%d\n",ops[op->op],pos,str-start));
		dpf((L"=================\n"));
		dpf((L"S:%s\n%*s\n", start, str - start + 3, "^"));
		dpf((L"R:%s\n%*s\n", resrc.data(), op->srcpos + 3, "^"));

		if (str<=strend)
		{
			const auto MinSkip = [&](StateStackItem& st, const std::function<bool(const wchar_t* str)>& cmp)
			{
				int jj;
				switch (std::next(op)->op)
				{
				case opSymbol:
					jj = std::next(op)->symbol;
					if(*str!=jj)
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(str[0]==jj)
								break;
						}
					}
					break;

				case opNotSymbol:
					jj = std::next(op)->symbol;
					if(*str==jj)
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(str[0]!=jj)
								break;
						}
					}
					break;

				case opSymbolIgnoreCase:
					jj = std::next(op)->symbol;
					if(TOLOWER(*str)!=jj)
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(TOLOWER(str[0])==jj)
								break;
						}
					}
					break;

				case opNotSymbolIgnoreCase:
					jj = std::next(op)->symbol;
					if(TOLOWER(*str)==jj)
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(TOLOWER(str[0])!=jj)
								break;
						}
					}
					break;

				case opType:
					jj = std::next(op)->type;
					if(!isType(*str,jj))
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(isType(str[0],jj))
								break;
						}
					}
					break;

				case opNotType:
					jj = std::next(op)->type;
					if(isType(*str,jj))
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(!isType(str[0],jj))
								break;
						}
					}
					break;

				case opSymbolClass:
					cl = std::next(op)->symbolclass;
					if(!cl->GetBit(*str))
					{
						while(str<strend && cmp(str) && st.max--)
						{
							str++;
							if(cl->GetBit(str[0]))
								break;
						}
					}
					break;
				}
			};

			switch (op->op)
			{
				case opLineStart:
				{
					if (str==start || str[-1]==0x0d || str[-1]==0x0a)continue;

					break;
				}
				case opLineEnd:
				{
					if (str==strend || str[0]==0x0d || str[0]==0x0a)continue;

					break;
				}
				case opDataStart:
				{
					if (str==start)continue;

					break;
				}
				case opDataEnd:
				{
					if (str==strend)continue;

					break;
				}
				case opWordBound:
				{
					if ((str==start && ISWORD(*str))||
					        (!(ISWORD(str[-1])) && ISWORD(*str)) ||
					        (!(ISWORD(*str)) && ISWORD(str[-1])) ||
					        (str==strend && ISWORD(str[-1])))continue;

					break;
				}
				case opNotWordBound:
				{
					if (!((str==start && ISWORD(*str))||
					        (!(ISWORD(str[-1])) && ISWORD(*str)) ||
					        (!(ISWORD(*str)) && ISWORD(str[-1])) ||
					        (str==strend && ISWORD(str[-1]))))continue;

					break;
				}
				case opType:
				{
					if (isType(*str,op->type))
					{
						str++;
						continue;
					}

					break;
				}
				case opNotType:
				{
					if (!isType(*str,op->type))
					{
						str++;
						continue;
					}

					break;
				}
				case opCharAny:
				{
					if (*str!=0x0d && *str!=0x0a)
					{
						str++;
						continue;
					}

					break;
				}
				case opCharAnyAll:
				{
					str++;
					continue;
				}
				case opSymbol:
				{
					if (*str==op->symbol)
					{
						str++;
						continue;
					}

					break;
				}
				case opNotSymbol:
				{
					if (*str!=op->symbol)
					{
						str++;
						continue;
					}

					break;
				}
				case opSymbolIgnoreCase:
				{
					if (TOLOWER(*str)==op->symbol)
					{
						str++;
						continue;
					}

					break;
				}
				case opNotSymbolIgnoreCase:
				{
					if (TOLOWER(*str)!=op->symbol)
					{
						str++;
						continue;
					}

					break;
				}
				case opSymbolClass:
				{
					if (op->symbolclass->GetBit(*str))
					{
						str++;
						continue;
					}

					break;
				}
				case opOpenBracket:
				{
					if (op->bracket.index>=0 && op->bracket.index<matchcount)
					{
						//if (inrangebracket) Mantis#1388
						{
							StateStackItem st;
							st.op=opOpenBracket;
							st.pos=op;
							st.min=match[op->bracket.index].start;
							st.max=match[op->bracket.index].end;
							stack.push_back(st);
						}

						match[op->bracket.index].start = str - start;
					}

					if (op->bracket.nextalt)
					{
						StateStackItem st;
						st.op = opAlternative;
						st.pos=op->bracket.nextalt;
						st.savestr=str;
						stack.push_back(st);
					}

					continue;
				}
				case opNamedBracket:
				{
					if (hmatch)
					{
						RegExpMatch* m2;
						if (!hmatch->count(op->nbracket.name))
						{
							RegExpMatch sm;
							sm.start = -1;
							sm.end = -1;
							(*hmatch)[op->nbracket.name] = sm;
						}
						m2 = &(*hmatch)[op->nbracket.name];

						//if (inrangebracket) Mantis#1388
						{
							StateStackItem st;
							st.op = opNamedBracket;
							st.pos = op;
							st.min = m2->start;
							st.max = m2->end;
							stack.push_back(st);
						}

						m2->start = str - start;
					}

					if (op->bracket.nextalt)
					{
						StateStackItem st;
						st.op = opAlternative;
						st.pos = op->bracket.nextalt;
						st.savestr = str;
						stack.push_back(st);
					}

					continue;
				}
				case opClosingBracket:
				{
					switch (op->bracket.pairindex->op)
					{
						case opOpenBracket:
						{
							if (op->bracket.index>=0 && op->bracket.index<matchcount)
							{
								match[op->bracket.index].end = str - start;

								if (brhandler)
								{
									if (
									    !brhandler
									    (
									        brhdata,
									        bhMatch,
									        op->bracket.index,
									        match[op->bracket.index].start,
									        match[op->bracket.index].end
									    )
									)
									{
										return -1;
									}
								}
							}

							continue;
						}
						case opNamedBracket:
						{
							if (hmatch)
							{
								(*hmatch)[op->nbracket.name].end = str - start;
							}

							continue;
						}
						case opBracketRange:
						{
							auto st = FindStateByPos(stack, op->bracket.pairindex,opBracketRange);

							if (str==st.startstr)
							{
								if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
								{
									match[op->range.bracket.index].end = str - start;

									if (brhandler)
									{
										if (
										    !brhandler
										    (
										        brhdata,
										        bhMatch,
										        op->range.bracket.index,
										        match[op->range.bracket.index].start,
										        match[op->range.bracket.index].end
										    )
										)
										{
											return -1;
										}
									}
								}

								inrangebracket--;
								continue;
							}

							if (st.min>0)st.min--;

							if (st.min)
							{
								st.max--;
								st.startstr=str;
								st.savestr=str;
								op=st.pos;
								stack.push_back(st);

								if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
								{
									StateStackItem Item;
									match[op->range.bracket.index].start = str - start;
									Item.op=opOpenBracket;
									Item.pos=op;
									Item.min=match[op->range.bracket.index].start;
									Item.max=match[op->range.bracket.index].end;
									stack.push_back(Item);
								}

								if (op->range.bracket.nextalt)
								{
									StateStackItem Item;
									Item.op=opAlternative;
									Item.pos=op->range.bracket.nextalt;
									Item.savestr=str;
									stack.push_back(Item);
								}

								continue;
							}

							st.max--;

							if (!st.max)
							{
								if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
								{
									match[op->range.bracket.index].end = str - start;

									if (brhandler)
									{
										if (
										    !brhandler
										    (
										        brhdata,
										        bhMatch,
										        op->range.bracket.index,
										        match[op->range.bracket.index].start,
										        match[op->range.bracket.index].end
										    )
										)
										{
											return -1;
										}
									}
								}

								inrangebracket--;
								continue;
							}

							if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
							{
								match[op->range.bracket.index].end = str - start;

								if (brhandler)
								{
									if (
									    !brhandler
									    (
									        brhdata,
									        bhMatch,
									        op->range.bracket.index,
									        match[op->range.bracket.index].start,
									        match[op->range.bracket.index].end
									    )
									)
									{
										return -1;
									}
								}

								tmp=op;
							}

							st.startstr=str;
							st.savestr=str;
							op=st.pos;
							stack.push_back(st);

							if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
							{
								StateStackItem Item;
								Item.op=opOpenBracket;
								Item.pos=tmp;
								Item.min=match[op->range.bracket.index].start;
								Item.max=match[op->range.bracket.index].end;
								stack.push_back(Item);
								match[op->range.bracket.index].start = str-start;
							}

							if (op->range.bracket.nextalt)
							{
								StateStackItem Item;
								Item.op=opAlternative;
								Item.pos=op->range.bracket.nextalt;
								Item.savestr=str;
								stack.push_back(Item);
							}

							continue;
						}
						case opBracketMinRange:
						{
							auto& ps = FindStateByPos(stack, op->bracket.pairindex, opBracketMinRange);
							auto st = ps;

							if (st.min>0)st.min--;

							if (st.min)
							{
								//st.min--;
								st.max--;
								st.startstr=str;
								st.savestr=str;
								op=st.pos;
								stack.push_back(st);

								if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
								{
									if (brhandler)
									{
										if (
										    !brhandler
										    (
										        brhdata,
										        bhMatch,
										        op->range.bracket.index,
										        match[op->range.bracket.index].start,
										        (int)(str-start)
										    )
										)
										{
											return -1;
										}
									}

									match[op->range.bracket.index].start = str - start;
									st.op=opOpenBracket;
									st.pos=op;
									st.min=match[op->range.bracket.index].start;
									st.max=match[op->range.bracket.index].end;
									stack.push_back(st);
								}

								if (op->range.bracket.nextalt)
								{
									st.op=opAlternative;
									st.pos=op->range.bracket.nextalt;
									st.savestr=str;
									stack.push_back(st);
								}

								continue;
							}

							if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
							{
								match[op->range.bracket.index].end = str - start;

								if (brhandler)
								{
									if (
									    !brhandler
									    (
									        brhdata,
									        bhMatch,
									        op->range.bracket.index,
									        match[op->range.bracket.index].start,
									        match[op->range.bracket.index].end
									    )
									)
									{
										return -1;
									}
								}
							}

							st.max--;
							inrangebracket--;

							if (!st.max)continue;

							st.forward=str>ps.startstr?1:0;
							st.startstr=str;
							st.savestr=str;
							stack.push_back(st);

							if (op->range.bracket.nextalt)
							{
								st.op=opAlternative;
								st.pos=op->range.bracket.nextalt;
								st.savestr=str;
								stack.push_back(st);
							}

							continue;
						}
						case opLookAhead:
						{
							tmp=op->bracket.pairindex;

							while (stack.back().pos != tmp || stack.back().op != opLookAhead)
							{
								stack.pop_back();
							}
							str = stack.back().savestr;
							stack.pop_back();
							continue;
						}
						case opNotLookAhead:
						{
							while (stack.back().op!=opNotLookAhead)
							{
								stack.pop_back();
							}
							str = stack.back().savestr;
							stack.pop_back();
							break;
						}
						case opLookBehind:
						{
							continue;
						}
						case opNotLookBehind:
						{
							stack.back().forward=0;
							break;
						}
					}//switch(code[pairindex].op)

					break;
				}//case opClosingBracket
				case opAlternative:
				{
					op = std::prev(op->alternative.endindex);
					continue;
				}
				case opNamedBackRef:
				case opBackRef:
				{
					if (op->op == opBackRef)
					{
						m = &match[op->refindex];
					}
					else
					{
						if (!hmatch || !hmatch->count(op->refname))break;
						m = &(*hmatch)[op->refname];
					}

					if (m->start==-1 || m->end==-1)break;

					if (ignorecase)
					{
						j=m->end;

						for (i=m->start; i<j; i++,str++)
						{
							if (TOLOWER(start[i])!=TOLOWER(*str))break;

							if (str>strend)break;
						}

						if (i<j)break;
					}
					else
					{
						j=m->end;

						for (i=m->start; i<j; i++,str++)
						{
							if (start[i]!=*str)break;

							if (str>strend)break;
						}

						if (i<j)break;
					}

					continue;
				}
				case opAnyRange:
				case opAnyMinRange:
				{
					StateStackItem st;
					st.op=op->op;
					minimizing=op->op==opAnyMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					if (op->range.op==opCharAny)
					{
						for (i=0; i<j; i++,str++)
						{
							if (str>strend || *str==0x0d || *str==0x0a)break;
						}

						if (i<j)
						{
							break;
						}

						st.startstr=str;

						if (!minimizing)
						{
							while (str<strend && *str!=0x0d && *str!=0x0a && st.max--)str++;
						}
						else
						{
							MinSkip(st, [](const wchar_t* str) { return *str != L'\r' && *str != L'\n'; });

							if (st.max==-1)break;
						}
					}
					else
					{
						//opCharAnyAll:
						str+=j;

						if (str>strend)break;

						st.startstr=str;

						if (!minimizing)
						{
							if (st.max>=0)
							{
								if (str+st.max<strend)
								{
									str+=st.max;
									st.max=0;
								}
								else
								{
									st.max-=(int)(strend-str);
									str=strend;
								}
							}
							else
							{
								str=strend;
							}
						}
						else
						{
							MinSkip(st, [](const wchar_t* str) { return true; });

							if (st.max==-1)break;
						}
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opSymbolRange:
				case opSymbolMinRange:
				{
					StateStackItem st;
					st.op=op->op;
					minimizing=op->op==opSymbolMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					if (ignorecase)
					{
						for (i=0; i<j; i++,str++)
						{
							if (str>strend || TOLOWER(*str)!=op->range.symbol)break;
						}

						if (i<j)break;

						st.startstr=str;

						if (!minimizing)
						{
							while (str<strend && TOLOWER(*str)==op->range.symbol && st.max--)str++;
						}
						else
						{
							MinSkip(st, [op](const wchar_t* str) { return TOLOWER(*str) == op->range.symbol; });

							if (st.max==-1)break;
						}
					}
					else
					{
						for (i=0; i<j; i++,str++)
						{
							if (str>strend || *str!=op->range.symbol)break;
						}

						if (i<j)break;

						st.startstr=str;

						if (!minimizing)
						{
							while (str<strend && *str==op->range.symbol && st.max--)str++;
						}
						else
						{
							MinSkip(st, [op](const wchar_t* str) { return *str == op->range.symbol; });

							if (st.max==-1)break;
						}
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opNotSymbolRange:
				case opNotSymbolMinRange:
				{
					StateStackItem st;
					st.op=op->op;
					minimizing=op->op==opNotSymbolMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					if (ignorecase)
					{
						for (i=0; i<j; i++,str++)
						{
							if (str>strend || TOLOWER(*str)==op->range.symbol)break;
						}

						if (i<j)break;

						st.startstr=str;

						if (!minimizing)
						{
							while (str<strend && TOLOWER(*str)!=op->range.symbol && st.max--)str++;
						}
						else
						{
							MinSkip(st, [op](const wchar_t* str) { return TOLOWER(*str) != op->range.symbol; });

							if (st.max==-1)break;
						}
					}
					else
					{
						for (i=0; i<j; i++,str++)
						{
							if (str>strend || *str==op->range.symbol)break;
						}

						if (i<j)break;

						st.startstr=str;

						if (!minimizing)
						{
							while (str<strend && *str!=op->range.symbol && st.max--)str++;
						}
						else
						{
							MinSkip(st, [op](const wchar_t* str) { return *str != op->range.symbol; });

							if (st.max==-1)break;
						}
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opClassRange:
				case opClassMinRange:
				{
					StateStackItem st;
					st.op=op->op;
					minimizing=op->op==opClassMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					for (i=0; i<j; i++,str++)
					{
						if (str>strend || !op->range.symbolclass->GetBit(*str))break;
					}

					if (i<j)break;

					st.startstr=str;

					if (!minimizing)
					{
						while (str<strend && op->range.symbolclass->GetBit(*str) && st.max--)str++;
					}
					else
					{
						MinSkip(st, [op](const wchar_t* str) { return op->range.symbolclass->GetBit(*str); });

						if (st.max==-1)break;
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opTypeRange:
				case opTypeMinRange:
				{
					StateStackItem st;
					st.op=op->op;
					minimizing=op->op==opTypeMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					for (i=0; i<j; i++,str++)
					{
						if (str>strend || !isType(*str,op->range.type))break;
					}

					if (i<j)break;

					st.startstr=str;

					if (!minimizing)
					{
						while (str<strend && isType(*str,op->range.type) && st.max--)str++;
					}
					else
					{
						MinSkip(st, [op](const wchar_t* str) { return isType(*str, op->range.type); });

						if (st.max==-1)break;
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opNotTypeRange:
				case opNotTypeMinRange:
				{
					StateStackItem st;
					st.op = op->op;
					minimizing=op->op==opNotTypeMinRange;
					j=op->range.min;
					st.max=op->range.max-j;

					for (i=0; i<j; i++,str++)
					{
						if (str>strend || isType(*str,op->range.type))break;
					}

					if (i<j)break;

					st.startstr=str;

					if (!minimizing)
					{
						while (str<strend && !isType(*str,op->range.type) && st.max--)str++;
					}
					else
					{
						MinSkip(st, [op](const wchar_t* str) { return !isType(*str, op->range.type); });

						if (st.max==-1)break;
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opNamedRefRange:
				case opNamedRefMinRange:
				case opBackRefRange:
				case opBackRefMinRange:
				{
					StateStackItem st;
					st.op = op->op;
					minimizing = op->op == opBackRefMinRange || op->op == opNamedRefMinRange;
					j=op->range.min;
					st.max=op->range.max-j;
					if (op->op == opBackRefRange || op->op == opBackRefMinRange)
					{
						m = &match[op->range.refindex];
					}
					else
					{
						m = &(*hmatch)[op->range.refname];
					}

					if (!m)break;

					if (m->start==-1 || m->end==-1)
					{
						if (!j)continue;
						else break;
					}

					for (i=0; i<j; i++)
					{
						if (str>strend || !StrCmp(str,start+m->start,start+m->end))break;
					}

					if (i<j)break;

					st.startstr=str;

					if (!minimizing)
					{
						while (str<strend && StrCmp(str,start+m->start,start+m->end) && st.max--);
					}
					else
					{
						MinSkip(st, [&](const wchar_t* str) { return this->StrCmp(str, start + m->start, start + m->end) != 0; });

						if (st.max==-1)break;
					}

					if (op->range.max==j)continue;

					st.savestr=str;
					st.pos=op;
					stack.push_back(st);
					continue;
				}
				case opBracketRange:
				case opBracketMinRange:
				{
					if (inrangebracket && op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
					{
						StateStackItem st;
						st.op=opOpenBracket;
						st.pos=op->range.bracket.pairindex;
						st.min=match[op->range.bracket.index].start;
						st.max=match[op->range.bracket.index].end;
						stack.push_back(st);
					}

					{
						StateStackItem st;
						st.op = op->op;
						st.pos = op;
						st.min = op->range.min;
						st.max = op->range.max;
						st.startstr = str;
						st.savestr = str;
						st.forward = 1;
						stack.push_back(st);
					}

					if (op->range.nextalt)
					{
						StateStackItem st;
						st.op=opAlternative;
						st.pos=op->range.bracket.nextalt;
						st.savestr=str;
						stack.push_back(st);
					}

					if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
					{
						match[op->range.bracket.index].start=
						    /*match[op->range.bracket.index].end=*/ str - start;
					}

					if (op->op==opBracketMinRange && !op->range.min)
					{
						op=op->range.bracket.pairindex;
					}
					else
					{
						inrangebracket++;
					}

					continue;
				}
				case opLookAhead:
				case opNotLookAhead:
				{
					StateStackItem st;
					st.op=op->op;
					st.savestr=str;
					st.pos=op;
					st.forward=1;
					stack.push_back(st);

					if (op->assert.nextalt)
					{
						StateStackItem Item;
						Item.op=opAlternative;
						Item.pos=op->assert.nextalt;
						Item.savestr=str;
						stack.push_back(Item);
					}

					continue;
				}
				case opLookBehind:
				case opNotLookBehind:
				{
					if (str-op->assert.length<start)
					{
						if (op->op==opLookBehind)break;

						op=op->assert.pairindex;
						continue;
					}

					{
						StateStackItem st;
						st.op = op->op;
						st.savestr = str;
						st.pos = op;
						st.forward = 1;
						str -= op->assert.length;
						stack.push_back(st);
					}

					if (op->assert.nextalt)
					{
						StateStackItem st;
						st.op=opAlternative;
						st.pos=op->assert.nextalt;
						st.savestr=str;
						stack.push_back(st);
					}

					continue;
				}
				case opNoReturn:
				{
					StateStackItem st;
					st.op=opNoReturn;
					stack.push_back(st);
					continue;
				}
				case opRegExpEnd:return 1;
			}//switch(op)
		}

		for (;; stack.pop_back())
		{
			if (stack.empty())
				return 0;
			auto ps = std::prev(stack.end());

			//dpf(("ps->op:%s\n",ops[ps->op]));
			switch (ps->op)
			{
				case opAlternative:
				{
					str=ps->savestr;
					op=ps->pos;

					if (op->alternative.nextalt)
					{
						ps->pos=op->alternative.nextalt;
					}
					else
					{
						stack.pop_back();
					}

					break;
				}
				case opAnyRange:
				case opSymbolRange:
				case opNotSymbolRange:
				case opClassRange:
				case opTypeRange:
				case opNotTypeRange:
				{
					str=ps->savestr-1;
					op=ps->pos;

					if (str<ps->startstr)
					{
						continue;
					}

					ps->savestr=str;
					break;
				}
				case opNamedRefRange:
				case opBackRefRange:
				{
					if (ps->op == opBackRefRange)
					{
						m = &match[ps->pos->range.refindex];
					}
					else
					{
						m = &(*hmatch)[ps->pos->range.refname];
					}
					str=ps->savestr-(m->end-m->start);
					op=ps->pos;

					if (str<ps->startstr)
					{
						continue;
					}

					ps->savestr=str;
					break;
				}
				case opAnyMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (ps->pos->range.op==opCharAny)
					{
						if (str<strend && *str!=0x0a && *str!=0x0d)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}
					else
					{
						if (str<strend)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}

					break;
				}
				case opSymbolMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (ignorecase)
					{
						if (str<strend && TOLOWER(*str)==op->symbol)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}
					else
					{
						if (str<strend && *str==op->symbol)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}

					break;
				}
				case opNotSymbolMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (ignorecase)
					{
						if (str<strend && TOLOWER(*str)!=op->symbol)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}
					else
					{
						if (str<strend && *str!=op->symbol)
						{
							str++;
							ps->savestr=str;
						}
						else
						{
							continue;
						}
					}

					break;
				}
				case opClassMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (str<strend && op->range.symbolclass->GetBit(*str))
					{
						str++;
						ps->savestr=str;
					}
					else
					{
						continue;
					}

					break;
				}
				case opTypeMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (str<strend && isType(*str,op->range.type))
					{
						str++;
						ps->savestr=str;
					}
					else
					{
						continue;
					}

					break;
				}
				case opNotTypeMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;

					if (str<strend && !isType(*str,op->range.type))
					{
						str++;
						ps->savestr=str;
					}
					else
					{
						continue;
					}

					break;
				}
				case opNamedRefMinRange:
				case opBackRefMinRange:
				{
					if (!(ps->max--))continue;

					str=ps->savestr;
					op=ps->pos;
					if (ps->op == opBackRefMinRange)
					{
						m = &match[op->range.refindex];
					}
					else
					{
						m = &(*hmatch)[op->range.refname];
					}

					if (str+m->end-m->start<strend && StrCmp(str,start+m->start,start+m->end))
					{
						ps->savestr=str;
					}
					else
					{
						continue;
					}

					break;
				}
				case opBracketRange:
				{
					if (ps->pos->range.bracket.index>=0 && brhandler)
					{
						if (
						    !brhandler
						    (
						        brhdata,
						        bhRollBack,
						        ps->pos->range.bracket.index,
						        -1,
						        -1
						    )
						)
						{
							return -1;
						}
					}

					if (ps->min)
					{
						inrangebracket--;
						continue;
					}

					if (ps->forward)
					{
						ps->forward=0;
						op=ps->pos->range.bracket.pairindex;
						inrangebracket--;
						str=ps->savestr;

						if (op->range.nextalt)
						{
							StateStackItem st;
							st.op=opAlternative;
							st.pos=op->range.bracket.nextalt;
							st.savestr=str;
							stack.push_back(st);
						}

//            if(op->bracket.index>=0 && op->bracket.index<matchcount)
//            {
//              match[op->bracket.index].end=str-start;
//            }
						break;
					}

					continue;
				}
				case opBracketMinRange:
				{
					if (ps->pos->range.bracket.index>=0 && brhandler)
					{
						if (
						    !brhandler
						    (
						        brhdata,
						        bhRollBack,
						        ps->pos->range.bracket.index,
						        -1,
						        -1
						    )
						)
						{
							return -1;
						}
					}

					if (!(ps->max--))
					{
						inrangebracket--;
						continue;
					}

					if (ps->forward)
					{
						ps->forward=0;
						op=ps->pos;
						str=ps->savestr;

						if (op->range.bracket.index>=0 && op->range.bracket.index<matchcount)
						{
							match[op->range.bracket.index].start = str - start;
							StateStackItem st;
							st.op=opOpenBracket;
							st.pos=op;
							st.min=match[op->range.bracket.index].start;
							st.max=match[op->range.bracket.index].end;
							stack.push_back(st);
						}

						if (op->range.nextalt)
						{
							StateStackItem st;
							st.op=opAlternative;
							st.pos=op->range.bracket.nextalt;
							st.savestr=str;
							stack.push_back(st);
						}

						inrangebracket++;
						break;
					}

					inrangebracket--;
					continue;
				}
				case opOpenBracket:
				{
					j=ps->pos->bracket.index;

					if (j>=0 && j<matchcount)
					{
						if (brhandler)
						{
							if (
							    !brhandler
							    (
							        brhdata,
							        bhRollBack,
							        j,
							        match[j].start,
							        match[j].end
							    )
							)
							{
								return -1;
							}
						}

						match[j].start=ps->min;
						match[j].end=ps->max;
					}

					continue;
				}
				case opNamedBracket:
				{
					auto n = ps->pos->nbracket.name;

					if (n && hmatch)
					{
						RegExpMatch sm;
						sm.start = ps->min;
						sm.end = ps->max;
						(*hmatch)[n] = sm;
					}

					continue;
				}
				case opLookAhead:
				case opLookBehind:
				{
					continue;
				}
				case opNotLookBehind:
				case opNotLookAhead:
				{
					op=ps->pos->assert.pairindex;
					str=ps->savestr;

					if (ps->forward)
					{
						stack.pop_back();
						break;
					}
					else
					{
						continue;
					}
				}
				case opNoReturn:
				{
					return 0;
				}
			}//switch(op)

			break;
		}
	}

	return 1;
}

int RegExp::Match(const wchar_t* textstart, const wchar_t* textend, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	const wchar_t* const start = textstart;
	const wchar_t* tempend=textend;

	if (havefirst && !first[*start])return 0;

	TrimTail(start, tempend);

	if (tempend<start)return 0;

	if (minlength && tempend-start<minlength)return 0;

	std::vector<StateStackItem> stack;

	int res=InnerMatch(start, start, tempend, match, matchcount, hmatch, stack);

	if (res==1)
	{
		for (int i=0; i<matchcount; i++)
		{
			if (match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
			{
				match[i].start=match[i].end=-1;
			}
		}
	}

	return res;
}

int RegExp::MatchEx(const wchar_t* datastart, const wchar_t* textstart, const wchar_t* textend, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	if (havefirst && !first[(wchar_t)*textstart])return 0;

	const wchar_t* tempend=textend;

	const wchar_t* const start = datastart;
	TrimTail(start, tempend);

	if (tempend<textstart)return 0;

	if (minlength && tempend-start<minlength)return 0;

	std::vector<StateStackItem> stack;

	int res = InnerMatch(start, textstart, tempend, match, matchcount, hmatch, stack);

	if (res==1)
	{
		for (int i=0; i<matchcount; i++)
		{
			if (match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
			{
				match[i].start=match[i].end=-1;
			}
		}
	}

	return res;
}

int RegExp::Match(const wchar_t* textstart, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	const wchar_t* textend = textstart + wcslen(textstart);
	return Match(textstart,textend,match,matchcount, hmatch);
}

int RegExp::Optimize()
{
	REOpCode* jumps[MAXDEPTH];
	int jumpcount=0;

	if (havefirst)return 1;

	first.Reset();

	minlength=0;
	int mlstackmin[MAXDEPTH];
	int mlstacksave[MAXDEPTH];
	int mlscnt=0;

	for (const auto* it = code.data(), *end = it + code.size(); it != end; ++it)
	{
		switch (it->op)
		{
			case opType:
			case opNotType:
			case opCharAny:
			case opCharAnyAll:
			case opSymbol:
			case opNotSymbol:
			case opSymbolIgnoreCase:
			case opNotSymbolIgnoreCase:
			case opSymbolClass:
				minlength++;
				continue;
			case opSymbolRange:
			case opSymbolMinRange:
			case opNotSymbolRange:
			case opNotSymbolMinRange:
			case opAnyRange:
			case opAnyMinRange:
			case opTypeRange:
			case opTypeMinRange:
			case opNotTypeRange:
			case opNotTypeMinRange:
			case opClassRange:
			case opClassMinRange:
				minlength+=it->range.min;
				break;
			case opNamedBracket:
			case opOpenBracket:
			case opBracketRange:
			case opBracketMinRange:
				mlstacksave[mlscnt]=minlength;
				mlstackmin[mlscnt++]=-1;
				minlength=0;
				continue;
			case opClosingBracket:
			{
				if (it->bracket.pairindex->op>opAssertionsBegin &&
					it->bracket.pairindex->op<opAsserionsEnd)
				{
					continue;
				}

				if (mlstackmin[mlscnt-1]!=-1 && mlstackmin[mlscnt-1]<minlength)
				{
					minlength=mlstackmin[mlscnt-1];
				}

				switch (it->bracket.pairindex->op)
				{
					case opBracketRange:
					case opBracketMinRange:
						minlength *= it->range.min;
						break;
				}

				minlength+=mlstacksave[--mlscnt];
			} continue;
			case opAlternative:
			{
				if (mlstackmin[mlscnt-1]==-1)
				{
					mlstackmin[mlscnt-1]=minlength;
				}
				else
				{
					if (minlength<mlstackmin[mlscnt-1])
					{
						mlstackmin[mlscnt-1]=minlength;
					}
				}

				minlength=0;
				break;
			}
			case opLookAhead:
			case opNotLookAhead:
			case opLookBehind:
			case opNotLookBehind:
			{
				it = it->assert.pairindex;
				continue;
			}
			case opRegExpEnd:
				it = nullptr;
				break;
		}

		if (!it)break;
	}

	dpf((L"minlength=%d\n",minlength));

	for (const auto* op = code.data(), *end = op + code.size(); op != end; ++op)
	{
		switch (op->op)
		{
			default:
			{
				return 0;
			}
			case opType:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)if (isType(i,op->type))first[i]=1;

				break;
			}
			case opNotType:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)if (!isType(i,op->type))first[i]=1;

				break;
			}
			case opSymbol:
			{
				first[op->symbol]=1;
				break;
			}
			case opSymbolIgnoreCase:
			{
				first[op->symbol]=1;
				first[TOUPPER(op->symbol)]=1;
				break;
			}
			case opSymbolClass:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)
				{
					if (op->symbolclass->GetBit(i))first[i]=1;
				}

				break;
			}
			case opNamedBracket:
			case opOpenBracket:
			{
				if (op->bracket.nextalt)
				{
					jumps[jumpcount++]=op->bracket.nextalt;
				}

				continue;
			}
			case opClosingBracket:
			{
				continue;
			}
			case opAlternative:
			{
				return 0;
			}
			case opSymbolRange:
			case opSymbolMinRange:
			{
				if (ignorecase)
				{
					first[TOLOWER(op->range.symbol)]=1;
					first[TOUPPER(op->range.symbol)]=1;
				}
				else
				{
					first[op->range.symbol]=1;
				}

				if (!op->range.min)continue;

				break;
			}
			case opTypeRange:
			case opTypeMinRange:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)
				{
					if (isType(i,op->range.type))first[i]=1;
				}

				if (!op->range.min)continue;

				break;
			}
			case opNotTypeRange:
			case opNotTypeMinRange:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)
				{
					if (!isType(i,op->range.type))first[i]=1;
				}

				if (!op->range.min)continue;

				break;
			}
			case opClassRange:
			case opClassMinRange:
			{
				for (int i=0; i<RE_CHAR_COUNT; i++)
				{
					if (op->range.symbolclass->GetBit(i))first[i]=1;
				}

				if (!op->range.min)continue;

				break;
			}
			case opBracketRange:
			case opBracketMinRange:
			{
				if (!op->range.min)return 0;

				if (op->range.bracket.nextalt)
				{
					jumps[jumpcount++]=op->range.bracket.nextalt;
				}

				continue;
			}
			//case opLookAhead:
			//case opNotLookAhead:
			//case opLookBehind:
			//case opNotLookBehind:
			case opRegExpEnd:return 0;
		}

		if (jumpcount>0)
		{
			op=jumps[--jumpcount];

			if (op->op==opAlternative && op->alternative.nextalt)
			{
				jumps[jumpcount++]=op->alternative.nextalt;
			}

			continue;
		}

		break;
	}

	havefirst=1;
	return 1;
}

int RegExp::Search(const wchar_t* textstart, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	const wchar_t* textend = textstart + wcslen(textstart);
	return Search(textstart, textend, match, matchcount, hmatch);
}

int RegExp::Search(const wchar_t* textstart, const wchar_t* textend, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	const wchar_t* const start = textstart;
	const wchar_t* str=start;
	const wchar_t* tempend=textend;
	TrimTail(start, tempend);

	if (tempend<start)return 0;

	if (minlength && tempend-start<minlength)return 0;

	int res=0;

	std::vector<StateStackItem> stack;

	if (!code[0].bracket.nextalt && code[1].op == opDataStart)
	{
		res = InnerMatch(start, start, tempend, match, matchcount, hmatch, stack);
	}
	else
	{
		if (!code[0].bracket.nextalt && code[1].op == opDataEnd && code[2].op == opClosingBracket)
		{
			matchcount=1;
			match[0].start = textend - textstart;
			match[0].end=match[0].start;
			return 1;
		}

		if (havefirst)
		{
			do
			{
				while (!first[*str] && str<tempend)str++;

				if (0 != (res = InnerMatch(start, str, tempend, match, matchcount, hmatch, stack)))
				{
					break;
				}

				str++;
			}
			while (str<tempend);

			if (!res && InnerMatch(start, str, tempend, match, matchcount, hmatch, stack))
			{
				res=1;
			}
		}
		else
		{
			do
			{
				if (0 != (res = InnerMatch(start, str, tempend, match, matchcount, hmatch, stack)))
				{
					break;
				}

				str++;
			}
			while (str<=tempend);
		}
	}

	if (res==1)
	{
		for (int i=0; i<matchcount; i++)
		{
			if (match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
			{
				match[i].start=match[i].end=-1;
			}
		}
	}

	return res;
}

int RegExp::SearchEx(const wchar_t* datastart, const wchar_t* textstart, const wchar_t* textend, RegExpMatch* match, intptr_t& matchcount, MatchHash* hmatch) const
{
	const wchar_t* const start = datastart;
	const wchar_t* str=textstart;
	const wchar_t* tempend=textend;
	TrimTail(start, tempend);

	if (tempend<start)return 0;

	if (minlength && tempend-start<minlength)return 0;

	int res=0;

	std::vector<StateStackItem> stack;

	if (!code[0].bracket.nextalt && code[1].op == opDataStart)
	{
		res = InnerMatch(start, str, tempend, match, matchcount, hmatch, stack);
	}
	else
	{
		if (!code[0].bracket.nextalt && code[1].op == opDataEnd && code[2].op == opClosingBracket)
		{
			matchcount=1;
			match[0].start = textend - datastart;
			match[0].end=match[0].start;
			return 1;
		}

		if (havefirst)
		{
			do
			{
				while (!first[*str] && str<tempend)str++;

				if (0 != (res = InnerMatch(start, str, tempend, match, matchcount, hmatch, stack)))
				{
					break;
				}

				str++;
			}
			while (str<tempend);

			if (!res && InnerMatch(start, str, tempend, match, matchcount, hmatch, stack))
			{
				res=1;
			}
		}
		else
		{
			do
			{
				if (0 != (res = InnerMatch(start, str, tempend, match, matchcount, hmatch, stack)))
				{
					break;
				}

				str++;
			}
			while (str<=tempend);
		}
	}

	if (res==1)
	{
		for (int i=0; i<matchcount; i++)
		{
			if (match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
			{
				match[i].start=match[i].end=-1;
			}
		}
	}

	return res;
}

void RegExp::TrimTail(const wchar_t* const start, const wchar_t*& strend) const
{
	if (havelookahead)return;

	if (code.empty() || code[0].bracket.nextalt)return;

	REOpCode* op = code[0].bracket.pairindex - 1;

	while (op->op==opClosingBracket)
	{
		if (op->bracket.pairindex->op!=opOpenBracket)return;

		if (op->bracket.pairindex->bracket.nextalt)return;

		--op;
	}

	strend--;

	switch (op->op)
	{
		case opSymbol:
		{
			while (strend>=start && *strend!=op->symbol)strend--;

			break;
		}
		case opNotSymbol:
		{
			while (strend>=start && *strend==op->symbol)strend--;

			break;
		}
		case opSymbolIgnoreCase:
		{
			while (strend>=start && TOLOWER(*strend)!=op->symbol)strend--;

			break;
		}
		case opNotSymbolIgnoreCase:
		{
			while (strend>=start && TOLOWER(*strend)==op->symbol)strend--;

			break;
		}
		case opType:
		{
			while (strend>=start && !isType(*strend,op->type))strend--;

			break;
		}
		case opNotType:
		{
			while (strend>=start && isType(*strend,op->type))strend--;

			break;
		}
		case opSymbolClass:
		{
			while (strend>=start && !op->symbolclass->GetBit(*strend))strend--;

			break;
		}
		case opSymbolRange:
		case opSymbolMinRange:
		{
			if (!op->range.min)break;

			if (ignorecase)
			{
				while (strend>=start && TOLOWER(*strend)!=op->range.symbol)strend--;
			}
			else
			{
				while (strend>=start && *strend!=op->range.symbol)strend--;
			}

			break;
		}
		case opNotSymbolRange:
		case opNotSymbolMinRange:
		{
			if (!op->range.min)break;

			if (ignorecase)
			{
				while (strend>=start && TOLOWER(*strend)==op->range.symbol)strend--;
			}
			else
			{
				while (strend>=start && *strend==op->range.symbol)strend--;
			}

			break;
		}
		case opTypeRange:
		case opTypeMinRange:
		{
			if (!op->range.min)break;

			while (strend>=start && !isType(*strend,op->range.type))strend--;

			break;
		}
		case opNotTypeRange:
		case opNotTypeMinRange:
		{
			if (!op->range.min)break;

			while (strend>=start && isType(*strend,op->range.type))strend--;

			break;
		}
		case opClassRange:
		case opClassMinRange:
		{
			if (!op->range.min)break;

			while (strend>=start && !op->range.symbolclass->GetBit(*strend))strend--;

			break;
		}
		default:break;
	}

	strend++;
}

#ifdef _DEBUG
void Test()
{
	RegExp re;
	auto Result = re.Compile(L"/a*?ca/");

	assert(Result);

	RegExpMatch m = { -1, -1 };
	intptr_t n = 1;
	Result = re.Search(L"abca", &m, n);

	assert(Result);
	assert(n == 1);
	assert(m.start == 2 && m.end == 4);
}

SELF_TEST(Test());
#endif
