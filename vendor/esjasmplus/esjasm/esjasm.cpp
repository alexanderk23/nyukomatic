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

#include <cstdlib>
#include <chrono>
#include <ctime>
#include <stdexcept>

#include "sjdefs.h"
#include "esjasm.hpp"


namespace Options {
	const STerminalColorSequences tcols_ansi = {
		/*end*/		"\033[m",
		/*display*/	"\033[36m",		// Cyan
		/*warning*/	"\033[33m",		// Yellow
		/*error*/	"\033[31m",		// Red
		/*bold*/	"\033[1m"		// bold
	};

	const STerminalColorSequences tcols_none = {
		/*end*/		"",
		/*display*/	"",
		/*warning*/	"",
		/*error*/	"",
		/*bold*/	""
	};

	const STerminalColorSequences* tcols = &tcols_none;

	void SetTerminalColors(bool enabled) {
		tcols = enabled ? &tcols_ansi : &tcols_none;
	}

	std::filesystem::path OutPrefix {""};
	std::filesystem::path SymbolListFName {""};
	std::filesystem::path ListingFName {""};
	std::filesystem::path ExportFName {""};

	std::filesystem::path RAWFName {"-"};
	std::filesystem::path UnrealLabelListFName {""};

	std::filesystem::path SourceLevelDebugFName {""};
	bool IsDefaultSldName = false;

	EOutputVerbosity OutputVerbosity = OV_WARNING; // OV_ALL;
	bool IsLabelTableInListing = false;
	bool IsDefaultListingName = false;
	EFileVerbosity FileVerbosity = FNAME_BASE;
	bool AddLabelListing = false;
	bool HideLogo = false;
	bool ShowHelp = false;
	bool ShowHelpWarnings = false;
	bool ShowVersion = false;
	bool NoDestinationFile = true;		// no *.out file by default
	SSyntax syx, systemSyntax;
	bool IsI8080 = false;
	bool IsLR35902 = false;
	bool IsLongPtr = false;
	bool SortSymbols = false;
	bool IsBigEndian = false;
	bool EmitVirtualLabels = false;

	CDefineTable CmdDefineTable;		// is initialized by constructor

	static const char* fakes_disabled_txt_error = "Fake instructions are not enabled";
	static const char* fakes_in_i8080_txt_error = "Fake instructions are not implemented in i8080 mode";
	static const char* fakes_in_lr35902_txt_error = "Fake instructions are not implemented in Sharp LR35902 mode";

	// returns true if fakes are completely disabled, false when they are enabled
	// showMessage=true: will also display error/warning (use when fake ins. is emitted)
	// showMessage=false: can be used to silently check if fake instructions are even possible
	bool noFakes(bool showMessage) {
		bool fakesDisabled = Options::IsI8080 || Options::IsLR35902 || (!syx.FakeEnabled);
		if (!showMessage) return fakesDisabled;
		if (fakesDisabled) {
			const char* errorTxt = fakes_disabled_txt_error;
			if (Options::IsI8080) errorTxt = fakes_in_i8080_txt_error;
			if (Options::IsLR35902) errorTxt = fakes_in_lr35902_txt_error;
			Error(errorTxt, bp, SUPPRESS);
			return true;
		}
		if (syx.FakeWarning) {
			WarningById(W_FAKE, bp);
		}
		return false;
	}

	std::stack<SSyntax> SSyntax::syxStack;

	void SSyntax::resetCurrentSyntax() {
		new (&syx) SSyntax();	// restore defaults in current syntax
	}

	void SSyntax::pushCurrentSyntax() {
		syxStack.push(syx);		// store current syntax options into stack
	}

	bool SSyntax::popSyntax() {
		if (syxStack.empty()) return false;	// no syntax stored in stack
		syx = syxStack.top();	// copy the syntax values from stack
		syxStack.pop();
		return true;
	}

	void SSyntax::restoreSystemSyntax() {
		while (!syxStack.empty()) syxStack.pop();	// empty the syntax stack first
		syx = systemSyntax;		// reset to original system syntax
	}

} // eof namespace Options


CDevice *Devices = nullptr;
CDevice *Device = nullptr;
CDevicePage *Page = nullptr;
char* DeviceID = nullptr;
TextFilePos globalDeviceSourcePos;
aint deviceDirectivesCount = 0;
static char* globalDeviceID = nullptr;
static aint globalDeviceZxRamTop = 0;

char *lp, line[LINEMAX], temp[LINEMAX], *bp;
char sline[LINEMAX2], sline2[LINEMAX2], *substitutedLine, *eolComment, ModuleName[LINEMAX];

int ConvertEncoding = ENCWIN;
EDispMode PseudoORG = DISP_NONE;
bool IsLabelNotFound = false, IsSubstituting = false;
int pass, ErrorCount, WarningCount, IncludeLevel;
int IsRunning = 0, donotlist = 0, listmacro = 0;
int adrdisp = 0, dispPageNum = LABEL_PAGE_UNDEFINED, StartAddress = -1;
byte* MemoryPointer=NULL;
int macronummer = 0, lijst = 0, reglenwidth = 0;
source_positions_t sourcePosStack;
source_positions_t smartSmcLines;
source_positions_t::size_type smartSmcIndex;
uint32_t maxlin = 0;
aint CurAddress = 0, CompiledCurrentLine = 0, LastParsedLabelLine = 0, PredefinedCounter = 0;
aint destlen = 0, size = -1L, comlin = 0;

char *vorlabp = NULL, *macrolabp = NULL, *LastParsedLabel = NULL;
std::stack<SRepeatStack> RepeatStack;
CStringsList* lijstp = NULL;
CLabelTable LabelTable;
CTemporaryLabelTable TemporaryLabelTable;
CDefineTable DefineTable;
CMacroDefineTable MacroDefineTable;
CMacroTable MacroTable;
CStructureTable StructureTable;

// reserve keywords in labels table, to detect when user is defining label colliding with keyword
static void ReserveLabelKeywords() {
	for (const char* keyword : {
		"abs", "and", "exist", "high", "low", "mod", "norel", "not", "or", "shl", "shr", "xor"
	}) {
		LabelTable.Insert(keyword, -65536, LABEL_IS_UNDEFINED|LABEL_IS_KEYWORD);
	}
}

int InitPass() {
	assert(sourcePosStack.empty());				// there's no source position [left] in the stack
	Relocation::InitPass();
	Options::SSyntax::restoreSystemSyntax();	// release all stored syntax variants and reset to initial
	uint32_t maxpow10 = 1;
	reglenwidth = 0;

        do {
            ++reglenwidth;
            maxpow10 *= 10;
            if (maxpow10 < 10)
                ExitASM(1); // 32b overflow
        } while (maxpow10 <= maxlin);

        *ModuleName = 0;
	SetLastParsedLabel(nullptr);

        if (vorlabp)
            free(vorlabp);
        vorlabp = STRDUP("_");

	macrolabp = NULL;
        listmacro = 0;
        CurAddress = 0;
        CompiledCurrentLine = 0;
        smartSmcIndex = 0;
        PseudoORG = DISP_NONE;
        adrdisp = 0;
        dispPageNum = LABEL_PAGE_UNDEFINED;
        ListAddress = 0;
        macronummer = 0;
        lijst = 0;
        comlin = 0;
        lijstp = NULL;

        DidEmitByte();				// reset the emitted flag

	StructureTable.ReInit();
	MacroTable.ReInit();
	MacroDefineTable.ReInit();
	DefineTable = Options::CmdDefineTable;
	TemporaryLabelTable.InitPass();

	// reset "device" stuff + detect "global device" directive
	if (globalDeviceID) {		// globalDeviceID detector has to trigger before every pass
		free(globalDeviceID);
		globalDeviceID = nullptr;
	}

	if (1 < pass && 1 == deviceDirectivesCount && Devices) {	// only single DEVICE used
		globalDeviceID = STRDUP(Devices->ID);		// make it global for next pass
		globalDeviceZxRamTop = Devices->ZxRamTop;
	}

	if (Devices) delete Devices;
	Devices = Device = nullptr;

	DeviceID = nullptr;
	Page = nullptr;
	deviceDirectivesCount = 0;

	// resurrect "global" device here
	if (globalDeviceID) {
		sourcePosStack.push_back(globalDeviceSourcePos);
		if (!SetDevice(globalDeviceID, globalDeviceZxRamTop)) {		// manually tested (remove "!")
			Error("Failed to re-initialize global device", globalDeviceID, FATAL);
		}
		sourcePosStack.pop_back();
	}

	// predefined defines - (deprecated) classic sjasmplus v1.x (till v1.15.1)
	DefineTable.Replace("_SJASMPLUS", "1");
	DefineTable.Replace("_RELEASE", "0");
	DefineTable.Replace("_VERSION", "__VERSION__");
	DefineTable.Replace("_ERRORS", "__ERRORS__");
	DefineTable.Replace("_WARNINGS", "__WARNINGS__");
	// predefined defines - sjasmplus v2.x-like (since v1.16.0)
	// __DATE__ and __TIME__ are defined just once in main(...) (stored in Options::CmdDefineTable)
	DefineTable.Replace("__SJASMPLUS__", VERSION_NUM);		// modified from _SJASMPLUS
	DefineTable.Replace("__VERSION__", "\"" VERSION "\"");	// migrated from _VERSION
	DefineTable.Replace("__ERRORS__", ErrorCount);			// migrated from _ERRORS (can be already > 0 from earlier pass)
	DefineTable.Replace("__WARNINGS__", WarningCount);		// migrated from _WARNINGS (can be already > 0 from earlier pass)
	DefineTable.Replace("__PASS__", pass);					// current pass of assembler
	DefineTable.Replace("__INCLUDE_LEVEL__", "-1");			// include nesting
	DefineTable.Replace("__BASE_FILE__", "<none>");			// the include-level 0 file
	DefineTable.Replace("__FILE__", "<none>");				// current file
	DefineTable.Replace("__LINE__", "<dynamic value>");		// current line in current file
	DefineTable.Replace("__COUNTER__", "<dynamic value>");	// gcc-like, incremented upon every use
	PredefinedCounter = 0;

	return 0;
}

void FreeRAM() {
	if (Devices) {
		delete Devices;
		Devices = nullptr;
	}

	if (globalDeviceID) {
		free(globalDeviceID);
		globalDeviceID = nullptr;
	}

	for (CDeviceDef* deviceDef : DefDevices) delete deviceDef;
	DefDevices.clear();

	lijstp = NULL;		// do not delete this, should be released by owners of DUP/regular macros

	free(vorlabp);
	vorlabp = NULL;

	LabelTable.RemoveAll();
	DefineTable.RemoveAll();
	TemporaryLabelTable.RemoveAll();

	SetLastParsedLabel(nullptr);

	if (PreviousIsLabel) {
		free(PreviousIsLabel);
		PreviousIsLabel = nullptr;
	}
}

void ExitASM(int p) {
	FreeRAM();
	if (pass == LASTPASS)
		Close();
	throw std::runtime_error("Runtime error");
}

int assemble(std::string &input, std::string &output, std::string &errors) {
	// start counter
	// long dwStart = GetTickCount();

	sourcePosStack.clear();
	sourcePosStack.reserve(32);
	smartSmcLines.clear();
	smartSmcLines.reserve(64);

	Devices = nullptr;
	Device = nullptr;
	Page = nullptr;
	DeviceID = nullptr;
	deviceDirectivesCount = 0;
	globalDeviceID = nullptr;
	globalDeviceZxRamTop = 0;
	IsRunning = 0;
	pass = 0;
	ErrorCount = 0;
	WarningCount = 0;
	IncludeLevel = -1;
	donotlist = 0;
	listmacro = 0;
	adrdisp = 0;
	dispPageNum = LABEL_PAGE_UNDEFINED;
	StartAddress = -1;
	MemoryPointer = NULL;
	macronummer = 0;
	lijst = 0;
	reglenwidth = 0;
	maxlin = 0;
	CurAddress = 0;
	CompiledCurrentLine = 0;
	LastParsedLabelLine = 0;
	PredefinedCounter = 0;
	destlen = 0;
	size = -1L;
	comlin = 0;
	sourcePosStack.clear();
	ConvertEncoding = ENCWIN;
	PseudoORG = DISP_NONE;
	IsLabelNotFound = false;
	IsSubstituting = false;
	// RepeatStack?
	lijstp = NULL;
	LabelTable.RemoveAll();
	TemporaryLabelTable.RemoveAll();
	DefineTable.RemoveAll();
	MacroDefineTable.ReInit();
	MacroTable.ReInit();
	StructureTable.ReInit();

	// init some vars
	static int initialized = 0;
	if (!initialized)
	{
		const char *logo = "SjASMPlus Z80 Cross-Assembler v" VERSION " (https://github.com/z00m128/sjasmplus)";
		_CERR logo _ENDL;

		const word little_endian_test[] = {0x1234};
		const byte le_test_byte = *reinterpret_cast<const byte *>(little_endian_test);
		Options::IsBigEndian = (0x12 == le_test_byte);
                // if (Options::IsBigEndian) WarningById(W_BE_HOST, nullptr, W_EARLY);

                InitCPU();
		ReserveLabelKeywords();
		initialized = 1;
	}

	// setup __DATE__ and __TIME__ macros (setup them just once, not every pass!)
	// auto now = std::chrono::system_clock::now();
	// std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	// std::tm now_tm = *std::localtime(&now_c);
	// char dateBuffer[32] = {}, timeBuffer[32] = {};
	// SPRINTF3(dateBuffer, 30, "\"%04d-%02d-%02d\"", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday);
	// SPRINTF3(timeBuffer, 30, "\"%02d:%02d:%02d\"", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
	// Options::CmdDefineTable.Add("__DATE__", dateBuffer, nullptr);
	// Options::CmdDefineTable.Add("__TIME__", timeBuffer, nullptr);

	sj_input = &input;
	sj_output = &output;
	sj_errors = &errors;
	sj_output->clear();
	sj_errors->clear();

	Options::systemSyntax = Options::syx; // create copy of initial system settings of syntax

	int base_encoding = ConvertEncoding;

	do
	{
		++pass;
		InitPass();
		if (pass == LASTPASS) OpenDest();

		IsRunning = 1;
		ConvertEncoding = base_encoding;
		OpenFile(NULL, NULL);

		while (!RepeatStack.empty()) {
			sourcePosStack.push_back(RepeatStack.top().sourcePos);	// mark DUP line with error
			Error("[DUP/REPT] missing EDUP/ENDR to end repeat-block");
			sourcePosStack.pop_back();
			RepeatStack.pop();
		}

		if (DISP_NONE != PseudoORG) {
			CurAddress = adrdisp;
			PseudoORG = DISP_NONE;
		}

		// if (Options::OutputVerbosity <= OV_ALL) {
		// 	if (pass != LASTPASS) {
		// 		_CERR "Pass " _CMDL pass _CMDL " complete (" _CMDL ErrorCount _CMDL " errors)" _ENDL;
		// 	} else {
		// 		_CERR "Pass 3 complete" _ENDL;
		// 	}
		// }
	} while (pass < LASTPASS);

	if (ErrorCount == 0) {
		SaveRAM(*sj_output, 0x4000, 0xFFFF);
	}

	pass = 9999; /* added for detect end of compiling */
	Close();
	FreeRAM();
	lua_impl_close();

	/*
	if (Options::OutputVerbosity <= OV_ALL) {
		double dwCount;
		dwCount = GetTickCount() - dwStart;
		if (dwCount < 0) dwCount = 0;
		fprintf(stderr, "Errors: %d, warnings: %d, compiled: %d lines, work time: %.3f seconds\n",
				ErrorCount, WarningCount, CompiledCurrentLine, dwCount / 1000);
	}
	*/

	return ErrorCount;
}
//eof esjasm.cpp
