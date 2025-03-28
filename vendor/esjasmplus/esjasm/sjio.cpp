/*

  SjASMPlus Z80 Cross Compiler

  This is modified sources of SjASM by Aprisobal - aprisobal@tut.by

  Copyright (c) 2006 Sjoerd Mastijn

  This software is provided 'as-is', without any express or implied warranty.
  In no event will the authors be held liable for any damages arising from the
  use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it freely,
  subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not claim
	 that you wrote the original software. If you use this software in a product,
	 an acknowledgment in the product documentation would be appreciated but is
	 not required.

  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.

*/

// sjio.cpp

#include "sjdefs.h"
#include <fcntl.h>

#include <string>
std::string *sj_input;
std::string *sj_output;
std::string *sj_errors;

int ListAddress;

static constexpr int DESTBUFLEN = 8192;

// ReadLine buffer and variables around
static char rlbuf[LINEMAX2 * 2];
static char *rlpbuf, *rlpbuf_end, *rlppos;
static bool colonSubline;
static int blockComment;

static char WriteBuffer[DESTBUFLEN];
static FILE *FP_Output = NULL;
static aint WBLength = 0;

void CheckRamLimitExceeded() {
	if (Options::IsLongPtr) return;		// in "longptr" mode with no device keep the address as is
	static bool notWarnedCurAdr = true;
	static bool notWarnedDisp = true;
	char buf[64];
	if (CurAddress >= 0x10000) {
		if (LASTPASS == pass && notWarnedCurAdr) {
			SPRINTF2(buf, 64, "RAM limit exceeded 0x%X by %s",
					 (unsigned int)CurAddress, DISP_NONE != PseudoORG ? "DISP":"ORG");
			Warning(buf);
			notWarnedCurAdr = false;
		}
		if (DISP_NONE != PseudoORG) CurAddress &= 0xFFFF;	// fake DISP address gets auto-wrapped FFFF->0
	} else notWarnedCurAdr = true;
	if (DISP_NONE != PseudoORG && adrdisp >= 0x10000) {
		if (LASTPASS == pass && notWarnedDisp) {
			SPRINTF1(buf, 64, "RAM limit exceeded 0x%X by ORG", (unsigned int)adrdisp);
			Warning(buf);
			notWarnedDisp = false;
		}
	} else notWarnedDisp = true;
}

void resolveRelocationAndSmartSmc(const aint immediateOffset, Relocation::EType minType) {
	// call relocation data generator to do its own errands
	Relocation::resolveRelocationAffected(immediateOffset, minType);
	// check smart-SMC functionality, if there is unresolved record to be set up
	if (INT_MAX == immediateOffset || sourcePosStack.empty() || 0 == smartSmcIndex) return;
	if (smartSmcLines.size() < smartSmcIndex) return;
	auto & smartSmc = smartSmcLines.at(smartSmcIndex - 1);
	if (~0U != smartSmc.colBegin || smartSmc != sourcePosStack.back()) return;
	if (1 < sourcePosStack.back().colBegin) return;		// only first segment belongs to SMC label
	// record does match current line, resolve the smart offset
	smartSmc.colBegin = immediateOffset;
}

void WriteDest() {
	if (!WBLength) {
		return;
	}
	destlen += WBLength;
	// sj_output->append(WriteBuffer, WBLength);
	WBLength = 0;
}

void PrintHex(char* & dest, aint value, int nibbles) {
	if (nibbles < 1 || 8 < nibbles) ExitASM(33);	// invalid argument
	const char oldChAfter = dest[nibbles];
	const aint mask = (int(sizeof(aint)*2) <= nibbles) ? ~0L : (1L<<(nibbles*4))-1L;
	if (nibbles != SPRINTF2(dest, 16, "%0*X", nibbles, value&mask)) ExitASM(33);
	dest += nibbles;
	*dest = oldChAfter;
}

void PrintHex32(char*& dest, aint value) {
	PrintHex(dest, value, 8);
}

void PrintHexAlt(char*& dest, aint value)
{
	char buffer[24] = { 0 }, * bp = buffer;
	SPRINTF1(buffer, 24, "%04X", value);
	while (*bp) *dest++ = *bp++;
}

// static char pline[4*LINEMAX];

// buffer must be at least 4*LINEMAX chars long
void PrepareListLine(char* buffer, aint hexadd)
{
	// ////////////////////////////////////////////////////
	// // Line numbers to 1 to 99999 are supported only  //
	// // For more lines, then first char is incremented //
	// ////////////////////////////////////////////////////

	// int digit = ' ';
	// int linewidth = reglenwidth;
	// uint32_t currentLine = sourcePosStack.at(IncludeLevel).line;
	// aint linenumber = currentLine % 10000;
	// if (5 <= linewidth) {		// five-digit number, calculate the leading "digit"
	// 	linewidth = 5;
	// 	digit = currentLine / 10000 + '0';
	// 	if (digit > '~') digit = '~';
	// 	if (currentLine >= 10000) linenumber += 10000;
	// }
	// memset(buffer, ' ', 24);
	// if (listmacro) buffer[23] = '>';
	// if (Options::LST_T_MC_ONLY == Options::syx.ListingType) buffer[23] = '{';
	// SPRINTF2(buffer, LINEMAX, "%*u", linewidth, linenumber); buffer[linewidth] = ' ';
	// memcpy(buffer + linewidth, "++++++", IncludeLevel > 6 - linewidth ? 6 - linewidth : IncludeLevel);
	// SPRINTF1(buffer + 6, LINEMAX, "%04X", hexadd & 0xFFFF); buffer[10] = ' ';
	// if (digit > '0') *buffer = digit & 0xFF;
	// // if substitutedLine is completely empty, list rather source line any way
	// if (!*substitutedLine) substitutedLine = line;
	// STRCPY(buffer + 24, LINEMAX2-24, substitutedLine);
	// // add EOL comment if substituted was used and EOL comment is available
	// if (substitutedLine != line && eolComment) STRCAT(buffer, LINEMAX2, eolComment);
}

// static void ListFileStringRtrim() {
	// // find end of currently prepared line
	// char* beyondLine = pline+24;
	// while (*beyondLine) ++beyondLine;
	// // and remove trailing white space (space, tab, newline, carriage return, etc..)
	// while (pline < beyondLine && White(beyondLine[-1])) --beyondLine;
	// // set new line and new string terminator after
	// *beyondLine++ = '\n';
	// *beyondLine = 0;
// }

// returns FILE* handle to either actual file defined by --lst=xxx, or stderr if --msg=lst, or NULL
// ! do not fclose this handle, for fclose logic use the FP_ListingFile variable itself !
FILE* GetListingFile() {
	return NULL;
}

// static aint lastListedLine = -1;

void ListFile(bool showAsSkipped) {
	// if (LASTPASS != pass || NULL == GetListingFile() || donotlist || Options::syx.IsListingSuspended) {
	// 	donotlist = nListBytes = 0;
	// 	return;
	// }
	// if (showAsSkipped && Options::LST_T_ACTIVE == Options::syx.ListingType) {
	// 	assert(nListBytes <= 0);	// inactive line should not produce any machine code?!
	// 	nListBytes = 0;
	// 	return;		// filter out all "inactive" lines
	// }
	// if (Options::LST_T_MC_ONLY == Options::syx.ListingType && nListBytes <= 0) {
	// 	return;		// filter out all lines without machine-code bytes
	// }
	// int pos = 0;
	// do {
	// 	if (showAsSkipped) substitutedLine = line;	// override substituted lines in skipped mode
	// 	PrepareListLine(pline, ListAddress);
	// 	const bool hideSource = !showAsSkipped && (lastListedLine == CompiledCurrentLine);
	// 	if (hideSource) pline[24] = 0;				// hide *same* source line on sub-sequent list-lines
	// 	lastListedLine = CompiledCurrentLine;		// remember this line as listed
	// 	char* pp = pline + 10;
	// 	int BtoList = (nListBytes < 4) ? nListBytes : 4;
	// 	for (int i = 0; i < BtoList; ++i) {
	// 		if (-2 == ListEmittedBytes[i + pos]) pp += (memcpy(pp, "...", 3), 3);
	// 		else pp += SPRINTF1(pp, 4, " %02X", ListEmittedBytes[i + pos]);
	// 	}
	// 	*pp = ' ';
	// 	if (showAsSkipped) pline[11] = '~';
	// 	ListFileStringRtrim();
	// 	fputs(pline, GetListingFile());
	// 	nListBytes -= BtoList;
	// 	ListAddress += BtoList;
	// 	pos += BtoList;
	// } while (0 < nListBytes);
	// nListBytes = 0;
	// ListAddress = CurAddress;			// move ListAddress also beyond unlisted but emitted bytes
}

void ListSilentOrExternalEmits() {
	// // catch silent/external emits like "sj.add_byte(0x123)" from Lua script
	// if (0 == nListBytes) return;		// no silent/external emit happened
	// ++CompiledCurrentLine;
	// char silentOrExternalBytes[] = "; these bytes were emitted silently/externally (lua script?)";
	// substitutedLine = silentOrExternalBytes;
	// eolComment = nullptr;
	// ListFile();
	// substitutedLine = line;
}

static bool someByteEmitted = false;

bool DidEmitByte() {	// returns true if some byte was emitted since last call to this function
	bool didEmit = someByteEmitted;		// value to return
	someByteEmitted = false;			// reset the flag
	return didEmit;
}

static void EmitByteNoListing(int byte, bool preserveDeviceMemory = false) {
	someByteEmitted = true;
	if (LASTPASS == pass) {
		WriteBuffer[WBLength++] = (char)byte;
		if (DESTBUFLEN == WBLength) WriteDest();
	}
	// the page-checking in device mode must be done in all passes, the slot can have "wrap" option
	if (DeviceID) {
		Device->CheckPage(CDevice::CHECK_EMIT);
		if (MemoryPointer) {
			if (LASTPASS == pass && !preserveDeviceMemory) *MemoryPointer = (char)byte;
			++MemoryPointer;
		}
	} else {
		CheckRamLimitExceeded();
	}
	++CurAddress;
	if (DISP_NONE != PseudoORG) ++adrdisp;
}

// static bool PageDiffersWarningShown = false;

void EmitByte(int byte, bool isInstructionStart) {
	// if (isInstructionStart) {
	// 	// SLD (Source Level Debugging) tracing-data logging
	// 	if (IsSldExportActive()) {
	// 		int pageNum = Page->Number;
	// 		if (DISP_NONE != PseudoORG) {
	// 			int mappingPageNum = Device->GetPageOfA16(CurAddress);
	// 			if (LABEL_PAGE_UNDEFINED == dispPageNum) {	// special DISP page is not set, use mapped
	// 				pageNum = mappingPageNum;
	// 			} else {
	// 				pageNum = dispPageNum;					// special DISP page is set, use it instead
	// 				if (pageNum != mappingPageNum && !PageDiffersWarningShown) {
	// 					WarningById(W_DISP_MEM_PAGE);
	// 					PageDiffersWarningShown = true;		// show warning about different mapping only once
	// 				}
	// 			}
	// 		}
	// 		WriteToSldFile(pageNum, CurAddress);
	// 	}
	// }
	byte &= 0xFF;
	// if (nListBytes < LIST_EMIT_BYTES_BUFFER_SIZE-1) {
	// 	ListEmittedBytes[nListBytes++] = byte;		// write also into listing
	// } else {
	// 	if (nListBytes < LIST_EMIT_BYTES_BUFFER_SIZE) {
	// 		// too many bytes, show it in listing as "..."
	// 		ListEmittedBytes[nListBytes++] = -2;
	// 	}
	// }
	EmitByteNoListing(byte);
}

void EmitWord(int word, bool isInstructionStart) {
	EmitByte(word % 256, isInstructionStart);
	EmitByte(word / 256, false);
}

void EmitBytes(const int* bytes, bool isInstructionStart) {
	if (BYTES_END_MARKER == *bytes) {
		Error("Illegal instruction", line, IF_FIRST);
		SkipToEol(lp);
	}
	while (BYTES_END_MARKER != *bytes) {
		EmitByte(*bytes++, isInstructionStart);
		isInstructionStart = (INSTRUCTION_START_MARKER == *bytes);	// only true for first byte, or when marker
		if (isInstructionStart) ++bytes;
	}
}

void EmitWords(const int* words, bool isInstructionStart) {
	while (BYTES_END_MARKER != *words) {
		EmitWord(*words++, isInstructionStart);
		isInstructionStart = false;		// only true for first word
	}
}

void EmitBlock(aint byte, aint len, bool preserveDeviceMemory, int emitMaxToListing) {
	if (len <= 0) {
		const aint adrMask = Options::IsLongPtr ? ~0 : 0xFFFF;
		CurAddress = (CurAddress + len) & adrMask;
		if (DISP_NONE != PseudoORG)
			adrdisp = (adrdisp + len) & adrMask;
		if (DeviceID)
			Device->CheckPage(CDevice::CHECK_NO_EMIT);
		else
			CheckRamLimitExceeded();
		return;
	}
	// if (LIST_EMIT_BYTES_BUFFER_SIZE <= nListBytes + emitMaxToListing) {	// clamp emit to list buffer
	// 	emitMaxToListing = LIST_EMIT_BYTES_BUFFER_SIZE - nListBytes;
	// }
	while (len--) {
		// int dVal = (preserveDeviceMemory && DeviceID && MemoryPointer) ? MemoryPointer[0] : byte;
		EmitByteNoListing(byte, preserveDeviceMemory);
		// if (LASTPASS == pass && emitMaxToListing) {
		// 	// put "..." marker into listing if some more bytes are emitted after last listed
		// 	if ((0 == --emitMaxToListing) && len) ListEmittedBytes[nListBytes++] = -2;
		// 	else ListEmittedBytes[nListBytes++] = dVal&0xFF;
		// }
	}
}

// if offset is negative, it functions as "how many bytes from end of file"
// if length is negative, it functions as "how many bytes from end of file to not load"
// void BinIncFile(fullpath_ref_t file, aint offset, aint length) {
// }

// static void OpenDefaultList(fullpath_ref_t inputFile);

// static stdin_log_t::const_iterator stdin_read_it;
// static stdin_log_t* stdin_log = nullptr;

static std::string::const_iterator input_read_it;

void OpenFile(void *nfilename, void* fStdinLog)
{
	// if (++IncludeLevel > 20) {
	// 	Error("Over 20 files nested", NULL, ALL);
	// 	--IncludeLevel;
	// 	return;
	// }

	input_read_it = sj_input->cbegin();

	// assert(!fStdinLog || nfilename.full.empty());

	// if (fStdinLog) {
	// 	FP_Input = stdin;
	// 	stdin_log = fStdinLog;
	// 	stdin_read_it = stdin_log->cbegin();	// reset read iterator (for 2nd+ pass)
	// }
	// else
	// {
	// 	if (!FOPEN_ISOK(FP_Input, nfilename.full, "rb")) {
	// 		Error("opening file", nfilename.str.c_str(), ALL);
	// 		--IncludeLevel;
	// 		return;
	// 	}
	// }

	// fullpath_p_t oFileNameFull = fileNameFull;

	// archive the filename (for referencing it in SLD tracing data or listing/errors)
	// fileNameFull = &nfilename;
	// sourcePosStack.emplace_back(nfilename.str.c_str());
	sourcePosStack.clear();
	sourcePosStack.emplace_back("input");

	// // refresh pre-defined values related to file/include
	// DefineTable.Replace("__INCLUDE_LEVEL__", IncludeLevel);
	// DefineTable.Replace("__FILE__", nfilename.str.c_str());
	// if (0 == IncludeLevel) DefineTable.Replace("__BASE_FILE__", nfilename.str.c_str());

	// open default listing file for each new source file (if default listing is ON) / explicit listing is already opened
	// if (LASTPASS == pass && 0 == IncludeLevel && Options::IsDefaultListingName) OpenDefaultList(nfilename);

	// show in listing file which file was opened
	// FILE* listFile = GetListingFile();
	// if (LASTPASS == pass && listFile) {
	// 	fputs("# file opened: ", listFile);
	// 	fputs(nfilename.str.c_str(), listFile);
	// 	fputs("\n", listFile);
	// }

	rlpbuf = rlpbuf_end = rlbuf;
	colonSubline = false;
	blockComment = 0;

	ReadBufLine();

	// if (stdin != FP_Input)
	// 	fclose(FP_Input);
	// else
	// {
	// 	if (1 == pass)
	// 	{
	// 		stdin_log->push_back(0); // add extra zero terminator
	// 		clearerr(stdin);	 // reset EOF on the stdin for another round of input
	// 	}
	// }

	// if (1 == pass)
	// {
	// 	sj_input->push_back(0);
	// }

	// show in listing file which file was closed
	// if (LASTPASS == pass && listFile) {
	// 	fputs("# file closed: ", listFile);
	// 	fputs(nfilename.str.c_str(), listFile);
	// 	fputs("\n", listFile);

	// 	// close listing file (if "default" listing filename is used)
	// 	if (FP_ListingFile && 0 == IncludeLevel && Options::IsDefaultListingName) {
	// 		if (Options::AddLabelListing) LabelTable.Dump();
	// 		fclose(FP_ListingFile);
	// 		FP_ListingFile = NULL;
	// 	}
	// }

	--IncludeLevel;

	maxlin = std::max(maxlin, sourcePosStack.back().line);
	sourcePosStack.pop_back();
	// fileNameFull = oFileNameFull;

	// refresh pre-defined values related to file/include
	// DefineTable.Replace("__INCLUDE_LEVEL__", IncludeLevel);
	// DefineTable.Replace("__FILE__", fileNameFull ? fileNameFull->str.c_str() : "<none>");
	// if (-1 == IncludeLevel)
	// 	DefineTable.Replace("__BASE_FILE__", "<none>");
}

void IncludeFile(fullpath_ref_t nfilename)
{
	// auto oStdin_log = stdin_log;
	// auto oStdin_read_it = stdin_read_it;
	// FILE* oFP_Input = FP_Input;
	// FP_Input = 0;

	// char* pbuf = rlpbuf, * pbuf_end = rlpbuf_end, * buf = STRDUP(rlbuf);
	// if (buf == NULL) ErrorOOM();
	// bool oColonSubline = colonSubline;
	// if (blockComment) Error("Internal error 'block comment'", NULL, FATAL);	// comment can't INCLUDE

	// OpenFile(nfilename);

	// colonSubline = oColonSubline;
	// rlpbuf = pbuf, rlpbuf_end = pbuf_end;
	// STRCPY(rlbuf, 8192, buf);
	// free(buf);

	// FP_Input = oFP_Input;
	// stdin_log = oStdin_log;
	// stdin_read_it = oStdin_read_it;
}

// typedef struct {
// 	char	name[12];
// 	size_t	length;
// 	byte	marker[16];
// } BOMmarkerDef;

// const BOMmarkerDef UtfBomMarkers[] = {
// 	{ { "UTF8" }, 3, { 0xEF, 0xBB, 0xBF } },
// 	{ { "UTF32BE" }, 4, { 0, 0, 0xFE, 0xFF } },
// 	{ { "UTF32LE" }, 4, { 0xFF, 0xFE, 0, 0 } },		// must be detected *BEFORE* UTF16LE
// 	{ { "UTF16BE" }, 2, { 0xFE, 0xFF } },
// 	{ { "UTF16LE" }, 2, { 0xFF, 0xFE } }
// };

static bool ReadBufData()
{
	// check here also if `line` buffer is not full
	if ((LINEMAX-2) <= (rlppos - line)) Error("Line too long", NULL, FATAL);

	// now check for read data
	if (rlpbuf < rlpbuf_end) return 1;		// some data still in buffer

	// // check EOF on files in every pass, stdin only in first, following will starve the stdin_log
	// if ((stdin != FP_Input || 1 == pass) && feof(FP_Input)) return 0;	// no more data in file

	// read next block of data
	rlpbuf = rlbuf;

	// handle STDIN file differently (pass1 = read it, pass2+ replay "log" variable)
	// if (1 == pass || stdin != FP_Input) {	// ordinary file is re-read every pass normally
	// 	rlpbuf_end = rlbuf + fread(rlbuf, 1, 4096, FP_Input);
	// 	*rlpbuf_end = 0;					// add zero terminator after new block
	// }

	// if (stdin == FP_Input) {
	// 	// store copy of stdin into stdin_log during pass 1
	// 	if (1 == pass && rlpbuf < rlpbuf_end) {
	// 		stdin_log->insert(stdin_log->end(), rlpbuf, rlpbuf_end);
	// 	}
	// 	// replay the log in 2nd+ pass
	// 	if (1 < pass) {
	// 		rlpbuf_end = rlpbuf;
	// 		long toCopy = std::min(8000L, (long)std::distance(stdin_read_it, stdin_log->cend()));
	// 		if (0 < toCopy) {
	// 			memcpy(rlbuf, &(*stdin_read_it), toCopy);
	// 			stdin_read_it += toCopy;
	// 			rlpbuf_end += toCopy;
	// 		}
	// 		*rlpbuf_end = 0;				// add zero terminator after new block
	// 	}
	// }

	rlpbuf_end = rlpbuf;
	long toCopy = std::min(8000L, (long)std::distance(input_read_it, sj_input->cend()));
	if (toCopy > 0)
	{
		memcpy(rlbuf, &(*input_read_it), toCopy);
		input_read_it += toCopy;
		rlpbuf_end += toCopy;
	}
	*rlpbuf_end = 0; // add zero terminator after new block

	return (rlpbuf < rlpbuf_end);			// return true if some data were read
}

void ReadBufLine(bool Parse, bool SplitByColon) {
	// if everything else fails (no data, not running, etc), return empty line
	*line = 0;
	bool IsLabel = true;

	// try to read through the buffer and produce new line from it
	while (IsRunning && ReadBufData()) {
		// start of new line (or fake "line" by colon)
		rlppos = line;
		substitutedLine = line;		// also reset "substituted" line to the raw new one
		eolComment = NULL;

		if (colonSubline) {			// starting from colon (creating new fake "line")
			colonSubline = false;	// (can't happen inside block comment)
			*(rlppos++) = ' ';
			IsLabel = false;
		} else {					// starting real new line
			IsLabel = (0 == blockComment);
		}

		bool afterNonAlphaNum, afterNonAlphaNumNext = true;

		// copy data from read buffer into `line` buffer until EOL/colon is found
		while (
		    ReadBufData() && '\n' != *rlpbuf && '\r' != *rlpbuf && // split by EOL
		    // split by colon only on 2nd+ char && SplitByColon && not inside block comment
		    (blockComment || !SplitByColon || rlppos == line || ':' != *rlpbuf))
		{
			// copy the new character to new line
			*rlppos = *rlpbuf++;
			afterNonAlphaNum = afterNonAlphaNumNext;
			afterNonAlphaNumNext = !isalnum((byte)*rlppos);

			// handle EOL escaping, limited implementation, usage not recommended
			if ('\\' == *rlppos && ReadBufData() && ('\r' == *rlpbuf || '\n' == *rlpbuf))  {
				char CRLFtest = (*rlpbuf++) ^ ('\r'^'\n');	// flip CR->LF || LF->CR (and eats first)
				if (ReadBufData() && CRLFtest == *rlpbuf) ++rlpbuf;	// if CRLF/LFCR pair, eat also second
				sourcePosStack.back().nextSegment();	// mark last line in errors/etc
				continue;								// continue with chars from next line
			}

			// Block comments logic first (anything serious may happen only "outside" of block comment
			if ('*' == *rlppos && ReadBufData() && '/' == *rlpbuf) {
				if (0 < blockComment) --blockComment;	// block comment ends here, -1 from nesting
				++rlppos;	*rlppos++ = *rlpbuf++;		// copy the second char too
				continue;
			}

			if ('/' == *rlppos && ReadBufData() && '*' == *rlpbuf) {
				++rlppos, ++blockComment;				// block comment starts here, nest +1 more
				*rlppos++ = *rlpbuf++;					// copy the second char too
				continue;
			}

			if (blockComment) {							// inside block comment just copy chars
				++rlppos;
				continue;
			}

			// check if still in label area, if yes, copy the finishing colon as char (don't split by it)
			if ((IsLabel = (IsLabel && islabchar(*rlppos)))) {
				++rlppos;					// label character
				//SMC offset handling
				if (ReadBufData() && '+' == *rlpbuf) {	// '+' after label, add it as SMC_offset syntax
					IsLabel = false;
					*rlppos++ = *rlpbuf++;
					if (ReadBufData() && (isdigit(byte(*rlpbuf)) || '*' == *rlpbuf)) *rlppos++ = *rlpbuf++;
				}

				if (ReadBufData() && ':' == *rlpbuf) {	// colon after label, add it
					*rlppos++ = *rlpbuf++;
					IsLabel = false;
				}

				continue;
			}

			// not in label any more, check for EOL comments ";" or "//"
			if ((';' == *rlppos) || ('/' == *rlppos && ReadBufData() && '/' == *rlpbuf)) {
				eolComment = rlppos;
				++rlppos;					// EOL comment ";"
				while (ReadBufData() && '\n' != *rlpbuf && '\r' != *rlpbuf) *rlppos++ = *rlpbuf++;

				continue;
			}

			// check for string literals - double/single quotes
			if (afterNonAlphaNum && ('"' == *rlppos || '\'' == *rlppos)) {
				const bool quotes = '"' == *rlppos;
				int escaped = 0;
				do {
					if (escaped) --escaped;
					++rlppos;				// previous char confirmed
					*rlppos = ReadBufData() ? *rlpbuf : 0;	// copy next char (if available)
					if (!*rlppos || '\r' == *rlppos || '\n' == *rlppos) *rlppos = 0;	// not valid
					else ++rlpbuf;			// buffer char read (accepted)
					if (quotes && !escaped && '\\' == *rlppos) escaped = 2;	// escape sequence detected
				} while (*rlppos && (escaped || (quotes ? '"' : '\'') != *rlppos));
				if (*rlppos) ++rlppos;		// there should be ending "/' in line buffer, confirm it

				continue;
			}

			// anything else just copy
			++rlppos;				// previous char confirmed
		} // while "some char in buffer, and it's not line delimiter"

		// line interrupted somehow, may be correctly finished, check + finalize line and process it
		*rlppos = 0;
		// skip <EOL> char sequence in read buffer
		if (ReadBufData() && ('\r' == *rlpbuf || '\n' == *rlpbuf)) {
			char CRLFtest = (*rlpbuf++) ^ ('\r'^'\n');	// flip CR->LF || LF->CR (and eats first)
			if (ReadBufData() && CRLFtest == *rlpbuf) ++rlpbuf;	// if CRLF/LFCR pair, eat also second
			// if this was very last <EOL> in file (on non-empty line), add one more fake empty line
			if (!ReadBufData() && *line) *rlpbuf_end++ = '\n';	// to make listing files "as before"
		} else {
			// advance over single colon if that was the reason to terminate line parsing
			colonSubline = SplitByColon && ReadBufData() && (':' == *rlpbuf) && ++rlpbuf;
		}

		// do +1 for very first colon-segment only (rest is +1 due to artificial space at beginning)
		assert(!sourcePosStack.empty());
		size_t advanceColumns = colonSubline ? (0 == sourcePosStack.back().colEnd) + strlen(line) : 0;
		sourcePosStack.back().nextSegment(colonSubline, advanceColumns);

		// line is parsed and ready to be processed
		if (Parse) 	ParseLine();	// processed here in loop
		else 		return;			// processed externally
	} // while (IsRunning && ReadBufData())
}

// static void OpenListImp(const std::filesystem::path & listFilename) {
	// if STDERR is configured to contain listing, disable other listing files
	// if (OV_LST == Options::OutputVerbosity) return;
	// if (listFilename.empty()) return;
	// if (!FOPEN_ISOK(FP_ListingFile, listFilename, "w")) {
	// 	Error("opening file for write", listFilename.string().c_str(), FATAL);
	// }
// }

void OpenList() {
	// // if STDERR is configured to contain listing, disable other listing files
	// if (OV_LST == Options::OutputVerbosity) return;
	// // check if listing file is already opened, or it is set to "default" file names
	// if (Options::IsDefaultListingName || NULL != FP_ListingFile) return;
	// // Only explicit listing files are opened here
	// OpenListImp(Options::ListingFName);
}

// static void OpenDefaultList(fullpath_ref_t inputFile) {
	// // if STDERR is configured to contain listing, disable other listing files
	// if (OV_LST == Options::OutputVerbosity) return;
	// // check if listing file is already opened, or it is set to explicit file name
	// if (!Options::IsDefaultListingName || NULL != FP_ListingFile) return;
	// if (inputFile.full.empty()) return;		// no filename provided
	// // Create default listing name, and try to open it
	// std::filesystem::path listName { inputFile.full };
	// listName.replace_extension("lst");
	// OpenListImp(listName);
// }

void CloseDest() {
	// Flush buffer before any other operations
	WriteDest();
	// does main output file exist? (to close it)
	if (FP_Output == NULL) return;
	// pad to desired size (and check for exceed of it)
	if (size != -1L) {
		if (destlen > size) {
			ErrorInt("File exceeds 'size' by", destlen - size);
		}
		memset(WriteBuffer, 0, DESTBUFLEN);
		while (destlen < size) {
			WBLength = std::min(aint(DESTBUFLEN), size-destlen);
			WriteDest();
		}
		size = -1L;
	}
	fclose(FP_Output);
	FP_Output = NULL;
}

void SeekDest(long offset, int method) {
	// WriteDest();
	// if (FP_Output != NULL && fseek(FP_Output, offset, method)) {
	// 	Error("File seek error (FPOS)", NULL, FATAL);
	// }
}

void NewDest(const std::filesystem::path &newfilename, int mode) {
	// close previous output file
	CloseDest();

	// and open new file (keep previous/default name, if no explicit was provided)
	// if (!newfilename.empty()) Options::DestinationFName = newfilename;
	OpenDest(mode);
}

void OpenDest(int mode) {
	destlen = 0;
	mode = OUTPUT_TRUNCATE;
	Options::NoDestinationFile = false;
	sj_output->clear();
}

// check if file exists and can be read for content
bool FileExists(const std::filesystem::path & file_name) {
        return 0;
}

bool FileExistsCstr(const char* file_name) {
	return false;
}

void Close() {
	if (*ModuleName) {
		Warning("ENDMODULE missing for module", ModuleName, W_ALL);
	}

	CloseDest();
}

int SaveRAM(std::string &out, int start, int length) {
	//unsigned int addadr = 0,save = 0;
	aint save = 0;
	if (!DeviceID) return 0;		// unreachable currently
	if (length + start > 0x10000) {
		length = -1;
	}
	if (length <= 0) {
		length = 0x10000 - start;
	}

	CDeviceSlot* S;
	out.clear();
	for (int i = 0; i < Device->SlotsCount; i++)
	{
		S = Device->GetSlot(i);
		if (start >= (int)S->Address && start < (int)(S->Address + S->Size))
		{
			if (length < (int)(S->Size - (start - S->Address))) {
				save = length;
			} else {
				save = S->Size - (start - S->Address);
			}
			// if ((aint) fwrite(S->Page->RAM + (start - S->Address), 1, save, ff) != save) {
			// 	return 0;
			// }
			out.append((char *)(S->Page->RAM + (start - S->Address)), save);
			length -= save;
			start += save;
			if (length <= 0) {
				return 1;
			}
		}
	}
	return 0;		// unreachable (with current devices)
}

unsigned int MemGetWord(unsigned int address) {
	return MemGetByte(address) + (MemGetByte(address+1)<<8);
}

unsigned char MemGetByte(unsigned int address) {
	if (!DeviceID || pass != LASTPASS) {
		return 0;
	}

	CDeviceSlot* S;
	for (int i=0;i<Device->SlotsCount;i++) {
		S = Device->GetSlot(i);
		if (address >= (unsigned int)S->Address  && address < (unsigned int)S->Address + (unsigned int)S->Size) {
			return S->Page->RAM[address - S->Address];
		}
	}

	ErrorInt("MemGetByte: Error reading address", address);
	return 0;
}

EReturn ReadFile() {
	while (ReadLine()) {
		const bool isInsideDupCollectingLines = !RepeatStack.empty() && !RepeatStack.top().IsInWork;
		if (!isInsideDupCollectingLines) {
			// check for ending of IF/IFN/... block (keywords: ENDIF, ELSE and ELSEIF)
			char* p = line;
			SkipBlanks(p);
			if ('.' == *p) ++p;
			EReturn retVal = END;
			if (cmphstr(p, "elseif")) retVal = ELSEIF;
			if (cmphstr(p, "else")) retVal = ELSE;
			if (cmphstr(p, "endif")) retVal = ENDIF;
			if (END != retVal) {
				// one of the end-block keywords was found, don't parse it as regular line
				// but just substitute the rest of it and return end value of the keyword
				++CompiledCurrentLine;
				lp = ReplaceDefine(p);		// skip any empty substitutions and comments
				substitutedLine = line;		// for listing override substituted line with source
				if (ENDIF != retVal) ListFile();	// do the listing for ELSE and ELSEIF
				return retVal;
			}
		}
		ParseLineSafe();
	}
	return END;
}


EReturn SkipFile() {
	int iflevel = 0;
	while (ReadLine()) {
		char* p = line;
		if (isLabelStart(p) && !Options::syx.IsPseudoOpBOF) {
			// this could be label, skip it (the --dirbol users can't use label + IF/... inside block)
			while (islabchar(*p)) ++p;
			if (':' == *p) ++p;
		}
		SkipBlanks(p);
		if ('.' == *p) ++p;
		if (cmphstr(p, "if") || cmphstr(p, "ifn") || cmphstr(p, "ifused") ||
			cmphstr(p, "ifnused") || cmphstr(p, "ifdef") || cmphstr(p, "ifndef")) {
			++iflevel;
		} else if (cmphstr(p, "endif")) {
			if (iflevel) {
				--iflevel;
			} else {
				++CompiledCurrentLine;
				lp = ReplaceDefine(p);		// skip any empty substitutions and comments
				substitutedLine = line;		// override substituted listing for ENDIF
				return ENDIF;
			}
		} else if (cmphstr(p, "else")) {
			if (!iflevel) {
				++CompiledCurrentLine;
				lp = ReplaceDefine(p);		// skip any empty substitutions and comments
				substitutedLine = line;		// override substituted listing for ELSE
				ListFile();
				return ELSE;
			}
		} else if (cmphstr(p, "elseif")) {
			if (!iflevel) {
				++CompiledCurrentLine;
				lp = ReplaceDefine(p);		// skip any empty substitutions and comments
				substitutedLine = line;		// override substituted listing for ELSEIF
				ListFile();
				return ELSEIF;
			}
		} else if (cmphstr(p, "lua")) {		// lua script block detected, skip it whole
			// with extra custom while loop, to avoid confusion by `if/...` inside lua scripts
			ListFile(true);
			while (ReadLine()) {
				p = line;
				SkipBlanks(p);
				if (cmphstr(p, "endlua")) break;
				ListFile(true);
			}
		}
		ListFile(true);
	}
	return END;
}

int ReadLineNoMacro(bool SplitByColon) {
	if (!IsRunning || !ReadBufData()) return 0;
	ReadBufLine(false, SplitByColon);
	return 1;
}

int ReadLine(bool SplitByColon) {
	if (IsRunning && lijst) {		// read MACRO lines, if macro is being emitted
		if (!lijstp || !lijstp->string) return 0;
		assert(!sourcePosStack.empty());
		sourcePosStack.back() = lijstp->source;
		STRCPY(line, LINEMAX, lijstp->string);
		substitutedLine = line;		// reset substituted listing
		eolComment = NULL;			// reset end of line comment
		lijstp = lijstp->next;
		return 1;
	}
	return ReadLineNoMacro(SplitByColon);
}

int ReadFileToCStringsList(CStringsList*& f, const char* end) {
	// f itself should be already NULL, not resetting it here
	CStringsList** s = &f;
	bool SplitByColon = true;
	while (ReadLineNoMacro(SplitByColon)) {
		++CompiledCurrentLine;
		char* p = line;
		SkipBlanks(p);
		if ('.' == *p) ++p;
		if (cmphstr(p, end)) {		// finished, read rest after end marker into line buffers
			lp = ReplaceDefine(p);
			return 1;
		}
		*s = new CStringsList(line);
		s = &((*s)->next);
		ListFile(true);
		// Try to ignore colons inside lua blocks... this is far from bulletproof, but should improve it
		if (SplitByColon && cmphstr(p, "lua")) {
			SplitByColon = false;
		} else if (!SplitByColon && cmphstr(p, "endlua")) {
			SplitByColon = true;
		}
	}
	return 0;
}
