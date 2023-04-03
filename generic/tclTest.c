/*
 * tclTest.c --
 *
 *	This file contains C command functions for a bunch of additional Tcl
 *	commands that are used for testing out Tcl's C interfaces. These
 *	commands are not normally included in Tcl applications; they're only
 *	used for testing.
 *
 * Copyright © 1993-1994 The Regents of the University of California.
 * Copyright © 1994-1997 Sun Microsystems, Inc.
 * Copyright © 1998-2000 Ajuba Solutions.
 * Copyright © 2003 Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#include "tclInt.h"
#ifdef TCL_WITH_EXTERNAL_TOMMATH
#   include "tommath.h"
#else
#   include "tclTomMath.h"
#endif
#include "tclOO.h"
#include <math.h>

/*
 * Required for Testregexp*Cmd
 */
#include "tclRegexp.h"

/*
 * Required for the TestChannelCmd and TestChannelEventCmd
 */
#include "tclIO.h"

#include "tclUuid.h"

/*
 * Declare external functions used in Windows tests.
 */
DLLEXPORT int		Tcltest_Init(Tcl_Interp *interp);
DLLEXPORT int		Tcltest_SafeInit(Tcl_Interp *interp);

/*
 * Dynamic string shared by TestdcallCmd and DelCallbackProc; used to collect
 * the results of the various deletion callbacks.
 */

static Tcl_DString delString;
static Tcl_Interp *delInterp;

/*
 * One of the following structures exists for each command created by the
 * "testcmdtoken" command.
 */

typedef struct TestCommandTokenRef {
    int id;			/* Identifier for this reference. */
    Tcl_Command token;		/* Tcl's token for the command. */
    const char *value;
    struct TestCommandTokenRef *nextPtr;
				/* Next in list of references. */
} TestCommandTokenRef;

static TestCommandTokenRef *firstCommandTokenRef = NULL;
static int nextCommandTokenRefId = 1;

/*
 * One of the following structures exists for each asynchronous handler
 * created by the "testasync" command".
 */

typedef struct TestAsyncHandler {
    int id;			/* Identifier for this handler. */
    Tcl_AsyncHandler handler;	/* Tcl's token for the handler. */
    char *command;		/* Command to invoke when the handler is
				 * invoked. */
    struct TestAsyncHandler *nextPtr;
				/* Next is list of handlers. */
} TestAsyncHandler;

/*
 * Start of the socket driver state structure to acces field testFlags
 */

typedef struct TcpState TcpState;

struct TcpState {
    Tcl_Channel channel;	/* Channel associated with this socket. */
    int flags;			/* ORed combination of various bitfields. */
};

TCL_DECLARE_MUTEX(asyncTestMutex)

static TestAsyncHandler *firstHandler = NULL;

/*
 * The dynamic string below is used by the "testdstring" command to test the
 * dynamic string facilities.
 */

static Tcl_DString dstring;

/*
 * The command trace below is used by the "testcmdtraceCmd" command to test
 * the command tracing facilities.
 */

static Tcl_Trace cmdTrace;

/*
 * One of the following structures exists for each command created by
 * TestdelCmd:
 */

typedef struct {
    Tcl_Interp *interp;		/* Interpreter in which command exists. */
    char *deleteCmd;		/* Script to execute when command is deleted.
				 * Malloc'ed. */
} DelCmd;

/*
 * The following is used to keep track of an encoding that invokes a Tcl
 * command.
 */

typedef struct {
    Tcl_Interp *interp;
    char *toUtfCmd;
    char *fromUtfCmd;
} TclEncoding;

/*
 * Boolean flag used by the "testsetmainloop" and "testexitmainloop" commands.
 */

static int exitMainLoop = 0;

/*
 * Event structure used in testing the event queue management procedures.
 */

typedef struct {
    Tcl_Event header;		/* Header common to all events */
    Tcl_Interp *interp;		/* Interpreter that will handle the event */
    Tcl_Obj *command;		/* Command to evaluate when the event occurs */
    Tcl_Obj *tag;		/* Tag for this event used to delete it */
} TestEvent;

/*
 * Simple detach/attach facility for testchannel cut|splice. Allow testing of
 * channel transfer in core testsuite.
 */

typedef struct TestChannel {
    Tcl_Channel chan;		/* Detached channel */
    struct TestChannel *nextPtr;/* Next in detached channel pool */
} TestChannel;

static TestChannel *firstDetached;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		AsyncHandlerProc(void *clientData,
			    Tcl_Interp *interp, int code);
static Tcl_ThreadCreateType AsyncThreadProc(void *);
static void		CleanupTestSetassocdataTests(
			    void *clientData, Tcl_Interp *interp);
static void		CmdDelProc1(void *clientData);
static void		CmdDelProc2(void *clientData);
static Tcl_CmdProc	CmdProc1;
static Tcl_CmdProc	CmdProc2;
static void		CmdTraceDeleteProc(
			    void *clientData, Tcl_Interp *interp,
			    int level, char *command, Tcl_CmdProc *cmdProc,
			    void *cmdClientData, int argc,
			    const char *argv[]);
static void		CmdTraceProc(void *clientData,
			    Tcl_Interp *interp, int level, char *command,
			    Tcl_CmdProc *cmdProc, void *cmdClientData,
			    int argc, const char *argv[]);
static Tcl_CmdProc	CreatedCommandProc;
static Tcl_CmdProc	CreatedCommandProc2;
static void		DelCallbackProc(void *clientData,
			    Tcl_Interp *interp);
static Tcl_CmdProc	DelCmdProc;
static void		DelDeleteProc(void *clientData);
static void		EncodingFreeProc(void *clientData);
static int		EncodingToUtfProc(void *clientData,
			    const char *src, int srcLen, int flags,
			    Tcl_EncodingState *statePtr, char *dst,
			    int dstLen, int *srcReadPtr, int *dstWrotePtr,
			    int *dstCharsPtr);
static int		EncodingFromUtfProc(void *clientData,
			    const char *src, int srcLen, int flags,
			    Tcl_EncodingState *statePtr, char *dst,
			    int dstLen, int *srcReadPtr, int *dstWrotePtr,
			    int *dstCharsPtr);
static void		ExitProcEven(void *clientData);
static void		ExitProcOdd(void *clientData);
static Tcl_ObjCmdProc	GetTimesObjCmd;
static Tcl_ResolveCompiledVarProc	InterpCompiledVarResolver;
static void		MainLoop(void);
static Tcl_CmdProc	NoopCmd;
static Tcl_ObjCmdProc	NoopObjCmd;
static int		ObjTraceProc(void *clientData,
			    Tcl_Interp *interp, int level, const char *command,
			    Tcl_Command commandToken, int objc,
			    Tcl_Obj *const objv[]);
static void		ObjTraceDeleteProc(void *clientData);
static void		PrintParse(Tcl_Interp *interp, Tcl_Parse *parsePtr);
static void		SpecialFree(void *blockPtr);
static int		StaticInitProc(Tcl_Interp *interp);
static Tcl_CmdProc	TestasyncCmd;
static Tcl_ObjCmdProc	TestbumpinterpepochObjCmd;
static Tcl_ObjCmdProc	TestbytestringObjCmd;
static Tcl_ObjCmdProc	TestsetbytearraylengthObjCmd;
static Tcl_ObjCmdProc	TestpurebytesobjObjCmd;
static Tcl_ObjCmdProc	TeststringbytesObjCmd;
static Tcl_CmdProc	TestcmdinfoCmd;
static Tcl_CmdProc	TestcmdtokenCmd;
static Tcl_CmdProc	TestcmdtraceCmd;
static Tcl_CmdProc	TestconcatobjCmd;
static Tcl_CmdProc	TestcreatecommandCmd;
static Tcl_CmdProc	TestdcallCmd;
static Tcl_CmdProc	TestdelCmd;
static Tcl_CmdProc	TestdelassocdataCmd;
static Tcl_ObjCmdProc	TestdoubledigitsObjCmd;
static Tcl_CmdProc	TestdstringCmd;
static Tcl_ObjCmdProc	TestencodingObjCmd;
static Tcl_ObjCmdProc	TestevalexObjCmd;
static Tcl_ObjCmdProc	TestevalobjvObjCmd;
static Tcl_ObjCmdProc	TesteventObjCmd;
static int		TesteventProc(Tcl_Event *event, int flags);
static int		TesteventDeleteProc(Tcl_Event *event,
			    void *clientData);
static Tcl_CmdProc	TestexithandlerCmd;
static Tcl_CmdProc	TestexprlongCmd;
static Tcl_ObjCmdProc	TestexprlongobjCmd;
static Tcl_CmdProc	TestexprdoubleCmd;
static Tcl_ObjCmdProc	TestexprdoubleobjCmd;
static Tcl_ObjCmdProc	TestexprparserObjCmd;
static Tcl_CmdProc	TestexprstringCmd;
static Tcl_ObjCmdProc	TestfileCmd;
static Tcl_ObjCmdProc	TestfilelinkCmd;
static Tcl_CmdProc	TestfeventCmd;
static Tcl_CmdProc	TestgetassocdataCmd;
static Tcl_CmdProc	TestgetintCmd;
static Tcl_CmdProc	TestlongsizeCmd;
static Tcl_CmdProc	TestgetplatformCmd;
static Tcl_ObjCmdProc	TestgetvarfullnameCmd;
static Tcl_CmdProc	TestinterpdeleteCmd;
static Tcl_CmdProc	TestlinkCmd;
static Tcl_ObjCmdProc	TestlinkarrayCmd;
static Tcl_ObjCmdProc	TestlistrepCmd;
static Tcl_ObjCmdProc	TestlocaleCmd;
static Tcl_CmdProc	TestmainthreadCmd;
static Tcl_ObjCmdProc	TestmsbObjCmd;
static Tcl_CmdProc	TestsetmainloopCmd;
static Tcl_CmdProc	TestexitmainloopCmd;
static Tcl_CmdProc	TestpanicCmd;
static Tcl_ObjCmdProc	TestparseargsCmd;
static Tcl_ObjCmdProc	TestparserObjCmd;
static Tcl_ObjCmdProc	TestparsevarObjCmd;
static Tcl_ObjCmdProc	TestparsevarnameObjCmd;
static Tcl_ObjCmdProc	TestpreferstableObjCmd;
static Tcl_ObjCmdProc	TestprintObjCmd;
static Tcl_ObjCmdProc	TestregexpObjCmd;
static Tcl_ObjCmdProc	TestreturnObjCmd;
static void		TestregexpXflags(const char *string,
			    size_t length, int *cflagsPtr, int *eflagsPtr);
static Tcl_CmdProc	TestsetassocdataCmd;
static Tcl_CmdProc	TestsetCmd;
static Tcl_CmdProc	Testset2Cmd;
static Tcl_CmdProc	TestseterrorcodeCmd;
static Tcl_ObjCmdProc	TestsetobjerrorcodeCmd;
static Tcl_CmdProc	TestsetplatformCmd;
static Tcl_CmdProc	TeststaticlibraryCmd;
static Tcl_CmdProc	TesttranslatefilenameCmd;
static Tcl_CmdProc	TestupvarCmd;
static Tcl_ObjCmdProc2	TestWrongNumArgsObjCmd;
static Tcl_ObjCmdProc	TestGetIndexFromObjStructObjCmd;
static Tcl_CmdProc	TestChannelCmd;
static Tcl_CmdProc	TestChannelEventCmd;
static Tcl_CmdProc	TestSocketCmd;
static Tcl_ObjCmdProc	TestFilesystemObjCmd;
static Tcl_ObjCmdProc	TestSimpleFilesystemObjCmd;
static void		TestReport(const char *cmd, Tcl_Obj *arg1,
			    Tcl_Obj *arg2);
static Tcl_ObjCmdProc	TestgetencpathObjCmd;
static Tcl_ObjCmdProc	TestsetencpathObjCmd;
static Tcl_Obj *	TestReportGetNativePath(Tcl_Obj *pathPtr);
static Tcl_FSStatProc TestReportStat;
static Tcl_FSAccessProc TestReportAccess;
static Tcl_FSOpenFileChannelProc TestReportOpenFileChannel;
static Tcl_FSMatchInDirectoryProc TestReportMatchInDirectory;
static Tcl_FSChdirProc TestReportChdir;
static Tcl_FSLstatProc TestReportLstat;
static Tcl_FSCopyFileProc TestReportCopyFile;
static Tcl_FSDeleteFileProc TestReportDeleteFile;
static Tcl_FSRenameFileProc TestReportRenameFile;
static Tcl_FSCreateDirectoryProc TestReportCreateDirectory;
static Tcl_FSCopyDirectoryProc TestReportCopyDirectory;
static Tcl_FSRemoveDirectoryProc TestReportRemoveDirectory;
static int TestReportLoadFile(Tcl_Interp *interp, Tcl_Obj *pathPtr,
	Tcl_LoadHandle *handlePtr, Tcl_FSUnloadFileProc **unloadProcPtr);
static Tcl_FSLinkProc TestReportLink;
static Tcl_FSFileAttrStringsProc TestReportFileAttrStrings;
static Tcl_FSFileAttrsGetProc TestReportFileAttrsGet;
static Tcl_FSFileAttrsSetProc TestReportFileAttrsSet;
static Tcl_FSUtimeProc TestReportUtime;
static Tcl_FSNormalizePathProc TestReportNormalizePath;
static Tcl_FSPathInFilesystemProc TestReportInFilesystem;
static Tcl_FSFreeInternalRepProc TestReportFreeInternalRep;
static Tcl_FSDupInternalRepProc TestReportDupInternalRep;
static Tcl_CmdProc TestServiceModeCmd;
static Tcl_FSStatProc SimpleStat;
static Tcl_FSAccessProc SimpleAccess;
static Tcl_FSOpenFileChannelProc SimpleOpenFileChannel;
static Tcl_FSListVolumesProc SimpleListVolumes;
static Tcl_FSPathInFilesystemProc SimplePathInFilesystem;
static Tcl_Obj *	SimpleRedirect(Tcl_Obj *pathPtr);
static Tcl_FSMatchInDirectoryProc SimpleMatchInDirectory;
static Tcl_ObjCmdProc	TestUtfNextCmd;
static Tcl_ObjCmdProc	TestUtfPrevCmd;
static Tcl_ObjCmdProc	TestNumUtfCharsCmd;
static Tcl_ObjCmdProc	TestFindFirstCmd;
static Tcl_ObjCmdProc	TestFindLastCmd;
static Tcl_ObjCmdProc	TestHashSystemHashCmd;
static Tcl_ObjCmdProc	TestGetIntForIndexCmd;

static Tcl_NRPostProc	NREUnwind_callback;
static Tcl_ObjCmdProc	TestNREUnwind;
static Tcl_ObjCmdProc	TestNRELevels;
static Tcl_ObjCmdProc	TestInterpResolverCmd;
#if defined(HAVE_CPUID) && !defined(MAC_OSX_TCL)
static Tcl_ObjCmdProc	TestcpuidCmd;
#endif
static Tcl_ObjCmdProc	TestApplyLambdaObjCmd;

static const Tcl_Filesystem testReportingFilesystem = {
    "reporting",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    TestReportInFilesystem, /* path in */
    TestReportDupInternalRep,
    TestReportFreeInternalRep,
    NULL, /* native to norm */
    NULL, /* convert to native */
    TestReportNormalizePath,
    NULL, /* path type */
    NULL, /* separator */
    TestReportStat,
    TestReportAccess,
    TestReportOpenFileChannel,
    TestReportMatchInDirectory,
    TestReportUtime,
    TestReportLink,
    NULL /* list volumes */,
    TestReportFileAttrStrings,
    TestReportFileAttrsGet,
    TestReportFileAttrsSet,
    TestReportCreateDirectory,
    TestReportRemoveDirectory,
    TestReportDeleteFile,
    TestReportCopyFile,
    TestReportRenameFile,
    TestReportCopyDirectory,
    TestReportLstat,
    (Tcl_FSLoadFileProc *) TestReportLoadFile,
    NULL /* cwd */,
    TestReportChdir
};

static const Tcl_Filesystem simpleFilesystem = {
    "simple",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    SimplePathInFilesystem,
    NULL,
    NULL,
    /* No internal to normalized, since we don't create any
     * pure 'internal' Tcl_Obj path representations */
    NULL,
    /* No create native rep function, since we don't use it
     * or 'Tcl_FSNewNativePath' */
    NULL,
    /* Normalize path isn't needed - we assume paths only have
     * one representation */
    NULL,
    NULL,
    NULL,
    SimpleStat,
    SimpleAccess,
    SimpleOpenFileChannel,
    SimpleMatchInDirectory,
    NULL,
    /* We choose not to support symbolic links inside our vfs's */
    NULL,
    SimpleListVolumes,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    /* No copy file - fallback will occur at Tcl level */
    NULL,
    /* No rename file - fallback will occur at Tcl level */
    NULL,
    /* No copy directory - fallback will occur at Tcl level */
    NULL,
    /* Use stat for lstat */
    NULL,
    /* No load - fallback on core implementation */
    NULL,
    /* We don't need a getcwd or chdir - fallback on Tcl's versions */
    NULL,
    NULL
};


/*
 *----------------------------------------------------------------------
 *
 * Tcltest_Init --
 *
 *	This procedure performs application-specific initialization. Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

static const char version[] = TCL_PATCH_LEVEL "+" STRINGIFY(TCL_VERSION_UUID)
#if defined(__clang__) && defined(__clang_major__)
	    ".clang-" STRINGIFY(__clang_major__)
#if __clang_minor__ < 10
	    "0"
#endif
	    STRINGIFY(__clang_minor__)
#endif
#ifdef TCL_COMPILE_DEBUG
	    ".compiledebug"
#endif
#ifdef TCL_COMPILE_STATS
	    ".compilestats"
#endif
#if defined(__cplusplus) && !defined(__OBJC__)
	    ".cplusplus"
#endif
#ifndef NDEBUG
	    ".debug"
#endif
#if !defined(__clang__) && !defined(__INTEL_COMPILER) && defined(__GNUC__)
	    ".gcc-" STRINGIFY(__GNUC__)
#if __GNUC_MINOR__ < 10
	    "0"
#endif
	    STRINGIFY(__GNUC_MINOR__)
#endif
#ifdef __INTEL_COMPILER
	    ".icc-" STRINGIFY(__INTEL_COMPILER)
#endif
#if (defined(_WIN32) && !defined(_WIN64)) || (ULONG_MAX == 0xffffffffUL)
	    ".ilp32"
#endif
#ifdef TCL_MEM_DEBUG
	    ".memdebug"
#endif
#if defined(_MSC_VER)
	    ".msvc-" STRINGIFY(_MSC_VER)
#endif
#ifdef USE_NMAKE
	    ".nmake"
#endif
#if !TCL_THREADS
	    ".no-thread"
#endif
#ifndef TCL_CFG_OPTIMIZED
	    ".no-optimize"
#endif
#ifdef __OBJC__
	    ".objective-c"
#if defined(__cplusplus)
	    "plusplus"
#endif
#endif
#ifdef TCL_CFG_PROFILED
	    ".profile"
#endif
#ifdef PURIFY
	    ".purify"
#endif
#ifdef STATIC_BUILD
	    ".static"
#endif
#if TCL_UTF_MAX < 4
	    ".utf-16"
#endif
;

int
Tcltest_Init(
    Tcl_Interp *interp)		/* Interpreter for application. */
{
    Tcl_CmdInfo info;
    Tcl_Obj **objv, *objPtr;
    Tcl_Size objc;
    int index;
    static const char *const specialOptions[] = {
	"-appinitprocerror", "-appinitprocdeleteinterp",
	"-appinitprocclosestderr", "-appinitprocsetrcfile", NULL
    };

    if (Tcl_InitStubs(interp, "8.7-", 0) == NULL) {
	return TCL_ERROR;
    }
#ifndef TCL_WITH_EXTERNAL_TOMMATH
    if (Tcl_TomMath_InitStubs(interp, "8.7-") == NULL) {
	return TCL_ERROR;
    }
#endif
    if (Tcl_OOInitStubs(interp) == NULL) {
	return TCL_ERROR;
    }

    if (Tcl_GetCommandInfo(interp, "::tcl::build-info", &info)) {
#if TCL_MAJOR_VERSION > 8
	if (info.isNativeObjectProc == 2) {
	    Tcl_CreateObjCommand2(interp, "::tcl::test::build-info",
		    info.objProc2, (void *)version, NULL);
    } else
#endif
	Tcl_CreateObjCommand(interp, "::tcl::test::build-info",
		info.objProc, (void *)version, NULL);
    }
    if (Tcl_PkgProvideEx(interp, "tcl::test", TCL_PATCH_LEVEL, NULL) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Create additional commands and math functions for testing Tcl.
     */

    Tcl_CreateObjCommand(interp, "gettimes", GetTimesObjCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "noop", NoopCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "noop", NoopObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testpurebytesobj", TestpurebytesobjObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testsetbytearraylength", TestsetbytearraylengthObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testbytestring", TestbytestringObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "teststringbytes", TeststringbytesObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "testwrongnumargs", TestWrongNumArgsObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testfilesystem", TestFilesystemObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testsimplefilesystem", TestSimpleFilesystemObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testgetindexfromobjstruct",
	    TestGetIndexFromObjStructObjCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testasync", TestasyncCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testbumpinterpepoch",
	    TestbumpinterpepochObjCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testchannel", TestChannelCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testchannelevent", TestChannelEventCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testcmdtoken", TestcmdtokenCmd, NULL,
	    NULL);
    Tcl_CreateCommand(interp, "testcmdinfo", TestcmdinfoCmd, NULL,
	    NULL);
    Tcl_CreateCommand(interp, "testcmdtrace", TestcmdtraceCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testconcatobj", TestconcatobjCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testcreatecommand", TestcreatecommandCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testdcall", TestdcallCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testdel", TestdelCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testdelassocdata", TestdelassocdataCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testdoubledigits", TestdoubledigitsObjCmd,
			 NULL, NULL);
    Tcl_DStringInit(&dstring);
    Tcl_CreateCommand(interp, "testdstring", TestdstringCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "testencoding", TestencodingObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "testevalex", TestevalexObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testevalobjv", TestevalobjvObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testevent", TesteventObjCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testexithandler", TestexithandlerCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testexprlong", TestexprlongCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testexprlongobj", TestexprlongobjCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testexprdouble", TestexprdoubleCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testexprdoubleobj", TestexprdoubleobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testexprparser", TestexprparserObjCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testexprstring", TestexprstringCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testfevent", TestfeventCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "testfilelink", TestfilelinkCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testfile", TestfileCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testhashsystemhash",
	    TestHashSystemHashCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testgetassocdata", TestgetassocdataCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testgetint", TestgetintCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testlongsize", TestlongsizeCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testgetplatform", TestgetplatformCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testgetvarfullname",
	    TestgetvarfullnameCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testinterpdelete", TestinterpdeleteCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testlink", TestlinkCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testlinkarray", TestlinkarrayCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testlistrep", TestlistrepCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testlocale", TestlocaleCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "testmsb", TestmsbObjCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testpanic", TestpanicCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testparseargs", TestparseargsCmd,NULL,NULL);
    Tcl_CreateObjCommand(interp, "testparser", TestparserObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testparsevar", TestparsevarObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testparsevarname", TestparsevarnameObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testpreferstable", TestpreferstableObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testprint", TestprintObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testregexp", TestregexpObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testreturn", TestreturnObjCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testservicemode", TestServiceModeCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testsetassocdata", TestsetassocdataCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testsetnoerr", TestsetCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testseterr", TestsetCmd,
	    INT2PTR(TCL_LEAVE_ERR_MSG), NULL);
    Tcl_CreateCommand(interp, "testset2", Testset2Cmd,
	    INT2PTR(TCL_LEAVE_ERR_MSG), NULL);
    Tcl_CreateCommand(interp, "testseterrorcode", TestseterrorcodeCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testsetobjerrorcode",
	    TestsetobjerrorcodeCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testutfnext",
	    TestUtfNextCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testutfprev",
	    TestUtfPrevCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testnumutfchars",
	    TestNumUtfCharsCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testfindfirst",
	    TestFindFirstCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testfindlast",
	    TestFindLastCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testgetintforindex",
	    TestGetIntForIndexCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testsetplatform", TestsetplatformCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testsocket", TestSocketCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "teststaticlibrary", TeststaticlibraryCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testtranslatefilename",
	    TesttranslatefilenameCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testupvar", TestupvarCmd, NULL, NULL);
    Tcl_CreateCommand(interp, "testmainthread", TestmainthreadCmd, NULL,
	    NULL);
    Tcl_CreateCommand(interp, "testsetmainloop", TestsetmainloopCmd,
	    NULL, NULL);
    Tcl_CreateCommand(interp, "testexitmainloop", TestexitmainloopCmd,
	    NULL, NULL);
#if defined(HAVE_CPUID) && !defined(MAC_OSX_TCL)
    Tcl_CreateObjCommand(interp, "testcpuid", TestcpuidCmd,
	    NULL, NULL);
#endif
    Tcl_CreateObjCommand(interp, "testnreunwind", TestNREUnwind,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testnrelevels", TestNRELevels,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testinterpresolver", TestInterpResolverCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testgetencpath", TestgetencpathObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testsetencpath", TestsetencpathObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testapplylambda", TestApplyLambdaObjCmd,
	    NULL, NULL);

    if (TclObjTest_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Procbodytest_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }
#if TCL_THREADS
    if (TclThread_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }
#endif

    /*
     * Check for special options used in ../tests/main.test
     */

    objPtr = Tcl_GetVar2Ex(interp, "argv", NULL, TCL_GLOBAL_ONLY);
    if (objPtr != NULL) {
	if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc && (Tcl_GetIndexFromObj(NULL, objv[0], specialOptions, NULL,
		TCL_EXACT, &index) == TCL_OK)) {
	    switch (index) {
	    case 0:
		return TCL_ERROR;
	    case 1:
		Tcl_DeleteInterp(interp);
		return TCL_ERROR;
	    case 2: {
		int mode;
		Tcl_UnregisterChannel(interp,
			Tcl_GetChannel(interp, "stderr", &mode));
		return TCL_ERROR;
	    }
	    case 3:
		if (objc > 1) {
		    Tcl_SetVar2Ex(interp, "tcl_rcFileName", NULL, objv[1],
			    TCL_GLOBAL_ONLY);
		}
		return TCL_ERROR;
	    }
	}
    }

    /*
     * And finally add any platform specific test commands.
     */

    return TclplatformtestInit(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcltest_SafeInit --
 *
 *	This procedure performs application-specific initialization. Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcltest_SafeInit(
    Tcl_Interp *interp)		/* Interpreter for application. */
{
    Tcl_CmdInfo info;

    if (Tcl_InitStubs(interp, "8.7-", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetCommandInfo(interp, "::tcl::build-info", &info)) {
#if TCL_MAJOR_VERSION > 8
	if (info.isNativeObjectProc == 2) {
	    Tcl_CreateObjCommand2(interp, "::tcl::test::build-info",
		    info.objProc2, (void *)version, NULL);
    } else
#endif
	Tcl_CreateObjCommand(interp, "::tcl::test::build-info",
		info.objProc, (void *)version, NULL);
    }
    if (Tcl_PkgProvideEx(interp, "tcl::test", TCL_PATCH_LEVEL, NULL) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return Procbodytest_SafeInit(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * TestasyncCmd --
 *
 *	This procedure implements the "testasync" command.  It is used
 *	to test the asynchronous handler facilities of Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes, and invokes handlers.
 *
 *----------------------------------------------------------------------
 */

static int
TestasyncCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,			/* Current interpreter. */
    int argc,				/* Number of arguments. */
    const char **argv)			/* Argument strings. */
{
    TestAsyncHandler *asyncPtr, *prevPtr;
    int id, code;
    static int nextId = 1;

    if (argc < 2) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	asyncPtr = (TestAsyncHandler *)Tcl_Alloc(sizeof(TestAsyncHandler));
	asyncPtr->command = (char *)Tcl_Alloc(strlen(argv[2]) + 1);
	strcpy(asyncPtr->command, argv[2]);
        Tcl_MutexLock(&asyncTestMutex);
	asyncPtr->id = nextId;
	nextId++;
	asyncPtr->handler = Tcl_AsyncCreate(AsyncHandlerProc,
                                            INT2PTR(asyncPtr->id));
	asyncPtr->nextPtr = firstHandler;
	firstHandler = asyncPtr;
        Tcl_MutexUnlock(&asyncTestMutex);
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(asyncPtr->id));
    } else if (strcmp(argv[1], "delete") == 0) {
	if (argc == 2) {
            Tcl_MutexLock(&asyncTestMutex);
	    while (firstHandler != NULL) {
		asyncPtr = firstHandler;
		firstHandler = asyncPtr->nextPtr;
		Tcl_AsyncDelete(asyncPtr->handler);
		Tcl_Free(asyncPtr->command);
		Tcl_Free(asyncPtr);
	    }
            Tcl_MutexUnlock(&asyncTestMutex);
	    return TCL_OK;
	}
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[2], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
        Tcl_MutexLock(&asyncTestMutex);
	for (prevPtr = NULL, asyncPtr = firstHandler; asyncPtr != NULL;
		prevPtr = asyncPtr, asyncPtr = asyncPtr->nextPtr) {
	    if (asyncPtr->id != id) {
		continue;
	    }
	    if (prevPtr == NULL) {
		firstHandler = asyncPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = asyncPtr->nextPtr;
	    }
	    Tcl_AsyncDelete(asyncPtr->handler);
	    Tcl_Free(asyncPtr->command);
	    Tcl_Free(asyncPtr);
	    break;
	}
        Tcl_MutexUnlock(&asyncTestMutex);
    } else if (strcmp(argv[1], "mark") == 0) {
	if (argc != 5) {
	    goto wrongNumArgs;
	}
	if ((Tcl_GetInt(interp, argv[2], &id) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &code) != TCL_OK)) {
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&asyncTestMutex);
	for (asyncPtr = firstHandler; asyncPtr != NULL;
		asyncPtr = asyncPtr->nextPtr) {
	    if (asyncPtr->id == id) {
		Tcl_AsyncMark(asyncPtr->handler);
		break;
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(argv[3], TCL_INDEX_NONE));
	Tcl_MutexUnlock(&asyncTestMutex);
	return code;
    } else if (strcmp(argv[1], "marklater") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[2], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
        Tcl_MutexLock(&asyncTestMutex);
	for (asyncPtr = firstHandler; asyncPtr != NULL;
		asyncPtr = asyncPtr->nextPtr) {
	    if (asyncPtr->id == id) {
		Tcl_ThreadId threadID;
		if (Tcl_CreateThread(&threadID, AsyncThreadProc,
			INT2PTR(id), TCL_THREAD_STACK_DEFAULT,
			TCL_THREAD_NOFLAGS) != TCL_OK) {
		    Tcl_AppendResult(interp, "can't create thread", NULL);
		    Tcl_MutexUnlock(&asyncTestMutex);
		    return TCL_ERROR;
		}
		break;
	    }
	}
        Tcl_MutexUnlock(&asyncTestMutex);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, int, mark, or marklater", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
AsyncHandlerProc(
    void *clientData,	/* If of TestAsyncHandler structure.
                                 * in global list. */
    Tcl_Interp *interp,		/* Interpreter in which command was
				 * executed, or NULL. */
    int code)			/* Current return code from command. */
{
    TestAsyncHandler *asyncPtr;
    int id = PTR2INT(clientData);
    const char *listArgv[4];
    char *cmd;
    char string[TCL_INTEGER_SPACE];

    Tcl_MutexLock(&asyncTestMutex);
    for (asyncPtr = firstHandler; asyncPtr != NULL;
            asyncPtr = asyncPtr->nextPtr) {
        if (asyncPtr->id == id) {
            break;
        }
    }
    Tcl_MutexUnlock(&asyncTestMutex);

    if (!asyncPtr) {
        /* Woops - this one was deleted between the AsyncMark and now */
        return TCL_OK;
    }

    TclFormatInt(string, code);
    listArgv[0] = asyncPtr->command;
    listArgv[1] = Tcl_GetStringResult(interp);
    listArgv[2] = string;
    listArgv[3] = NULL;
    cmd = Tcl_Merge(3, listArgv);
    if (interp != NULL) {
	code = Tcl_EvalEx(interp, cmd, TCL_INDEX_NONE, 0);
    } else {
	/*
	 * this should not happen, but by definition of how async handlers are
	 * invoked, it's possible.  Better error checking is needed here.
	 */
    }
    Tcl_Free(cmd);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * AsyncThreadProc --
 *
 *	Delivers an asynchronous event to a handler in another thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invokes Tcl_AsyncMark on the handler
 *
 *----------------------------------------------------------------------
 */

static Tcl_ThreadCreateType
AsyncThreadProc(
    void *clientData)	/* Parameter is the id of a
				 * TestAsyncHandler, defined above. */
{
    TestAsyncHandler *asyncPtr;
    int id = PTR2INT(clientData);

    Tcl_Sleep(1);
    Tcl_MutexLock(&asyncTestMutex);
    for (asyncPtr = firstHandler; asyncPtr != NULL;
         asyncPtr = asyncPtr->nextPtr) {
        if (asyncPtr->id == id) {
            Tcl_AsyncMark(asyncPtr->handler);
            break;
        }
    }
    Tcl_MutexUnlock(&asyncTestMutex);
    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}

static int
TestbumpinterpepochObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Interp *iPtr = (Interp *)interp;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    iPtr->compileEpoch++;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestcmdinfoCmd --
 *
 *	This procedure implements the "testcmdinfo" command.  It is used to
 *	test Tcl_GetCommandInfo, Tcl_SetCommandInfo, and command creation and
 *	deletion.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various commands and modifies their data.
 *
 *----------------------------------------------------------------------
 */

static int
TestcmdinfoCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tcl_CmdInfo info;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option cmdName\"", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	Tcl_CreateCommand(interp, argv[2], CmdProc1, (void *) "original",
		CmdDelProc1);
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_DStringInit(&delString);
	Tcl_DeleteCommand(interp, argv[2]);
	Tcl_DStringResult(interp, &delString);
    } else if (strcmp(argv[1], "get") == 0) {
	if (Tcl_GetCommandInfo(interp, argv[2], &info) ==0) {
	    Tcl_AppendResult(interp, "??", NULL);
	    return TCL_OK;
	}
	if (info.proc == CmdProc1) {
	    Tcl_AppendResult(interp, "CmdProc1", " ",
		    (char *) info.clientData, NULL);
	} else if (info.proc == CmdProc2) {
	    Tcl_AppendResult(interp, "CmdProc2", " ",
		    (char *) info.clientData, NULL);
	} else {
	    Tcl_AppendResult(interp, "unknown", NULL);
	}
	if (info.deleteProc == CmdDelProc1) {
	    Tcl_AppendResult(interp, " CmdDelProc1", " ",
		    (char *) info.deleteData, NULL);
	} else if (info.deleteProc == CmdDelProc2) {
	    Tcl_AppendResult(interp, " CmdDelProc2", " ",
		    (char *) info.deleteData, NULL);
	} else {
	    Tcl_AppendResult(interp, " unknown", NULL);
	}
	Tcl_AppendResult(interp, " ", info.namespacePtr->fullName, NULL);
	if (info.isNativeObjectProc) {
	    Tcl_AppendResult(interp, " nativeObjectProc", NULL);
	} else {
	    Tcl_AppendResult(interp, " stringProc", NULL);
	}
    } else if (strcmp(argv[1], "modify") == 0) {
	info.proc = CmdProc2;
	info.clientData = (void *) "new_command_data";
	info.objProc = NULL;
	info.objClientData = NULL;
	info.deleteProc = CmdDelProc2;
	info.deleteData = (void *) "new_delete_data";
	if (Tcl_SetCommandInfo(interp, argv[2], &info) == 0) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(0));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(1));
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, get, or modify", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
CmdProc0(
    void *clientData,	/* String to return. */
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    TestCommandTokenRef *refPtr = (TestCommandTokenRef *) clientData;
    Tcl_AppendResult(interp, "CmdProc1 ", refPtr->value, NULL);
    return TCL_OK;
}

static int
CmdProc1(
    void *clientData,	/* String to return. */
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    Tcl_AppendResult(interp, "CmdProc1 ", (char *) clientData, NULL);
    return TCL_OK;
}

static int
CmdProc2(
    void *clientData,	/* String to return. */
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    Tcl_AppendResult(interp, "CmdProc2 ", (char *) clientData, NULL);
    return TCL_OK;
}

static void
CmdDelProc0(
    void *clientData)	/* String to save. */
{
    TestCommandTokenRef *thisRefPtr, *prevRefPtr = NULL;
    TestCommandTokenRef *refPtr = (TestCommandTokenRef *) clientData;
    int id = refPtr->id;
    for (thisRefPtr = firstCommandTokenRef; refPtr != NULL;
	thisRefPtr = thisRefPtr->nextPtr) {
	if (thisRefPtr->id == id) {
	    if (prevRefPtr != NULL) {
		prevRefPtr->nextPtr = thisRefPtr->nextPtr;
	    } else {
		firstCommandTokenRef = thisRefPtr->nextPtr;
	    }
	    break;
	}
	prevRefPtr = thisRefPtr;
    }
    Tcl_Free(refPtr);
}

static void
CmdDelProc1(
    void *clientData)	/* String to save. */
{
    Tcl_DStringInit(&delString);
    Tcl_DStringAppend(&delString, "CmdDelProc1 ", TCL_INDEX_NONE);
    Tcl_DStringAppend(&delString, (char *) clientData, TCL_INDEX_NONE);
}

static void
CmdDelProc2(
    void *clientData)	/* String to save. */
{
    Tcl_DStringInit(&delString);
    Tcl_DStringAppend(&delString, "CmdDelProc2 ", TCL_INDEX_NONE);
    Tcl_DStringAppend(&delString, (char *) clientData, TCL_INDEX_NONE);
}

/*
 *----------------------------------------------------------------------
 *
 * TestcmdtokenCmd --
 *
 *	This procedure implements the "testcmdtoken" command. It is used to
 *	test Tcl_Command tokens and procedures such as Tcl_GetCommandFullName.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various commands and modifies their data.
 *
 *----------------------------------------------------------------------
 */

static int
TestcmdtokenCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    TestCommandTokenRef *refPtr;
    int id;
    char buf[30];

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option arg\"", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	refPtr = (TestCommandTokenRef *)Tcl_Alloc(sizeof(TestCommandTokenRef));
	refPtr->token = Tcl_CreateCommand(interp, argv[2], CmdProc0,
		refPtr, CmdDelProc0);
	refPtr->id = nextCommandTokenRefId;
	refPtr->value = "original";
	nextCommandTokenRefId++;
	refPtr->nextPtr = firstCommandTokenRef;
	firstCommandTokenRef = refPtr;
	snprintf(buf, sizeof(buf), "%d", refPtr->id);
	Tcl_AppendResult(interp, buf, NULL);
    } else {
	if (sscanf(argv[2], "%d", &id) != 1) {
	    Tcl_AppendResult(interp, "bad command token \"", argv[2],
		    "\"", NULL);
	    return TCL_ERROR;
	}

	for (refPtr = firstCommandTokenRef; refPtr != NULL;
		refPtr = refPtr->nextPtr) {
	    if (refPtr->id == id) {
		break;
	    }
	}

	if (refPtr == NULL) {
	    Tcl_AppendResult(interp, "bad command token \"", argv[2],
		    "\"", NULL);
	    return TCL_ERROR;
	}

	if (strcmp(argv[1], "name") == 0) {
	    Tcl_Obj *objPtr;

	    objPtr = Tcl_NewObj();
	    Tcl_GetCommandFullName(interp, refPtr->token, objPtr);

	    Tcl_AppendElement(interp,
		    Tcl_GetCommandName(interp, refPtr->token));
	    Tcl_AppendElement(interp, Tcl_GetString(objPtr));
	    Tcl_DecrRefCount(objPtr);
	} else {
	    Tcl_AppendResult(interp, "bad option \"", argv[1],
		    "\": must be create, name, or free", NULL);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestcmdtraceCmd --
 *
 *	This procedure implements the "testcmdtrace" command. It is used
 *	to test Tcl_CreateTrace and Tcl_DeleteTrace.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes a command trace, and tests the invocation of
 *	a procedure by the command trace.
 *
 *----------------------------------------------------------------------
 */

static int
TestcmdtraceCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tcl_DString buffer;
    int result;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option script\"", NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1], "tracetest") == 0) {
	Tcl_DStringInit(&buffer);
	cmdTrace = Tcl_CreateTrace(interp, 50000, CmdTraceProc, &buffer);
	result = Tcl_EvalEx(interp, argv[2], TCL_INDEX_NONE, 0);
	if (result == TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, Tcl_DStringValue(&buffer), NULL);
	}
	Tcl_DeleteTrace(interp, cmdTrace);
	Tcl_DStringFree(&buffer);
    } else if (strcmp(argv[1], "deletetest") == 0) {
	/*
	 * Create a command trace then eval a script to check whether it is
	 * called. Note that this trace procedure removes itself as a further
	 * check of the robustness of the trace proc calling code in
	 * TclNRExecuteByteCode.
	 */

	cmdTrace = Tcl_CreateTrace(interp, 50000, CmdTraceDeleteProc, NULL);
	Tcl_EvalEx(interp, argv[2], TCL_INDEX_NONE, 0);
    } else if (strcmp(argv[1], "leveltest") == 0) {
	Interp *iPtr = (Interp *) interp;
	Tcl_DStringInit(&buffer);
	cmdTrace = Tcl_CreateTrace(interp, iPtr->numLevels + 4, CmdTraceProc,
		&buffer);
	result = Tcl_EvalEx(interp, argv[2], TCL_INDEX_NONE, 0);
	if (result == TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, Tcl_DStringValue(&buffer), NULL);
	}
	Tcl_DeleteTrace(interp, cmdTrace);
	Tcl_DStringFree(&buffer);
    } else if (strcmp(argv[1], "resulttest") == 0) {
	/* Create an object-based trace, then eval a script. This is used
	 * to test return codes other than TCL_OK from the trace engine.
	 */

	static int deleteCalled;

	deleteCalled = 0;
	cmdTrace = Tcl_CreateObjTrace(interp, 50000,
		TCL_ALLOW_INLINE_COMPILATION, ObjTraceProc,
		&deleteCalled, ObjTraceDeleteProc);
	result = Tcl_EvalEx(interp, argv[2], TCL_INDEX_NONE, 0);
	Tcl_DeleteTrace(interp, cmdTrace);
	if (!deleteCalled) {
	    Tcl_AppendResult(interp, "Delete wasn't called", NULL);
	    return TCL_ERROR;
	} else {
	    return result;
	}
    } else if (strcmp(argv[1], "doubletest") == 0) {
	Tcl_Trace t1, t2;

	Tcl_DStringInit(&buffer);
	t1 = Tcl_CreateTrace(interp, 1, CmdTraceProc, &buffer);
	t2 = Tcl_CreateTrace(interp, 50000, CmdTraceProc, &buffer);
	result = Tcl_EvalEx(interp, argv[2], TCL_INDEX_NONE, 0);
	if (result == TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, Tcl_DStringValue(&buffer), NULL);
	}
	Tcl_DeleteTrace(interp, t2);
	Tcl_DeleteTrace(interp, t1);
	Tcl_DStringFree(&buffer);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be tracetest, deletetest, doubletest or resulttest", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
CmdTraceProc(
    void *clientData,	/* Pointer to buffer in which the
				 * command and arguments are appended.
				 * Accumulates test result. */
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*level*/,
    char *command,		/* The command being traced (after
				 * substitutions). */
    TCL_UNUSED(Tcl_CmdProc *) /*cmdProc*/,
    TCL_UNUSED(void *),
    int argc,			/* Number of arguments. */
    const char *argv[])		/* Argument strings. */
{
    Tcl_DString *bufPtr = (Tcl_DString *) clientData;
    int i;

    Tcl_DStringAppendElement(bufPtr, command);

    Tcl_DStringStartSublist(bufPtr);
    for (i = 0;  i < argc;  i++) {
	Tcl_DStringAppendElement(bufPtr, argv[i]);
    }
    Tcl_DStringEndSublist(bufPtr);
}

static void
CmdTraceDeleteProc(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*level*/,
    TCL_UNUSED(char *) /*command*/,
    TCL_UNUSED(Tcl_CmdProc *),
    TCL_UNUSED(void *),
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    /*
     * Remove ourselves to test whether calling Tcl_DeleteTrace within a trace
     * callback causes the for loop in TclNRExecuteByteCode that calls traces to
     * reference freed memory.
     */

    Tcl_DeleteTrace(interp, cmdTrace);
}

static int
ObjTraceProc(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Tcl interpreter */
    TCL_UNUSED(int) /*level*/,
    const char *command,
    TCL_UNUSED(Tcl_Command),
    TCL_UNUSED(int) /*objc*/,
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    const char *word = Tcl_GetString(objv[0]);

    if (!strcmp(word, "Error")) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(command, TCL_INDEX_NONE));
	return TCL_ERROR;
    } else if (!strcmp(word, "Break")) {
	return TCL_BREAK;
    } else if (!strcmp(word, "Continue")) {
	return TCL_CONTINUE;
    } else if (!strcmp(word, "Return")) {
	return TCL_RETURN;
    } else if (!strcmp(word, "OtherStatus")) {
	return 6;
    } else {
	return TCL_OK;
    }
}

static void
ObjTraceDeleteProc(
    void *clientData)
{
    int *intPtr = (int *) clientData;
    *intPtr = 1;		/* Record that the trace was deleted */
}

/*
 *----------------------------------------------------------------------
 *
 * TestcreatecommandCmd --
 *
 *	This procedure implements the "testcreatecommand" command. It is used
 *	to test that the Tcl_CreateCommand creates a new command in the
 *	namespace specified as part of its name, if any. It also checks that
 *	the namespace code ignore single ":"s in the middle or end of a
 *	command name.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes two commands ("test_ns_basic::createdcommand"
 *	and "value:at:").
 *
 *----------------------------------------------------------------------
 */

static int
TestcreatecommandCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option\"", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	Tcl_CreateCommand(interp, "test_ns_basic::createdcommand",
		CreatedCommandProc, NULL, NULL);
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_DeleteCommand(interp, "test_ns_basic::createdcommand");
    } else if (strcmp(argv[1], "create2") == 0) {
	Tcl_CreateCommand(interp, "value:at:",
		CreatedCommandProc2, NULL, NULL);
    } else if (strcmp(argv[1], "delete2") == 0) {
	Tcl_DeleteCommand(interp, "value:at:");
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create, delete, create2, or delete2", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
CreatedCommandProc(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    Tcl_CmdInfo info;
    int found;

    found = Tcl_GetCommandInfo(interp, "test_ns_basic::createdcommand",
	    &info);
    if (!found) {
	Tcl_AppendResult(interp, "CreatedCommandProc could not get command info for test_ns_basic::createdcommand",
		NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "CreatedCommandProc in ",
	    info.namespacePtr->fullName, NULL);
    return TCL_OK;
}

static int
CreatedCommandProc2(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    Tcl_CmdInfo info;
    int found;

    found = Tcl_GetCommandInfo(interp, "value:at:", &info);
    if (!found) {
	Tcl_AppendResult(interp, "CreatedCommandProc2 could not get command info for test_ns_basic::createdcommand",
		NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "CreatedCommandProc2 in ",
	    info.namespacePtr->fullName, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdcallCmd --
 *
 *	This procedure implements the "testdcall" command.  It is used
 *	to test Tcl_CallWhenDeleted.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes interpreters.
 *
 *----------------------------------------------------------------------
 */

static int
TestdcallCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int i, id;

    delInterp = Tcl_CreateInterp();
    Tcl_DStringInit(&delString);
    for (i = 1; i < argc; i++) {
	if (Tcl_GetInt(interp, argv[i], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (id < 0) {
	    Tcl_DontCallWhenDeleted(delInterp, DelCallbackProc,
		    INT2PTR(-id));
	} else {
	    Tcl_CallWhenDeleted(delInterp, DelCallbackProc,
		    INT2PTR(id));
	}
    }
    Tcl_DeleteInterp(delInterp);
    Tcl_DStringResult(interp, &delString);
    return TCL_OK;
}

/*
 * The deletion callback used by TestdcallCmd:
 */

static void
DelCallbackProc(
    void *clientData,	/* Numerical value to append to delString. */
    Tcl_Interp *interp)		/* Interpreter being deleted. */
{
    int id = PTR2INT(clientData);
    char buffer[TCL_INTEGER_SPACE];

    TclFormatInt(buffer, id);
    Tcl_DStringAppendElement(&delString, buffer);
    if (interp != delInterp) {
	Tcl_DStringAppendElement(&delString, "bogus interpreter argument!");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestdelCmd --
 *
 *	This procedure implements the "testdel" command.  It is used
 *	to test calling of command deletion callbacks.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a command.
 *
 *----------------------------------------------------------------------
 */

static int
TestdelCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    DelCmd *dPtr;
    Tcl_Interp *child;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    }

    child = Tcl_GetChild(interp, argv[1]);
    if (child == NULL) {
	return TCL_ERROR;
    }

    dPtr = (DelCmd *)Tcl_Alloc(sizeof(DelCmd));
    dPtr->interp = interp;
    dPtr->deleteCmd = (char *)Tcl_Alloc(strlen(argv[3]) + 1);
    strcpy(dPtr->deleteCmd, argv[3]);

    Tcl_CreateCommand(child, argv[2], DelCmdProc, dPtr,
	    DelDeleteProc);
    return TCL_OK;
}

static int
DelCmdProc(
    void *clientData,	/* String result to return. */
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    DelCmd *dPtr = (DelCmd *) clientData;

    Tcl_AppendResult(interp, dPtr->deleteCmd, NULL);
    Tcl_Free(dPtr->deleteCmd);
    Tcl_Free(dPtr);
    return TCL_OK;
}

static void
DelDeleteProc(
    void *clientData)	/* String command to evaluate. */
{
    DelCmd *dPtr = (DelCmd *)clientData;

    Tcl_EvalEx(dPtr->interp, dPtr->deleteCmd, TCL_INDEX_NONE, 0);
    Tcl_ResetResult(dPtr->interp);
    Tcl_Free(dPtr->deleteCmd);
    Tcl_Free(dPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestdelassocdataCmd --
 *
 *	This procedure implements the "testdelassocdata" command. It is used
 *	to test Tcl_DeleteAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes an association between a key and associated data from an
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
TestdelassocdataCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" data_key\"", NULL);
	return TCL_ERROR;
    }
    Tcl_DeleteAssocData(interp, argv[1]);
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * TestdoubledigitsCmd --
 *
 *	This procedure implements the 'testdoubledigits' command. It is
 *	used to test the low-level floating-point formatting primitives
 *	in Tcl.
 *
 * Usage:
 *	testdoubledigits fpval ndigits type ?shorten"
 *
 * Parameters:
 *	fpval - Floating-point value to format.
 *	ndigits - Digit count to request from Tcl_DoubleDigits
 *	type - One of 'shortest', 'e', 'f'
 *	shorten - Indicates that the 'shorten' flag should be passed in.
 *
 *-----------------------------------------------------------------------------
 */

static int
TestdoubledigitsObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp* interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj* const objv[])	/* Parameter vector */
{
    static const char *options[] = {
	"shortest",
	"e",
	"f",
	NULL
    };
    static const int types[] = {
	TCL_DD_SHORTEST,
	TCL_DD_E_FORMAT,
	TCL_DD_F_FORMAT
    };

    const Tcl_ObjType* doubleType;
    double d;
    int status;
    int ndigits;
    int type;
    int decpt;
    int signum;
    char *str;
    char *endPtr;
    Tcl_Obj* strObj;
    Tcl_Obj* retval;

    if (objc < 4 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "fpval ndigits type ?shorten?");
	return TCL_ERROR;
    }
    status = Tcl_GetDoubleFromObj(interp, objv[1], &d);
    if (status != TCL_OK) {
	doubleType = Tcl_GetObjType("double");
	if (Tcl_FetchInternalRep(objv[1], doubleType)
	    && isnan(objv[1]->internalRep.doubleValue)) {
	    status = TCL_OK;
	    memcpy(&d, &(objv[1]->internalRep.doubleValue), sizeof(double));
	}
    }
    if (status != TCL_OK
	|| Tcl_GetIntFromObj(interp, objv[2], &ndigits) != TCL_OK
	|| Tcl_GetIndexFromObj(interp, objv[3], options, "conversion type",
			       TCL_EXACT, &type) != TCL_OK) {
	fprintf(stderr, "bad value? %g\n", d);
	return TCL_ERROR;
    }
    type = types[type];
    if (objc > 4) {
	if (strcmp(Tcl_GetString(objv[4]), "shorten")) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bad flag", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	type |= TCL_DD_SHORTEST;
    }
    str = TclDoubleDigits(d, ndigits, type, &decpt, &signum, &endPtr);
    strObj = Tcl_NewStringObj(str, endPtr-str);
    Tcl_Free(str);
    retval = Tcl_NewListObj(1, &strObj);
    Tcl_ListObjAppendElement(NULL, retval, Tcl_NewWideIntObj(decpt));
    strObj = Tcl_NewStringObj(signum ? "-" : "+", 1);
    Tcl_ListObjAppendElement(NULL, retval, strObj);
    Tcl_SetObjResult(interp, retval);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdstringCmd --
 *
 *	This procedure implements the "testdstring" command.  It is used
 *	to test the dynamic string facilities of Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes, and invokes handlers.
 *
 *----------------------------------------------------------------------
 */

static int
TestdstringCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int count;

    if (argc < 2) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "append") == 0) {
	if (argc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[3], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&dstring, argv[2], count);
    } else if (strcmp(argv[1], "element") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	Tcl_DStringAppendElement(&dstring, argv[2]);
    } else if (strcmp(argv[1], "end") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringEndSublist(&dstring);
    } else if (strcmp(argv[1], "free") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringFree(&dstring);
    } else if (strcmp(argv[1], "get") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_SetResult(interp, Tcl_DStringValue(&dstring), TCL_VOLATILE);
    } else if (strcmp(argv[1], "gresult") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (strcmp(argv[2], "staticsmall") == 0) {
	    Tcl_AppendResult(interp, "short", NULL);
	} else if (strcmp(argv[2], "staticlarge") == 0) {
	    Tcl_AppendResult(interp, "first0 first1 first2 first3 first4 first5 first6 first7 first8 first9\nsecond0 second1 second2 second3 second4 second5 second6 second7 second8 second9\nthird0 third1 third2 third3 third4 third5 third6 third7 third8 third9\nfourth0 fourth1 fourth2 fourth3 fourth4 fourth5 fourth6 fourth7 fourth8 fourth9\nfifth0 fifth1 fifth2 fifth3 fifth4 fifth5 fifth6 fifth7 fifth8 fifth9\nsixth0 sixth1 sixth2 sixth3 sixth4 sixth5 sixth6 sixth7 sixth8 sixth9\nseventh0 seventh1 seventh2 seventh3 seventh4 seventh5 seventh6 seventh7 seventh8 seventh9\n", NULL);
	} else if (strcmp(argv[2], "free") == 0) {
	    char *s = (char *)Tcl_Alloc(100);
	    strcpy(s, "This is a malloc-ed string");
	    Tcl_SetResult(interp, s, TCL_DYNAMIC);
	} else if (strcmp(argv[2], "special") == 0) {
	    char *s = (char *)Tcl_Alloc(100) + 16;
	    strcpy(s, "This is a specially-allocated string");
	    Tcl_SetResult(interp, s, SpecialFree);
	} else {
	    Tcl_AppendResult(interp, "bad gresult option \"", argv[2],
		    "\": must be staticsmall, staticlarge, free, or special",
		    NULL);
	    return TCL_ERROR;
	}
	Tcl_DStringGetResult(interp, &dstring);
    } else if (strcmp(argv[1], "length") == 0) {

	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(Tcl_DStringLength(&dstring)));
    } else if (strcmp(argv[1], "result") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringResult(interp, &dstring);
    } else if (strcmp(argv[1], "toobj") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_SetObjResult(interp, Tcl_DStringToObj(&dstring));
    } else if (strcmp(argv[1], "trunc") == 0) {
	if (argc != 3) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetInt(interp, argv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_DStringSetLength(&dstring, count);
    } else if (strcmp(argv[1], "start") == 0) {
	if (argc != 2) {
	    goto wrongNumArgs;
	}
	Tcl_DStringStartSublist(&dstring);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be append, element, end, free, get, gresult, length, "
		"result, start, toobj, or trunc", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * The procedure below is used as a special freeProc to test how well
 * Tcl_DStringGetResult handles freeProc's other than free.
 */

static void SpecialFree(
    void *blockPtr			/* Block to free. */
) {
    Tcl_Free(((char *)blockPtr) - 16);
}

/*
 *------------------------------------------------------------------------
 *
 * UtfTransformFn --
 *
 *    Implements a direct call into Tcl_UtfToExternal and Tcl_ExternalToUtf
 *    as otherwise there is no script level command that directly exercises
 *    these functions (i/o command cannot test all combinations)
 *    The arguments at the script level are roughly those of the above
 *    functions:
 *        encodingname srcbytes flags state dstlen ?srcreadvar? ?dstwrotevar? ?dstcharsvar?
 *
 * Results:
 *    TCL_OK or TCL_ERROR. This any errors running the test, NOT the
 *    result of Tcl_UtfToExternal or Tcl_ExternalToUtf.
 *
 * Side effects:
 *
 *    The result in the interpreter is a list of the return code from the
 *    Tcl_UtfToExternal/Tcl_ExternalToUtf functions, the encoding state, and
 *    an encoded binary string of length dstLen. Note the string is the
 *    entire output buffer, not just the part containing the decoded
 *    portion. This allows for additional checks at test script level.
 *
 *    If any of the srcreadvar, dstwrotevar and
 *    dstcharsvar are specified and not empty, they are treated as names
 *    of variables where the *srcRead, *dstWrote and *dstChars output
 *    from the functions are stored.
 *
 *    The function also checks internally whether nuls are correctly
 *    appended as requested but the TCL_ENCODING_NO_TERMINATE flag
 *    and that no buffer overflows occur.
 *------------------------------------------------------------------------
 */
typedef int
UtfTransformFn(Tcl_Interp *interp, Tcl_Encoding encoding, const char *src, Tcl_Size srcLen, int flags, Tcl_EncodingState *statePtr,
               char *dst, Tcl_Size dstLen, int *srcReadPtr, int *dstWrotePtr, int *dstCharsPtr);
static int UtfExtWrapper(
    Tcl_Interp *interp, UtfTransformFn *transformer, int objc, Tcl_Obj *const objv[])
{
    Tcl_Encoding encoding;
    Tcl_EncodingState encState, *encStatePtr;
    Tcl_Size srcLen, bufLen;
    const unsigned char *bytes;
    unsigned char *bufPtr;
    int srcRead, dstLen, dstWrote, dstChars;
    Tcl_Obj *srcReadVar, *dstWroteVar, *dstCharsVar;
    int result;
    int flags;
    Tcl_Obj **flagObjs;
    int nflags;
    static const struct {
	const char *flagKey;
	int flag;
    } flagMap[] = {
	{"start", TCL_ENCODING_START},
	{"end", TCL_ENCODING_END},
	{"stoponerror", TCL_ENCODING_STOPONERROR},
	{"noterminate", TCL_ENCODING_NO_TERMINATE},
	{"charlimit", TCL_ENCODING_CHAR_LIMIT},
	{"profiletcl8", TCL_ENCODING_PROFILE_TCL8},
	{"profilestrict", TCL_ENCODING_PROFILE_STRICT},
	{"profilereplace", TCL_ENCODING_PROFILE_REPLACE},
	{NULL, 0}
    };
    int i;
    Tcl_WideInt wide;

    if (objc < 7 || objc > 10) {
        Tcl_WrongNumArgs(interp,
                         2,
                         objv,
                         "encoding srcbytes flags state dstlen ?srcreadvar? ?dstwrotevar? ?dstcharsvar?");
        return TCL_ERROR;
    }
    if (Tcl_GetEncodingFromObj(interp, objv[2], &encoding) != TCL_OK) {
        return TCL_ERROR;
    }

    /* Flags may be specified as list of integers and keywords */
    flags = 0;
    if (Tcl_ListObjGetElements(interp, objv[4], &nflags, &flagObjs) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < nflags; ++i) {
	int flag;
	if (Tcl_GetIntFromObj(NULL, flagObjs[i], &flag) == TCL_OK) {
	    flags |= flag;
	} else {
	    int idx;
	    if (Tcl_GetIndexFromObjStruct(interp,
					  flagObjs[i],
					  flagMap,
					  sizeof(flagMap[0]),
					  "flag",
					  0,
					  &idx) != TCL_OK) {
		return TCL_ERROR;
	    }
	    flags |= flagMap[idx].flag;
	}
    }

    /* Assumes state is integer if not "" */
    if (Tcl_GetWideIntFromObj(interp, objv[5], &wide) == TCL_OK) {
        encState = (Tcl_EncodingState)(size_t)wide;
        encStatePtr = &encState;
    } else if (Tcl_GetCharLength(objv[5]) == 0) {
        encStatePtr = NULL;
    } else {
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[6], &dstLen) != TCL_OK) {
        return TCL_ERROR;
    }
    srcReadVar = NULL;
    dstWroteVar = NULL;
    dstCharsVar = NULL;
    if (objc > 7) {
	/* Has caller requested srcRead? */
	if (Tcl_GetCharLength(objv[7])) {
	    srcReadVar = objv[7];
	}
	if (objc > 8) {
	    /* Ditto for dstWrote */
            if (Tcl_GetCharLength(objv[8])) {
                dstWroteVar = objv[8];
            }
	    if (objc > 9) {
                if (Tcl_GetCharLength(objv[9])) {
                    dstCharsVar = objv[9];
		}
	    }
	}
    }
    if (flags & TCL_ENCODING_CHAR_LIMIT) {
	/* Caller should have specified the dest char limit */
	Tcl_Obj *valueObj;
	if (dstCharsVar == NULL ||
	    (valueObj = Tcl_ObjGetVar2(interp, dstCharsVar, NULL, 0)) == NULL
	) {
	    Tcl_SetResult(interp,
			 "dstCharsVar must be specified with integer value if "
			 "TCL_ENCODING_CHAR_LIMIT set in flags.", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, valueObj, &dstChars) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	dstChars = 0; /* Only used for output */
    }

    bufLen = dstLen + 4; /* 4 -> overflow detection */
    bufPtr = (unsigned char *) Tcl_Alloc(bufLen);
    memset(bufPtr, 0xFF, dstLen); /* Need to check nul terminator */
    memmove(bufPtr + dstLen, "\xAB\xCD\xEF\xAB", 4);   /* overflow detection */
    bytes = Tcl_GetByteArrayFromObj(objv[3], &srcLen); /* Last! to avoid shimmering */
    result = (*transformer)(interp, encoding, (const char *)bytes, srcLen, flags,
                            encStatePtr, (char *) bufPtr, dstLen,
                            srcReadVar ? &srcRead : NULL,
                            &dstWrote,
                            dstCharsVar ? &dstChars : NULL);
    if (memcmp(bufPtr + bufLen - 4, "\xAB\xCD\xEF\xAB", 4)) {
        Tcl_SetResult(interp,
                      "Tcl_ExternalToUtf wrote past output buffer",
                      TCL_STATIC);
        result = TCL_ERROR;
    } else if (result != TCL_ERROR) {
        Tcl_Obj *resultObjs[3];
        switch (result) {
        case TCL_OK:
            resultObjs[0] = Tcl_NewStringObj("ok", TCL_INDEX_NONE);
            break;
        case TCL_CONVERT_MULTIBYTE:
            resultObjs[0] = Tcl_NewStringObj("multibyte", TCL_INDEX_NONE);
            break;
        case TCL_CONVERT_SYNTAX:
            resultObjs[0] = Tcl_NewStringObj("syntax", TCL_INDEX_NONE);
            break;
        case TCL_CONVERT_UNKNOWN:
            resultObjs[0] = Tcl_NewStringObj("unknown", TCL_INDEX_NONE);
            break;
        case TCL_CONVERT_NOSPACE:
            resultObjs[0] = Tcl_NewStringObj("nospace", TCL_INDEX_NONE);
            break;
        default:
            resultObjs[0] = Tcl_NewIntObj(result);
            break;
        }
        result = TCL_OK;
        resultObjs[1] =
            encStatePtr ? Tcl_NewWideIntObj((Tcl_WideInt)(size_t)encState) : Tcl_NewObj();
        resultObjs[2] = Tcl_NewByteArrayObj(bufPtr, dstLen);
        if (srcReadVar) {
	    if (Tcl_ObjSetVar2(interp,
			       srcReadVar,
			       NULL,
			       Tcl_NewIntObj(srcRead),
			       TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
        if (dstWroteVar) {
	    if (Tcl_ObjSetVar2(interp,
			       dstWroteVar,
			       NULL,
			       Tcl_NewIntObj(dstWrote),
			       TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
        if (dstCharsVar) {
	    if (Tcl_ObjSetVar2(interp,
			       dstCharsVar,
			       NULL,
			       Tcl_NewIntObj(dstChars),
			       TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
        Tcl_SetObjResult(interp, Tcl_NewListObj(3, resultObjs));
    }

    Tcl_Free(bufPtr);
    Tcl_FreeEncoding(encoding); /* Free returned reference */
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TestencodingCmd --
 *
 *	This procedure implements the "testencoding" command.  It is used
 *	to test the encoding package.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Load encodings.
 *
 *----------------------------------------------------------------------
 */

static int
TestencodingObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Encoding encoding;
    size_t length;
    const char *string;
    TclEncoding *encodingPtr;
    static const char *const optionStrings[] = {
	"create", "delete", "nullength", "Tcl_ExternalToUtf", "Tcl_UtfToExternal", NULL
    };
    enum options {
	ENC_CREATE, ENC_DELETE, ENC_NULLENGTH, ENC_EXTTOUTF, ENC_UTFTOEXT
    } index;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?args?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case ENC_CREATE: {
	Tcl_EncodingType type;

	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "name toutfcmd fromutfcmd");
	    return TCL_ERROR;
	}
	encodingPtr = (TclEncoding *)Tcl_Alloc(sizeof(TclEncoding));
	encodingPtr->interp = interp;

	string = Tcl_GetStringFromObj(objv[3], &length);
	encodingPtr->toUtfCmd = (char *)Tcl_Alloc(length + 1);
	memcpy(encodingPtr->toUtfCmd, string, length + 1);

	string = Tcl_GetStringFromObj(objv[4], &length);
	encodingPtr->fromUtfCmd = (char *)Tcl_Alloc(length + 1);
	memcpy(encodingPtr->fromUtfCmd, string, length + 1);

	string = Tcl_GetStringFromObj(objv[2], &length);

	type.encodingName = string;
	type.toUtfProc = EncodingToUtfProc;
	type.fromUtfProc = EncodingFromUtfProc;
	type.freeProc = EncodingFreeProc;
	type.clientData = encodingPtr;
	type.nullSize = 1;

	Tcl_CreateEncoding(&type);
	break;
    }
    case ENC_DELETE:
	if (objc != 3) {
	    return TCL_ERROR;
	}
	if (TCL_OK != Tcl_GetEncodingFromObj(interp, objv[2], &encoding)) {
	    return TCL_ERROR;
	}
	Tcl_FreeEncoding(encoding);	/* Free returned reference */
	Tcl_FreeEncoding(encoding);	/* Free to match CREATE */
	TclFreeInternalRep(objv[2]);		/* Free the cached ref */
	break;

    case ENC_NULLENGTH:
	if (objc > 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?encoding?");
	    return TCL_ERROR;
	}
	encoding =
	    Tcl_GetEncoding(interp, objc == 2 ? NULL : Tcl_GetString(objv[2]));
	if (encoding == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp,
			 Tcl_NewIntObj(Tcl_GetEncodingNulLength(encoding)));
	Tcl_FreeEncoding(encoding);
        break;
    case ENC_EXTTOUTF:
        return UtfExtWrapper(interp,Tcl_ExternalToUtf,objc,objv);
    case ENC_UTFTOEXT:
        return UtfExtWrapper(interp,Tcl_UtfToExternal,objc,objv);
    }
    return TCL_OK;
}

static int
EncodingToUtfProc(
    void *clientData,	/* TclEncoding structure. */
    TCL_UNUSED(const char *) /*src*/,
    int srcLen,			/* Source string length in bytes. */
    TCL_UNUSED(int) /*flags*/,
    TCL_UNUSED(Tcl_EncodingState *),
    char *dst,			/* Output buffer. */
    int dstLen,			/* The maximum length of output buffer. */
    int *srcReadPtr,		/* Filled with number of bytes read. */
    int *dstWrotePtr,		/* Filled with number of bytes stored. */
    int *dstCharsPtr)		/* Filled with number of chars stored. */
{
    int len;
    TclEncoding *encodingPtr;

    encodingPtr = (TclEncoding *) clientData;
    Tcl_EvalEx(encodingPtr->interp, encodingPtr->toUtfCmd, TCL_INDEX_NONE, TCL_EVAL_GLOBAL);

    len = strlen(Tcl_GetStringResult(encodingPtr->interp));
    if (len > dstLen) {
	len = dstLen;
    }
    memcpy(dst, Tcl_GetStringResult(encodingPtr->interp), len);
    Tcl_ResetResult(encodingPtr->interp);

    *srcReadPtr = srcLen;
    *dstWrotePtr = len;
    *dstCharsPtr = len;
    return TCL_OK;
}

static int
EncodingFromUtfProc(
    void *clientData,	/* TclEncoding structure. */
    TCL_UNUSED(const char *) /*src*/,
    int srcLen,			/* Source string length in bytes. */
    TCL_UNUSED(int) /*flags*/,
    TCL_UNUSED(Tcl_EncodingState *),
    char *dst,			/* Output buffer. */
    int dstLen,			/* The maximum length of output buffer. */
    int *srcReadPtr,		/* Filled with number of bytes read. */
    int *dstWrotePtr,		/* Filled with number of bytes stored. */
    int *dstCharsPtr)		/* Filled with number of chars stored. */
{
    int len;
    TclEncoding *encodingPtr;

    encodingPtr = (TclEncoding *) clientData;
    Tcl_EvalEx(encodingPtr->interp, encodingPtr->fromUtfCmd, TCL_INDEX_NONE, TCL_EVAL_GLOBAL);

    len = strlen(Tcl_GetStringResult(encodingPtr->interp));
    if (len > dstLen) {
	len = dstLen;
    }
    memcpy(dst, Tcl_GetStringResult(encodingPtr->interp), len);
    Tcl_ResetResult(encodingPtr->interp);

    *srcReadPtr = srcLen;
    *dstWrotePtr = len;
    *dstCharsPtr = len;
    return TCL_OK;
}

static void
EncodingFreeProc(
    void *clientData)	/* ClientData associated with type. */
{
    TclEncoding *encodingPtr = (TclEncoding *)clientData;

    Tcl_Free(encodingPtr->toUtfCmd);
    Tcl_Free(encodingPtr->fromUtfCmd);
    Tcl_Free(encodingPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestevalexObjCmd --
 *
 *	This procedure implements the "testevalex" command.  It is
 *	used to test Tcl_EvalEx.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestevalexObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int length, flags;
    const char *script;

    flags = 0;
    if (objc == 3) {
	const char *global = Tcl_GetString(objv[2]);
	if (strcmp(global, "global") != 0) {
	    Tcl_AppendResult(interp, "bad value \"", global,
		    "\": must be global", NULL);
	    return TCL_ERROR;
	}
	flags = TCL_EVAL_GLOBAL;
    } else if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "script ?global?");
	return TCL_ERROR;
    }

    script = Tcl_GetStringFromObj(objv[1], &length);
    return Tcl_EvalEx(interp, script, length, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * TestevalobjvObjCmd --
 *
 *	This procedure implements the "testevalobjv" command.  It is
 *	used to test Tcl_EvalObjv.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestevalobjvObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int evalGlobal;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "global word ?word ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[1], &evalGlobal) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tcl_EvalObjv(interp, objc-2, objv+2,
	    (evalGlobal) ? TCL_EVAL_GLOBAL : 0);
}

/*
 *----------------------------------------------------------------------
 *
 * TesteventObjCmd --
 *
 *	This procedure implements a 'testevent' command.  The command
 *	is used to test event queue management.
 *
 * The command takes two forms:
 *	- testevent queue name position script
 *		Queues an event at the given position in the queue, and
 *		associates a given name with it (the same name may be
 *		associated with multiple events). When the event comes
 *		to the head of the queue, executes the given script at
 *		global level in the current interp. The position may be
 *		one of 'head', 'tail' or 'mark'.
 *	- testevent delete name
 *		Deletes any events associated with the given name from
 *		the queue.
 *
 * Return value:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Manipulates the event queue as directed.
 *
 *----------------------------------------------------------------------
 */

static int
TesteventObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const objv[])	/* Parameter vector */
{
    static const char *const subcommands[] = { /* Possible subcommands */
	"queue", "delete", NULL
    };
    int subCmdIndex;		/* Index of the chosen subcommand */
    static const char *const positions[] = { /* Possible queue positions */
	"head", "tail", "mark", NULL
    };
    int posIndex;		/* Index of the chosen position */
    static const int posNum[] = {
				/* Interpretation of the chosen position */
	TCL_QUEUE_HEAD,
	TCL_QUEUE_TAIL,
	TCL_QUEUE_MARK
    };
    TestEvent *ev;		/* Event to be queued */

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subcommands, "subcommand",
	    TCL_EXACT, &subCmdIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (subCmdIndex) {
    case 0:			/* queue */
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "name position script");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[3], positions,
		"position specifier", TCL_EXACT, &posIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	ev = (TestEvent *)Tcl_Alloc(sizeof(TestEvent));
	ev->header.proc = TesteventProc;
	ev->header.nextPtr = NULL;
	ev->interp = interp;
	ev->command = objv[4];
	Tcl_IncrRefCount(ev->command);
	ev->tag = objv[2];
	Tcl_IncrRefCount(ev->tag);
	Tcl_QueueEvent((Tcl_Event *) ev, posNum[posIndex]);
	break;

    case 1:			/* delete */
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "name");
	    return TCL_ERROR;
	}
	Tcl_DeleteEvents(TesteventDeleteProc, objv[2]);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TesteventProc --
 *
 *	Delivers a test event to the Tcl interpreter as part of event
 *	queue testing.
 *
 * Results:
 *	Returns 1 if the event has been serviced, 0 otherwise.
 *
 * Side effects:
 *	Evaluates the event's callback script, so has whatever side effects
 *	the callback has.  The return value of the callback script becomes the
 *	return value of this function.  If the callback script reports an
 *	error, it is reported as a background error.
 *
 *----------------------------------------------------------------------
 */

static int
TesteventProc(
    Tcl_Event *event,		/* Event to deliver */
    TCL_UNUSED(int) /*flags*/)
{
    TestEvent *ev = (TestEvent *) event;
    Tcl_Interp *interp = ev->interp;
    Tcl_Obj *command = ev->command;
    int result = Tcl_EvalObjEx(interp, command,
	    TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    int retval;

    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"    (command bound to \"testevent\" callback)");
	Tcl_BackgroundException(interp, TCL_ERROR);
	return 1;		/* Avoid looping on errors */
    }
    if (Tcl_GetBooleanFromObj(interp, Tcl_GetObjResult(interp),
	    &retval) != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"    (return value from \"testevent\" callback)");
	Tcl_BackgroundException(interp, TCL_ERROR);
	return 1;
    }
    if (retval) {
	Tcl_DecrRefCount(ev->tag);
	Tcl_DecrRefCount(ev->command);
    }

    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * TesteventDeleteProc --
 *
 *	Removes some set of events from the queue.
 *
 * This procedure is used as part of testing event queue management.
 *
 * Results:
 *	Returns 1 if a given event should be deleted, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TesteventDeleteProc(
    Tcl_Event *event,		/* Event to examine */
    void *clientData)	/* Tcl_Obj containing the name of the event(s)
				 * to remove */
{
    TestEvent *ev;		/* Event to examine */
    const char *evNameStr;
    Tcl_Obj *targetName;	/* Name of the event(s) to delete */
    const char *targetNameStr;

    if (event->proc != TesteventProc) {
	return 0;
    }
    targetName = (Tcl_Obj *) clientData;
    targetNameStr = (char *) Tcl_GetString(targetName);
    ev = (TestEvent *) event;
    evNameStr = Tcl_GetString(ev->tag);
    if (strcmp(evNameStr, targetNameStr) == 0) {
	Tcl_DecrRefCount(ev->tag);
	Tcl_DecrRefCount(ev->command);
	return 1;
    } else {
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestexithandlerCmd --
 *
 *	This procedure implements the "testexithandler" command. It is
 *	used to test Tcl_CreateExitHandler and Tcl_DeleteExitHandler.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexithandlerCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int value;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" create|delete value\"", NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	Tcl_CreateExitHandler((value & 1) ? ExitProcOdd : ExitProcEven,
		INT2PTR(value));
    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_DeleteExitHandler((value & 1) ? ExitProcOdd : ExitProcEven,
		INT2PTR(value));
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be create or delete", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static void
ExitProcOdd(
    void *clientData)	/* Integer value to print. */
{
    char buf[16 + TCL_INTEGER_SPACE];
    int len;

    snprintf(buf, sizeof(buf), "odd %d\n", (int)PTR2INT(clientData));
    len = strlen(buf);
    if (len != (int) write(1, buf, len)) {
	Tcl_Panic("ExitProcOdd: unable to write to stdout");
    }
}

static void
ExitProcEven(
    void *clientData)	/* Integer value to print. */
{
    char buf[16 + TCL_INTEGER_SPACE];
    int len;

    snprintf(buf, sizeof(buf), "even %d\n", (int)PTR2INT(clientData));
    len = strlen(buf);
    if (len != (int) write(1, buf, len)) {
	Tcl_Panic("ExitProcEven: unable to write to stdout");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprlongCmd --
 *
 *	This procedure verifies that Tcl_ExprLong does not modify the
 *	interpreter result if there is no error.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprlongCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    long exprResult;
    char buf[4 + TCL_INTEGER_SPACE];
    int result;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" expression\"", NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "This is a result", NULL);
    result = Tcl_ExprLong(interp, argv[1], &exprResult);
    if (result != TCL_OK) {
	return result;
    }
    snprintf(buf, sizeof(buf), ": %ld", exprResult);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprlongobjCmd --
 *
 *	This procedure verifies that Tcl_ExprLongObj does not modify the
 *	interpreter result if there is no error.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprlongobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    long exprResult;
    char buf[4 + TCL_INTEGER_SPACE];
    int result;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "expression");
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "This is a result", NULL);
    result = Tcl_ExprLongObj(interp, objv[1], &exprResult);
    if (result != TCL_OK) {
	return result;
    }
    snprintf(buf, sizeof(buf), ": %ld", exprResult);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprdoubleCmd --
 *
 *	This procedure verifies that Tcl_ExprDouble does not modify the
 *	interpreter result if there is no error.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprdoubleCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    double exprResult;
    char buf[4 + TCL_DOUBLE_SPACE];
    int result;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" expression\"", NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "This is a result", NULL);
    result = Tcl_ExprDouble(interp, argv[1], &exprResult);
    if (result != TCL_OK) {
	return result;
    }
    strcpy(buf, ": ");
    Tcl_PrintDouble(interp, exprResult, buf+2);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprdoubleobjCmd --
 *
 *	This procedure verifies that Tcl_ExprLongObj does not modify the
 *	interpreter result if there is no error.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprdoubleobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument objects. */
{
    double exprResult;
    char buf[4 + TCL_DOUBLE_SPACE];
    int result;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "expression");
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "This is a result", NULL);
    result = Tcl_ExprDoubleObj(interp, objv[1], &exprResult);
    if (result != TCL_OK) {
	return result;
    }
    strcpy(buf, ": ");
    Tcl_PrintDouble(interp, exprResult, buf+2);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprstringCmd --
 *
 *	This procedure tests the basic operation of Tcl_ExprString.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprstringCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" expression\"", NULL);
	return TCL_ERROR;
    }
    return Tcl_ExprString(interp, argv[1]);
}

/*
 *----------------------------------------------------------------------
 *
 * TestfilelinkCmd --
 *
 *	This procedure implements the "testfilelink" command.  It is used to
 *	test the effects of creating and manipulating filesystem links in Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May create a link on disk.
 *
 *----------------------------------------------------------------------
 */

static int
TestfilelinkCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    Tcl_Obj *contents;

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "source ?target?");
	return TCL_ERROR;
    }

    if (Tcl_FSConvertToPathType(interp, objv[1]) != TCL_OK) {
	return TCL_ERROR;
    }

    if (objc == 3) {
	/* Create link from source to target */
	contents = Tcl_FSLink(objv[1], objv[2],
		TCL_CREATE_SYMBOLIC_LINK|TCL_CREATE_HARD_LINK);
	if (contents == NULL) {
	    Tcl_AppendResult(interp, "could not create link from \"",
		    Tcl_GetString(objv[1]), "\" to \"",
		    Tcl_GetString(objv[2]), "\": ",
		    Tcl_PosixError(interp), NULL);
	    return TCL_ERROR;
	}
    } else {
	/* Read link */
	contents = Tcl_FSLink(objv[1], NULL, 0);
	if (contents == NULL) {
	    Tcl_AppendResult(interp, "could not read link \"",
		    Tcl_GetString(objv[1]), "\": ",
		    Tcl_PosixError(interp), NULL);
	    return TCL_ERROR;
	}
    }
    Tcl_SetObjResult(interp, contents);
    if (objc == 2) {
	/*
	 * If we are creating a link, this will actually just
	 * be objv[3], and we don't own it
	 */
	Tcl_DecrRefCount(contents);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetassocdataCmd --
 *
 *	This procedure implements the "testgetassocdata" command. It is
 *	used to test Tcl_GetAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetassocdataCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    char *res;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" data_key\"", NULL);
	return TCL_ERROR;
    }
    res = (char *) Tcl_GetAssocData(interp, argv[1], NULL);
    if (res != NULL) {
	Tcl_AppendResult(interp, res, NULL);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetplatformCmd --
 *
 *	This procedure implements the "testgetplatform" command. It is
 *	used to retrievel the value of the tclPlatform global variable.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetplatformCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    static const char *const platformStrings[] = { "unix", "mac", "windows" };
    TclPlatformType *platform;

    platform = TclGetPlatform();

    if (argc != 1) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		NULL);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, platformStrings[*platform], NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestinterpdeleteCmd --
 *
 *	This procedure tests the code in tclInterp.c that deals with
 *	interpreter deletion. It deletes a user-specified interpreter
 *	from the hierarchy, and subsequent code checks integrity.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes one or more interpreters.
 *
 *----------------------------------------------------------------------
 */

static int
TestinterpdeleteCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tcl_Interp *childToDelete;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" path\"", NULL);
	return TCL_ERROR;
    }
    childToDelete = Tcl_GetChild(interp, argv[1]);
    if (childToDelete == NULL) {
	return TCL_ERROR;
    }
    Tcl_DeleteInterp(childToDelete);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestlinkCmd --
 *
 *	This procedure implements the "testlink" command.  It is used
 *	to test Tcl_LinkVar and related library procedures.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes various variable links, plus returns
 *	values of the linked variables.
 *
 *----------------------------------------------------------------------
 */

static int
TestlinkCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    static int intVar = 43;
    static int boolVar = 4;
    static double realVar = 1.23;
    static Tcl_WideInt wideVar = 79;
    static char *stringVar = NULL;
    static char charVar = '@';
    static unsigned char ucharVar = 130;
    static short shortVar = 3000;
    static unsigned short ushortVar = 60000;
    static unsigned int uintVar = 0xBEEFFEED;
    static long longVar = 123456789L;
    static unsigned long ulongVar = 3456789012UL;
    static float floatVar = 4.5;
    static Tcl_WideUInt uwideVar = 123;
    static int created = 0;
    char buffer[2*TCL_DOUBLE_SPACE];
    int writable, flag;
    Tcl_Obj *tmp;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg arg arg arg arg arg arg arg arg arg arg"
		" arg arg?\"", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "create") == 0) {
	if (argc != 16) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ", argv[1],
		" intRO realRO boolRO stringRO wideRO charRO ucharRO shortRO"
		" ushortRO uintRO longRO ulongRO floatRO uwideRO\"", NULL);
	    return TCL_ERROR;
	}
	if (created) {
	    Tcl_UnlinkVar(interp, "int");
	    Tcl_UnlinkVar(interp, "real");
	    Tcl_UnlinkVar(interp, "bool");
	    Tcl_UnlinkVar(interp, "string");
	    Tcl_UnlinkVar(interp, "wide");
	    Tcl_UnlinkVar(interp, "char");
	    Tcl_UnlinkVar(interp, "uchar");
	    Tcl_UnlinkVar(interp, "short");
	    Tcl_UnlinkVar(interp, "ushort");
	    Tcl_UnlinkVar(interp, "uint");
	    Tcl_UnlinkVar(interp, "long");
	    Tcl_UnlinkVar(interp, "ulong");
	    Tcl_UnlinkVar(interp, "float");
	    Tcl_UnlinkVar(interp, "uwide");
	}
	created = 1;
	if (Tcl_GetBoolean(interp, argv[2], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "int", &intVar,
		TCL_LINK_INT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[3], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "real", &realVar,
		TCL_LINK_DOUBLE | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[4], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "bool", &boolVar,
		TCL_LINK_BOOLEAN | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[5], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "string", &stringVar,
		TCL_LINK_STRING | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[6], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "wide", &wideVar,
			TCL_LINK_WIDE_INT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[7], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "char", &charVar,
		TCL_LINK_CHAR | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[8], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "uchar", &ucharVar,
		TCL_LINK_UCHAR | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[9], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "short", &shortVar,
		TCL_LINK_SHORT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[10], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "ushort", &ushortVar,
		TCL_LINK_USHORT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[11], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "uint", &uintVar,
		TCL_LINK_UINT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[12], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "long", &longVar,
		TCL_LINK_LONG | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[13], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "ulong", &ulongVar,
		TCL_LINK_ULONG | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[14], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "float", &floatVar,
		TCL_LINK_FLOAT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBoolean(interp, argv[15], &writable) != TCL_OK) {
	    return TCL_ERROR;
	}
	flag = writable ? 0 : TCL_LINK_READ_ONLY;
	if (Tcl_LinkVar(interp, "uwide", &uwideVar,
		TCL_LINK_WIDE_UINT | flag) != TCL_OK) {
	    return TCL_ERROR;
	}

    } else if (strcmp(argv[1], "delete") == 0) {
	Tcl_UnlinkVar(interp, "int");
	Tcl_UnlinkVar(interp, "real");
	Tcl_UnlinkVar(interp, "bool");
	Tcl_UnlinkVar(interp, "string");
	Tcl_UnlinkVar(interp, "wide");
	Tcl_UnlinkVar(interp, "char");
	Tcl_UnlinkVar(interp, "uchar");
	Tcl_UnlinkVar(interp, "short");
	Tcl_UnlinkVar(interp, "ushort");
	Tcl_UnlinkVar(interp, "uint");
	Tcl_UnlinkVar(interp, "long");
	Tcl_UnlinkVar(interp, "ulong");
	Tcl_UnlinkVar(interp, "float");
	Tcl_UnlinkVar(interp, "uwide");
	created = 0;
    } else if (strcmp(argv[1], "get") == 0) {
	TclFormatInt(buffer, intVar);
	Tcl_AppendElement(interp, buffer);
	Tcl_PrintDouble(NULL, realVar, buffer);
	Tcl_AppendElement(interp, buffer);
	TclFormatInt(buffer, boolVar);
	Tcl_AppendElement(interp, buffer);
	Tcl_AppendElement(interp, (stringVar == NULL) ? "-" : stringVar);
	/*
	 * Wide ints only have an object-based interface.
	 */
	tmp = Tcl_NewWideIntObj(wideVar);
	Tcl_AppendElement(interp, Tcl_GetString(tmp));
	Tcl_DecrRefCount(tmp);
	TclFormatInt(buffer, (int) charVar);
	Tcl_AppendElement(interp, buffer);
	TclFormatInt(buffer, (int) ucharVar);
	Tcl_AppendElement(interp, buffer);
	TclFormatInt(buffer, (int) shortVar);
	Tcl_AppendElement(interp, buffer);
	TclFormatInt(buffer, (int) ushortVar);
	Tcl_AppendElement(interp, buffer);
	TclFormatInt(buffer, (int) uintVar);
	Tcl_AppendElement(interp, buffer);
	tmp = Tcl_NewWideIntObj(longVar);
	Tcl_AppendElement(interp, Tcl_GetString(tmp));
	Tcl_DecrRefCount(tmp);
#ifdef TCL_WIDE_INT_IS_LONG
	if (ulongVar > WIDE_MAX) {
		mp_int bignumValue;
		if (mp_init_u64(&bignumValue, ulongVar) != MP_OKAY) {
		    Tcl_Panic("%s: memory overflow", "Tcl_SetWideUIntObj");
		}
		tmp = Tcl_NewBignumObj(&bignumValue);
	} else
#endif /* TCL_WIDE_INT_IS_LONG */
	tmp = Tcl_NewWideIntObj((Tcl_WideInt)ulongVar);
	Tcl_AppendElement(interp, Tcl_GetString(tmp));
	Tcl_DecrRefCount(tmp);
	Tcl_PrintDouble(NULL, (double)floatVar, buffer);
	Tcl_AppendElement(interp, buffer);
	if (uwideVar > WIDE_MAX) {
		mp_int bignumValue;
		if (mp_init_u64(&bignumValue, uwideVar) != MP_OKAY) {
		    Tcl_Panic("%s: memory overflow", "Tcl_SetWideUIntObj");
		}
		tmp = Tcl_NewBignumObj(&bignumValue);
	} else {
	    tmp = Tcl_NewWideIntObj((Tcl_WideInt)uwideVar);
	}
	Tcl_AppendElement(interp, Tcl_GetString(tmp));
	Tcl_DecrRefCount(tmp);
    } else if (strcmp(argv[1], "set") == 0) {
	int v;

	if (argc != 16) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1],
		    " intValue realValue boolValue stringValue wideValue"
		    " charValue ucharValue shortValue ushortValue uintValue"
		    " longValue ulongValue floatValue uwideValue\"", NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] != 0) {
	    if (Tcl_GetInt(interp, argv[2], &intVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[3][0] != 0) {
	    if (Tcl_GetDouble(interp, argv[3], &realVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[4][0] != 0) {
	    if (Tcl_GetInt(interp, argv[4], &boolVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (argv[5][0] != 0) {
	    if (stringVar != NULL) {
		Tcl_Free(stringVar);
	    }
	    if (strcmp(argv[5], "-") == 0) {
		stringVar = NULL;
	    } else {
		stringVar = (char *)Tcl_Alloc(strlen(argv[5]) + 1);
		strcpy(stringVar, argv[5]);
	    }
	}
	if (argv[6][0] != 0) {
	    tmp = Tcl_NewStringObj(argv[6], TCL_INDEX_NONE);
	    if (Tcl_GetWideIntFromObj(interp, tmp, &wideVar) != TCL_OK) {
		Tcl_DecrRefCount(tmp);
		return TCL_ERROR;
	    }
	    Tcl_DecrRefCount(tmp);
	}
	if (argv[7][0]) {
	    if (Tcl_GetInt(interp, argv[7], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    charVar = (char) v;
	}
	if (argv[8][0]) {
	    if (Tcl_GetInt(interp, argv[8], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ucharVar = (unsigned char) v;
	}
	if (argv[9][0]) {
	    if (Tcl_GetInt(interp, argv[9], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    shortVar = (short) v;
	}
	if (argv[10][0]) {
	    if (Tcl_GetInt(interp, argv[10], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ushortVar = (unsigned short) v;
	}
	if (argv[11][0]) {
	    if (Tcl_GetInt(interp, argv[11], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    uintVar = (unsigned int) v;
	}
	if (argv[12][0]) {
	    if (Tcl_GetInt(interp, argv[12], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    longVar = (long) v;
	}
	if (argv[13][0]) {
	    if (Tcl_GetInt(interp, argv[13], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ulongVar = (unsigned long) v;
	}
	if (argv[14][0]) {
	    double d;
	    if (Tcl_GetDouble(interp, argv[14], &d) != TCL_OK) {
		return TCL_ERROR;
	    }
	    floatVar = (float) d;
	}
	if (argv[15][0]) {
	    Tcl_WideInt w;
	    tmp = Tcl_NewStringObj(argv[15], TCL_INDEX_NONE);
	    if (Tcl_GetWideIntFromObj(interp, tmp, &w) != TCL_OK) {
		Tcl_DecrRefCount(tmp);
		return TCL_ERROR;
	    }
	    Tcl_DecrRefCount(tmp);
	    uwideVar = (Tcl_WideUInt) w;
	}
    } else if (strcmp(argv[1], "update") == 0) {
	int v;

	if (argc != 16) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1],
		    " intValue realValue boolValue stringValue wideValue"
		    " charValue ucharValue shortValue ushortValue uintValue"
		    " longValue ulongValue floatValue uwideValue\"", NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] != 0) {
	    if (Tcl_GetInt(interp, argv[2], &intVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "int");
	}
	if (argv[3][0] != 0) {
	    if (Tcl_GetDouble(interp, argv[3], &realVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "real");
	}
	if (argv[4][0] != 0) {
	    if (Tcl_GetInt(interp, argv[4], &boolVar) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_UpdateLinkedVar(interp, "bool");
	}
	if (argv[5][0] != 0) {
	    if (stringVar != NULL) {
		Tcl_Free(stringVar);
	    }
	    if (strcmp(argv[5], "-") == 0) {
		stringVar = NULL;
	    } else {
		stringVar = (char *)Tcl_Alloc(strlen(argv[5]) + 1);
		strcpy(stringVar, argv[5]);
	    }
	    Tcl_UpdateLinkedVar(interp, "string");
	}
	if (argv[6][0] != 0) {
	    tmp = Tcl_NewStringObj(argv[6], TCL_INDEX_NONE);
	    if (Tcl_GetWideIntFromObj(interp, tmp, &wideVar) != TCL_OK) {
		Tcl_DecrRefCount(tmp);
		return TCL_ERROR;
	    }
	    Tcl_DecrRefCount(tmp);
	    Tcl_UpdateLinkedVar(interp, "wide");
	}
	if (argv[7][0]) {
	    if (Tcl_GetInt(interp, argv[7], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    charVar = (char) v;
	    Tcl_UpdateLinkedVar(interp, "char");
	}
	if (argv[8][0]) {
	    if (Tcl_GetInt(interp, argv[8], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ucharVar = (unsigned char) v;
	    Tcl_UpdateLinkedVar(interp, "uchar");
	}
	if (argv[9][0]) {
	    if (Tcl_GetInt(interp, argv[9], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    shortVar = (short) v;
	    Tcl_UpdateLinkedVar(interp, "short");
	}
	if (argv[10][0]) {
	    if (Tcl_GetInt(interp, argv[10], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ushortVar = (unsigned short) v;
	    Tcl_UpdateLinkedVar(interp, "ushort");
	}
	if (argv[11][0]) {
	    if (Tcl_GetInt(interp, argv[11], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    uintVar = (unsigned int) v;
	    Tcl_UpdateLinkedVar(interp, "uint");
	}
	if (argv[12][0]) {
	    if (Tcl_GetInt(interp, argv[12], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    longVar = (long) v;
	    Tcl_UpdateLinkedVar(interp, "long");
	}
	if (argv[13][0]) {
	    if (Tcl_GetInt(interp, argv[13], &v) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ulongVar = (unsigned long) v;
	    Tcl_UpdateLinkedVar(interp, "ulong");
	}
	if (argv[14][0]) {
	    double d;
	    if (Tcl_GetDouble(interp, argv[14], &d) != TCL_OK) {
		return TCL_ERROR;
	    }
	    floatVar = (float) d;
	    Tcl_UpdateLinkedVar(interp, "float");
	}
	if (argv[15][0]) {
	    Tcl_WideInt w;
	    tmp = Tcl_NewStringObj(argv[15], TCL_INDEX_NONE);
	    if (Tcl_GetWideIntFromObj(interp, tmp, &w) != TCL_OK) {
		Tcl_DecrRefCount(tmp);
		return TCL_ERROR;
	    }
	    Tcl_DecrRefCount(tmp);
	    uwideVar = (Tcl_WideUInt) w;
	    Tcl_UpdateLinkedVar(interp, "uwide");
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": should be create, delete, get, set, or update", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestlinkarrayCmd --
 *
 *      This function is invoked to process the "testlinkarray" Tcl command.
 *      It is used to test the 'Tcl_LinkArray' function.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes, and invokes variable links.
 *
 *----------------------------------------------------------------------
 */

static int
TestlinkarrayCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,         /* Current interpreter. */
    int objc,                   /* Number of arguments. */
    Tcl_Obj *const objv[])      /* Argument objects. */
{
    static const char *LinkOption[] = {
        "update", "remove", "create", NULL
    };
    enum LinkOptionEnum {LINK_UPDATE, LINK_REMOVE, LINK_CREATE} optionIndex;
    static const char *LinkType[] = {
	"char", "uchar", "short", "ushort", "int", "uint", "long", "ulong",
	"wide", "uwide", "float", "double", "string", "char*", "binary", NULL
    };
    /* all values after TCL_LINK_CHARS_ARRAY are used as arrays (see below) */
    static int LinkTypes[] = {
	TCL_LINK_CHAR, TCL_LINK_UCHAR,
	TCL_LINK_SHORT, TCL_LINK_USHORT, TCL_LINK_INT, TCL_LINK_UINT,
	TCL_LINK_LONG, TCL_LINK_ULONG, TCL_LINK_WIDE_INT, TCL_LINK_WIDE_UINT,
	TCL_LINK_FLOAT, TCL_LINK_DOUBLE, TCL_LINK_STRING, TCL_LINK_CHARS,
	TCL_LINK_BINARY
    };
    int typeIndex, readonly, i, size, length;
    char *name, *arg;
    Tcl_WideInt addr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option args");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], LinkOption, "option", 0,
	    &optionIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (optionIndex) {
    case LINK_UPDATE:
	for (i=2; i<objc; i++) {
	    Tcl_UpdateLinkedVar(interp, Tcl_GetString(objv[i]));
	}
	return TCL_OK;
    case LINK_REMOVE:
	for (i=2; i<objc; i++) {
	    Tcl_UnlinkVar(interp, Tcl_GetString(objv[i]));
	}
	return TCL_OK;
    case LINK_CREATE:
	if (objc < 4) {
	    goto wrongArgs;
	}
	readonly = 0;
	i = 2;

	/*
	 * test on switch -r...
	 */

	arg = Tcl_GetStringFromObj(objv[i], &length);
	if (length < 2) {
	    goto wrongArgs;
	}
	if (arg[0] == '-') {
	    if (arg[1] != 'r') {
		goto wrongArgs;
	    }
	    readonly = TCL_LINK_READ_ONLY;
	    i++;
	}
	if (Tcl_GetIndexFromObj(interp, objv[i++], LinkType, "type", 0,
 		&typeIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[i++], &size) == TCL_ERROR) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong size value", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	name = Tcl_GetString(objv[i++]);

	/*
	 * If no address is given request one in the underlying function
	 */

	if (i < objc) {
	    if (Tcl_GetWideIntFromObj(interp, objv[i], &addr) == TCL_ERROR) {
 		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"wrong address value", TCL_INDEX_NONE));
		return TCL_ERROR;
	    }
	} else {
	    addr = 0;
	}
	return Tcl_LinkArray(interp, name, INT2PTR(addr),
		LinkTypes[typeIndex] | readonly, size);
    }
    return TCL_OK;

  wrongArgs:
    Tcl_WrongNumArgs(interp, 2, objv, "?-readonly? type size name ?address?");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestlistrepCmd --
 *
 *      This function is invoked to generate a list object with a specific
 *	internal representation.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestlistrepCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,         /* Current interpreter. */
    int objc,                   /* Number of arguments. */
    Tcl_Obj *const objv[])      /* Argument objects. */
{
    /* Subcommands supported by this command */
    const char* subcommands[] = {
	"new",
	"describe",
	"config",
	"validate",
	NULL
    };
    enum {
	LISTREP_NEW,
	LISTREP_DESCRIBE,
	LISTREP_CONFIG,
	LISTREP_VALIDATE
    } cmdIndex;
    Tcl_Obj *resultObj = NULL;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(
	    interp, objv[1], subcommands, "command", 0, &cmdIndex)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    switch (cmdIndex) {
    case LISTREP_NEW:
	if (objc < 3 || objc > 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "length ?leadSpace endSpace?");
	    return TCL_ERROR;
	} else {
	    Tcl_WideUInt length;
	    Tcl_WideUInt leadSpace = 0;
	    Tcl_WideUInt endSpace = 0;
	    if (Tcl_GetWideUIntFromObj(interp, objv[2], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (objc > 3) {
		if (Tcl_GetWideUIntFromObj(interp, objv[3], &leadSpace) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (objc > 4) {
		    if (Tcl_GetWideUIntFromObj(interp, objv[4], &endSpace)
			!= TCL_OK) {
			return TCL_ERROR;
		    }
		}
	    }
	    resultObj = TclListTestObj(length, leadSpace, endSpace);
	    if (resultObj == NULL) {
		Tcl_AppendResult(interp, "List capacity exceeded", NULL);
		return TCL_ERROR;
	    }
	}
	break;

    case LISTREP_DESCRIBE:
#define APPEND_FIELD(targetObj_, structPtr_, fld_)                        \
    do {                                                                  \
	Tcl_ListObjAppendElement(                                         \
	    interp, (targetObj_), Tcl_NewStringObj(#fld_, TCL_INDEX_NONE));           \
	Tcl_ListObjAppendElement(                                         \
	    interp, (targetObj_), Tcl_NewWideIntObj((structPtr_)->fld_)); \
    } while (0)
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "object");
	    return TCL_ERROR;
	} else {
	    Tcl_Obj **objs;
	    Tcl_Size nobjs;
	    ListRep listRep;
	    Tcl_Obj *listRepObjs[4];

	    /* Force list representation */
	    if (Tcl_ListObjGetElements(interp, objv[2], &nobjs, &objs) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ListObjGetRep(objv[2], &listRep);
	    listRepObjs[0] = Tcl_NewStringObj("store", TCL_INDEX_NONE);
	    listRepObjs[1] = Tcl_NewListObj(12, NULL);
	    Tcl_ListObjAppendElement(
		interp, listRepObjs[1], Tcl_NewStringObj("memoryAddress", TCL_INDEX_NONE));
	    Tcl_ListObjAppendElement(
		interp, listRepObjs[1], Tcl_ObjPrintf("%p", listRep.storePtr));
	    APPEND_FIELD(listRepObjs[1], listRep.storePtr, firstUsed);
	    APPEND_FIELD(listRepObjs[1], listRep.storePtr, numUsed);
	    APPEND_FIELD(listRepObjs[1], listRep.storePtr, numAllocated);
	    APPEND_FIELD(listRepObjs[1], listRep.storePtr, refCount);
	    APPEND_FIELD(listRepObjs[1], listRep.storePtr, flags);
	    if (listRep.spanPtr) {
		listRepObjs[2] = Tcl_NewStringObj("span", TCL_INDEX_NONE);
		listRepObjs[3] = Tcl_NewListObj(8, NULL);
		Tcl_ListObjAppendElement(interp,
					 listRepObjs[3],
					 Tcl_NewStringObj("memoryAddress", TCL_INDEX_NONE));
		Tcl_ListObjAppendElement(
		    interp, listRepObjs[3], Tcl_ObjPrintf("%p", listRep.spanPtr));
		APPEND_FIELD(listRepObjs[3], listRep.spanPtr, spanStart);
		APPEND_FIELD(
		    listRepObjs[3], listRep.spanPtr, spanLength);
		APPEND_FIELD(listRepObjs[3], listRep.spanPtr, refCount);
	    }
	    resultObj = Tcl_NewListObj(listRep.spanPtr ? 4 : 2, listRepObjs);
	}
#undef APPEND_FIELD
	break;

    case LISTREP_CONFIG:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, "object");
	    return TCL_ERROR;
	}
	resultObj = Tcl_NewListObj(2, NULL);
	Tcl_ListObjAppendElement(
	    NULL, resultObj, Tcl_NewStringObj("LIST_SPAN_THRESHOLD", TCL_INDEX_NONE));
	Tcl_ListObjAppendElement(
	    NULL, resultObj, Tcl_NewWideIntObj(LIST_SPAN_THRESHOLD));
	break;

    case LISTREP_VALIDATE:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "object");
	    return TCL_ERROR;
	}
	TclListObjValidate(interp, objv[2]); /* Panics if invalid */
	resultObj = Tcl_NewObj();
	break;
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestlocaleCmd --
 *
 *	This procedure implements the "testlocale" command.  It is used
 *	to test the effects of setting different locales in Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies the current C locale.
 *
 *----------------------------------------------------------------------
 */

static int
TestlocaleCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    int index;
    const char *locale;
    static const char *const optionStrings[] = {
	"ctype", "numeric", "time", "collate", "monetary",
	"all",	NULL
    };
    static const int lcTypes[] = {
	LC_CTYPE, LC_NUMERIC, LC_TIME, LC_COLLATE, LC_MONETARY,
	LC_ALL
    };

    /*
     * LC_CTYPE, etc. correspond to the indices for the strings.
     */

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "category ?locale?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    if (objc == 3) {
	locale = Tcl_GetString(objv[2]);
    } else {
	locale = NULL;
    }
    locale = setlocale(lcTypes[index], locale);
    if (locale) {
	Tcl_SetStringObj(Tcl_GetObjResult(interp), locale, TCL_INDEX_NONE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupTestSetassocdataTests --
 *
 *	This function is called when an interpreter is deleted to clean
 *	up any data left over from running the testsetassocdata command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Releases storage.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupTestSetassocdataTests(
    void *clientData,	/* Data to be released. */
    TCL_UNUSED(Tcl_Interp *))
{
    Tcl_Free(clientData);
}

/*
 *----------------------------------------------------------------------
 *
 * TestmsbObjCmd --
 *
 *	This procedure implements the "testmsb" command.  It is
 *	used for testing the TclMSB() routine.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestmsbObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    Tcl_WideInt w = 0;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "integer");
	return TCL_ERROR;
    }
    if (sizeof(Tcl_WideUInt) <= sizeof(size_t)) {
	if (TCL_OK != Tcl_GetWideIntFromObj(interp, objv[1], &w)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(TclMSB(w)));
    } else {
	int i;
	if (TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &i)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(TclMSB(i)));
    }
    return TCL_OK;

    (void)clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * TestparserObjCmd --
 *
 *	This procedure implements the "testparser" command.  It is
 *	used for testing the new Tcl script parser in Tcl 8.1.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestparserObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    const char *script;
    int length, dummy;
    Tcl_Parse parse;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "script length");
	return TCL_ERROR;
    }
    script = Tcl_GetStringFromObj(objv[1], &dummy);
    if (Tcl_GetIntFromObj(interp, objv[2], &length)) {
	return TCL_ERROR;
    }
    if (length == 0) {
	length = dummy;
    }
    if (Tcl_ParseCommand(interp, script, length, 0, &parse) != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (remainder of script: \"");
	Tcl_AddErrorInfo(interp, parse.term);
	Tcl_AddErrorInfo(interp, "\")");
	return TCL_ERROR;
    }

    /*
     * The parse completed successfully.  Just print out the contents
     * of the parse structure into the interpreter's result.
     */

    PrintParse(interp, &parse);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexprparserObjCmd --
 *
 *	This procedure implements the "testexprparser" command.  It is
 *	used for testing the new Tcl expression parser in Tcl 8.1.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexprparserObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    const char *script;
    int length, dummy;
    Tcl_Parse parse;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "expr length");
	return TCL_ERROR;
    }
    script = Tcl_GetStringFromObj(objv[1], &dummy);
    if (Tcl_GetIntFromObj(interp, objv[2], &length)) {
	return TCL_ERROR;
    }
    if (length == 0) {
	length = dummy;
    }
    parse.commentStart = NULL;
    parse.commentSize = 0;
    parse.commandStart = NULL;
    parse.commandSize = 0;
    if (Tcl_ParseExpr(interp, script, length, &parse) != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (remainder of expr: \"");
	Tcl_AddErrorInfo(interp, parse.term);
	Tcl_AddErrorInfo(interp, "\")");
	return TCL_ERROR;
    }

    /*
     * The parse completed successfully.  Just print out the contents
     * of the parse structure into the interpreter's result.
     */

    PrintParse(interp, &parse);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintParse --
 *
 *	This procedure prints out the contents of a Tcl_Parse structure
 *	in the result of an interpreter.
 *
 * Results:
 *	Interp's result is set to a prettily formatted version of the
 *	contents of parsePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintParse(
    Tcl_Interp *interp,		/* Interpreter whose result is to be set to
				 * the contents of a parse structure. */
    Tcl_Parse *parsePtr)	/* Parse structure to print out. */
{
    Tcl_Obj *objPtr;
    const char *typeString;
    Tcl_Token *tokenPtr;
    size_t i;

    objPtr = Tcl_GetObjResult(interp);
    if (parsePtr->commentSize + 1 > 1) {
	Tcl_ListObjAppendElement(NULL, objPtr,
		Tcl_NewStringObj(parsePtr->commentStart,
			parsePtr->commentSize));
    } else {
	Tcl_ListObjAppendElement(NULL, objPtr, Tcl_NewStringObj("-", 1));
    }
    Tcl_ListObjAppendElement(NULL, objPtr,
	    Tcl_NewStringObj(parsePtr->commandStart, parsePtr->commandSize));
    Tcl_ListObjAppendElement(NULL, objPtr,
	    Tcl_NewWideIntObj(parsePtr->numWords));
    for (i = 0; i < (size_t)parsePtr->numTokens; i++) {
	tokenPtr = &parsePtr->tokenPtr[i];
	switch (tokenPtr->type) {
	case TCL_TOKEN_EXPAND_WORD:
	    typeString = "expand";
	    break;
	case TCL_TOKEN_WORD:
	    typeString = "word";
	    break;
	case TCL_TOKEN_SIMPLE_WORD:
	    typeString = "simple";
	    break;
	case TCL_TOKEN_TEXT:
	    typeString = "text";
	    break;
	case TCL_TOKEN_BS:
	    typeString = "backslash";
	    break;
	case TCL_TOKEN_COMMAND:
	    typeString = "command";
	    break;
	case TCL_TOKEN_VARIABLE:
	    typeString = "variable";
	    break;
	case TCL_TOKEN_SUB_EXPR:
	    typeString = "subexpr";
	    break;
	case TCL_TOKEN_OPERATOR:
	    typeString = "operator";
	    break;
	default:
	    typeString = "??";
	    break;
	}
	Tcl_ListObjAppendElement(NULL, objPtr,
		Tcl_NewStringObj(typeString, TCL_INDEX_NONE));
	Tcl_ListObjAppendElement(NULL, objPtr,
		Tcl_NewStringObj(tokenPtr->start, tokenPtr->size));
	Tcl_ListObjAppendElement(NULL, objPtr,
		Tcl_NewWideIntObj(tokenPtr->numComponents));
    }
    Tcl_ListObjAppendElement(NULL, objPtr,
	    parsePtr->commandStart ?
	    Tcl_NewStringObj(parsePtr->commandStart + parsePtr->commandSize,
	    TCL_INDEX_NONE) : Tcl_NewObj());
}

/*
 *----------------------------------------------------------------------
 *
 * TestparsevarObjCmd --
 *
 *	This procedure implements the "testparsevar" command.  It is
 *	used for testing Tcl_ParseVar.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestparsevarObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    const char *value, *name, *termPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName");
	return TCL_ERROR;
    }
    name = Tcl_GetString(objv[1]);
    value = Tcl_ParseVar(interp, name, &termPtr);
    if (value == NULL) {
	return TCL_ERROR;
    }

    Tcl_AppendElement(interp, value);
    Tcl_AppendElement(interp, termPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestparsevarnameObjCmd --
 *
 *	This procedure implements the "testparsevarname" command.  It is
 *	used for testing the new Tcl script parser in Tcl 8.1.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestparsevarnameObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    const char *script;
    int append, length, dummy;
    Tcl_Parse parse;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "script length append");
	return TCL_ERROR;
    }
    script = Tcl_GetStringFromObj(objv[1], &dummy);
    if (Tcl_GetIntFromObj(interp, objv[2], &length)) {
	return TCL_ERROR;
    }
    if (length == 0) {
	length = dummy;
    }
    if (Tcl_GetIntFromObj(interp, objv[3], &append)) {
	return TCL_ERROR;
    }
    if (Tcl_ParseVarName(interp, script, length, &parse, append) != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (remainder of script: \"");
	Tcl_AddErrorInfo(interp, parse.term);
	Tcl_AddErrorInfo(interp, "\")");
	return TCL_ERROR;
    }

    /*
     * The parse completed successfully.  Just print out the contents
     * of the parse structure into the interpreter's result.
     */

    parse.commentSize = 0;
    parse.commandStart = script + parse.tokenPtr->size;
    parse.commandSize = 0;
    PrintParse(interp, &parse);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestpreferstableObjCmd --
 *
 *	This procedure implements the "testpreferstable" command.  It is
 *	used for being able to test the "package" command even when the
 *  environment variable TCL_PKG_PREFER_LATEST is set in your environment.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestpreferstableObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*objv*/)
{
    Interp *iPtr = (Interp *) interp;

    iPtr->packagePrefer = PKG_PREFER_STABLE;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestprintObjCmd --
 *
 *	This procedure implements the "testprint" command.  It is
 *	used for being able to test the Tcl_ObjPrintf() function.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestprintObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    Tcl_WideInt argv1 = 0;
    size_t argv2;

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "format wideint");
    }

    if (objc > 1) {
	Tcl_GetWideIntFromObj(interp, objv[2], &argv1);
    }
    argv2 = (size_t)argv1;
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(Tcl_GetString(objv[1]), argv1, argv2, argv2));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestregexpObjCmd --
 *
 *	This procedure implements the "testregexp" command. It is used to give
 *	a direct interface for regexp flags. It's identical to
 *	Tcl_RegexpObjCmd except for the -xflags option, and the consequences
 *	thereof (including the REG_EXPECT kludge).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TestregexpObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int i, indices, stringLength, match, about;
    Tcl_Size ii;
    int hasxflags, cflags, eflags;
    Tcl_RegExp regExpr;
    const char *string;
    Tcl_Obj *objPtr;
    Tcl_RegExpInfo info;
    static const char *const options[] = {
	"-indices",	"-nocase",	"-about",	"-expanded",
	"-line",	"-linestop",	"-lineanchor",
	"-xflags",
	"--",		NULL
    };
    enum optionsEnum {
	REGEXP_INDICES, REGEXP_NOCASE,	REGEXP_ABOUT,	REGEXP_EXPANDED,
	REGEXP_MULTI,	REGEXP_NOCROSS,	REGEXP_NEWL,
	REGEXP_XFLAGS,
	REGEXP_LAST
    } index;

    indices = 0;
    about = 0;
    cflags = REG_ADVANCED;
    eflags = 0;
    hasxflags = 0;

    for (i = 1; i < objc; i++) {
	const char *name;

	name = Tcl_GetString(objv[i]);
	if (name[0] != '-') {
	    break;
	}
	if (Tcl_GetIndexFromObj(interp, objv[i], options, "switch", TCL_EXACT,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case REGEXP_INDICES:
	    indices = 1;
	    break;
	case REGEXP_NOCASE:
	    cflags |= REG_ICASE;
	    break;
	case REGEXP_ABOUT:
	    about = 1;
	    break;
	case REGEXP_EXPANDED:
	    cflags |= REG_EXPANDED;
	    break;
	case REGEXP_MULTI:
	    cflags |= REG_NEWLINE;
	    break;
	case REGEXP_NOCROSS:
	    cflags |= REG_NLSTOP;
	    break;
	case REGEXP_NEWL:
	    cflags |= REG_NLANCH;
	    break;
	case REGEXP_XFLAGS:
	    hasxflags = 1;
	    break;
	case REGEXP_LAST:
	    i++;
	    goto endOfForLoop;
	}
    }

  endOfForLoop:
    if (objc - i < hasxflags + 2 - about) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?-switch ...? exp string ?matchVar? ?subMatchVar ...?");
	return TCL_ERROR;
    }
    objc -= i;
    objv += i;

    if (hasxflags) {
	string = Tcl_GetStringFromObj(objv[0], &stringLength);
	TestregexpXflags(string, stringLength, &cflags, &eflags);
	objc--;
	objv++;
    }

    regExpr = Tcl_GetRegExpFromObj(interp, objv[0], cflags);
    if (regExpr == NULL) {
	return TCL_ERROR;
    }

    if (about) {
	if (TclRegAbout(interp, regExpr) < 0) {
	    return TCL_ERROR;
	}
	return TCL_OK;
    }

    objPtr = objv[1];
    match = Tcl_RegExpExecObj(interp, regExpr, objPtr, 0 /* offset */,
	    objc-2 /* nmatches */, eflags);

    if (match < 0) {
	return TCL_ERROR;
    }
    if (match == 0) {
	/*
	 * Set the interpreter's object result to an integer object w/
	 * value 0.
	 */

	Tcl_SetWideIntObj(Tcl_GetObjResult(interp), 0);
	if (objc > 2 && (cflags&REG_EXPECT) && indices) {
	    const char *varName;
	    const char *value;
	    Tcl_Size start, end;
	    char resinfo[TCL_INTEGER_SPACE * 2];

	    varName = Tcl_GetString(objv[2]);
	    TclRegExpRangeUniChar(regExpr, TCL_INDEX_NONE, &start, &end);
	    snprintf(resinfo, sizeof(resinfo), "%" TCL_Z_MODIFIER "d %" TCL_Z_MODIFIER "d", start, end-1);
	    value = Tcl_SetVar2(interp, varName, NULL, resinfo, 0);
	    if (value == NULL) {
		Tcl_AppendResult(interp, "couldn't set variable \"",
			varName, "\"", NULL);
		return TCL_ERROR;
	    }
	} else if (cflags & TCL_REG_CANMATCH) {
	    const char *varName;
	    const char *value;
	    char resinfo[TCL_INTEGER_SPACE * 2];

	    Tcl_RegExpGetInfo(regExpr, &info);
	    varName = Tcl_GetString(objv[2]);
	    snprintf(resinfo, sizeof(resinfo), "%" TCL_Z_MODIFIER "d", info.extendStart);
	    value = Tcl_SetVar2(interp, varName, NULL, resinfo, 0);
	    if (value == NULL) {
		Tcl_AppendResult(interp, "couldn't set variable \"",
			varName, "\"", NULL);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    }

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    objc -= 2;
    objv += 2;

    Tcl_RegExpGetInfo(regExpr, &info);
    for (i = 0; i < objc; i++) {
	Tcl_Size start, end;
	Tcl_Obj *newPtr, *varPtr, *valuePtr;

	varPtr = objv[i];
	ii = ((cflags&REG_EXPECT) && i == objc-1) ? TCL_INDEX_NONE : (Tcl_Size)i;
	if (indices) {
	    Tcl_Obj *objs[2];

	    if (ii == TCL_INDEX_NONE) {
		TclRegExpRangeUniChar(regExpr, ii, &start, &end);
	    } else if (ii > info.nsubs) {
		start = TCL_INDEX_NONE;
		end = TCL_INDEX_NONE;
	    } else {
		start = info.matches[ii].start;
		end = info.matches[ii].end;
	    }

	    /*
	     * Adjust index so it refers to the last character in the match
	     * instead of the first character after the match.
	     */

	    if (end != TCL_INDEX_NONE) {
		end--;
	    }

	    objs[0] = Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(start + 1U)) - 1);
	    objs[1] = Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(end + 1U)) - 1);

	    newPtr = Tcl_NewListObj(2, objs);
	} else {
	    if (ii == TCL_INDEX_NONE) {
		TclRegExpRangeUniChar(regExpr, ii, &start, &end);
		newPtr = Tcl_GetRange(objPtr, start, end);
	    } else if (ii > info.nsubs || info.matches[ii].end + 1 <= 1) {
		newPtr = Tcl_NewObj();
	    } else {
		newPtr = Tcl_GetRange(objPtr, info.matches[ii].start,
			info.matches[ii].end - 1);
	    }
	}
	valuePtr = Tcl_ObjSetVar2(interp, varPtr, NULL, newPtr, TCL_LEAVE_ERR_MSG);
	if (valuePtr == NULL) {
	    return TCL_ERROR;
	}
    }

    /*
     * Set the interpreter's object result to an integer object w/ value 1.
     */

    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), 1);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TestregexpXflags --
 *
 *	Parse a string of extended regexp flag letters, for testing.
 *
 * Results:
 *	No return value (you're on your own for errors here).
 *
 * Side effects:
 *	Modifies *cflagsPtr, a regcomp flags word, and *eflagsPtr, a
 *	regexec flags word, as appropriate.
 *
 *----------------------------------------------------------------------
 */

static void
TestregexpXflags(
    const char *string,	/* The string of flags. */
    size_t length,			/* The length of the string in bytes. */
    int *cflagsPtr,		/* compile flags word */
    int *eflagsPtr)		/* exec flags word */
{
    size_t i;
    int cflags, eflags;

    cflags = *cflagsPtr;
    eflags = *eflagsPtr;
    for (i = 0; i < length; i++) {
	switch (string[i]) {
	case 'a':
	    cflags |= REG_ADVF;
	    break;
	case 'b':
	    cflags &= ~REG_ADVANCED;
	    break;
	case 'c':
	    cflags |= TCL_REG_CANMATCH;
	    break;
	case 'e':
	    cflags &= ~REG_ADVANCED;
	    cflags |= REG_EXTENDED;
	    break;
	case 'q':
	    cflags &= ~REG_ADVANCED;
	    cflags |= REG_QUOTE;
	    break;
	case 'o':			/* o for opaque */
	    cflags |= REG_NOSUB;
	    break;
	case 's':			/* s for start */
	    cflags |= REG_BOSONLY;
	    break;
	case '+':
	    cflags |= REG_FAKE;
	    break;
	case ',':
	    cflags |= REG_PROGRESS;
	    break;
	case '.':
	    cflags |= REG_DUMP;
	    break;
	case ':':
	    eflags |= REG_MTRACE;
	    break;
	case ';':
	    eflags |= REG_FTRACE;
	    break;
	case '^':
	    eflags |= REG_NOTBOL;
	    break;
	case '$':
	    eflags |= REG_NOTEOL;
	    break;
	case 't':
	    cflags |= REG_EXPECT;
	    break;
	case '%':
	    eflags |= REG_SMALL;
	    break;
	}
    }

    *cflagsPtr = cflags;
    *eflagsPtr = eflags;
}

/*
 *----------------------------------------------------------------------
 *
 * TestreturnObjCmd --
 *
 *	This procedure implements the "testreturn" command. It is
 *	used to verify that a
 *		return TCL_RETURN;
 *	has same behavior as
 *		return Tcl_SetReturnOptions(interp, Tcl_NewObj());
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TestreturnObjCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*objv*/)
{
    return TCL_RETURN;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetassocdataCmd --
 *
 *	This procedure implements the "testsetassocdata" command. It is used
 *	to test Tcl_SetAssocData.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies or creates an association between a key and associated
 *	data for this interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetassocdataCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    char *buf, *oldData;
    Tcl_InterpDeleteProc *procPtr;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" data_key data_item\"", NULL);
	return TCL_ERROR;
    }

    buf = (char *)Tcl_Alloc(strlen(argv[2]) + 1);
    strcpy(buf, argv[2]);

    /*
     * If we previously associated a malloced value with the variable,
     * free it before associating a new value.
     */

    oldData = (char *) Tcl_GetAssocData(interp, argv[1], &procPtr);
    if ((oldData != NULL) && (procPtr == CleanupTestSetassocdataTests)) {
	Tcl_Free(oldData);
    }

    Tcl_SetAssocData(interp, argv[1], CleanupTestSetassocdataTests,	buf);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetplatformCmd --
 *
 *	This procedure implements the "testsetplatform" command. It is
 *	used to change the tclPlatform global variable so all file
 *	name conversions can be tested on a single platform.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets the tclPlatform global variable.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetplatformCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    size_t length;
    TclPlatformType *platform;

    platform = TclGetPlatform();

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"", argv[0],
		" platform\"", NULL);
	return TCL_ERROR;
    }

    length = strlen(argv[1]);
    if (strncmp(argv[1], "unix", length) == 0) {
	*platform = TCL_PLATFORM_UNIX;
    } else if (strncmp(argv[1], "windows", length) == 0) {
	*platform = TCL_PLATFORM_WINDOWS;
    } else {
	Tcl_AppendResult(interp, "unsupported platform: should be one of "
		"unix, or windows", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TeststaticlibraryCmd --
 *
 *	This procedure implements the "teststaticlibrary" command.
 *	It is used to test the procedure Tcl_StaticLibrary.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	When the packge given by argv[1] is loaded into an interpreter,
 *	variable "x" in that interpreter is set to "loaded".
 *
 *----------------------------------------------------------------------
 */

static int
TeststaticlibraryCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int safe, loaded;

    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " prefix safe loaded\"", NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &safe) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &loaded) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_StaticLibrary((loaded) ? interp : NULL, argv[1],
	    StaticInitProc, (safe) ? StaticInitProc : NULL);
    return TCL_OK;
}

static int
StaticInitProc(
    Tcl_Interp *interp)		/* Interpreter in which package is supposedly
				 * being loaded. */
{
    Tcl_SetVar2(interp, "x", NULL, "loaded", TCL_GLOBAL_ONLY);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TesttranslatefilenameCmd --
 *
 *	This procedure implements the "testtranslatefilename" command.
 *	It is used to test the Tcl_TranslateFileName command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TesttranslatefilenameCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tcl_DString buffer;
    const char *result;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " path\"", NULL);
	return TCL_ERROR;
    }
    result = Tcl_TranslateFileName(interp, argv[1], &buffer);
    if (result == NULL) {
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, result, NULL);
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestupvarCmd --
 *
 *	This procedure implements the "testupvar" command.  It is used
 *	to test Tcl_UpVar and Tcl_UpVar2.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates or modifies an "upvar" reference.
 *
 *----------------------------------------------------------------------
 */

static int
TestupvarCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int flags = 0;

    if ((argc != 5) && (argc != 6)) {
	Tcl_AppendResult(interp, "wrong # arguments: should be \"",
		argv[0], " level name ?name2? dest global\"", NULL);
	return TCL_ERROR;
    }

    if (argc == 5) {
	if (strcmp(argv[4], "global") == 0) {
	    flags = TCL_GLOBAL_ONLY;
	} else if (strcmp(argv[4], "namespace") == 0) {
	    flags = TCL_NAMESPACE_ONLY;
	}
	return Tcl_UpVar2(interp, argv[1], argv[2], NULL, argv[3], flags);
    } else {
	if (strcmp(argv[5], "global") == 0) {
	    flags = TCL_GLOBAL_ONLY;
	} else if (strcmp(argv[5], "namespace") == 0) {
	    flags = TCL_NAMESPACE_ONLY;
	}
	return Tcl_UpVar2(interp, argv[1], argv[2],
		(argv[3][0] == 0) ? NULL : argv[3], argv[4],
		flags);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestseterrorcodeCmd --
 *
 *	This procedure implements the "testseterrorcodeCmd".  This tests up to
 *	five elements passed to the Tcl_SetErrorCode command.
 *
 * Results:
 *	A standard Tcl result. Always returns TCL_ERROR so that
 *	the error code can be tested.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestseterrorcodeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    if (argc > 6) {
	Tcl_AppendResult(interp, "too many args", NULL);
	return TCL_ERROR;
    }
    switch (argc) {
    case 1:
	Tcl_SetErrorCode(interp, "NONE", NULL);
	break;
    case 2:
	Tcl_SetErrorCode(interp, argv[1], NULL);
	break;
    case 3:
	Tcl_SetErrorCode(interp, argv[1], argv[2], NULL);
	break;
    case 4:
	Tcl_SetErrorCode(interp, argv[1], argv[2], argv[3], NULL);
	break;
    case 5:
	Tcl_SetErrorCode(interp, argv[1], argv[2], argv[3], argv[4], NULL);
	break;
    case 6:
	Tcl_SetErrorCode(interp, argv[1], argv[2], argv[3], argv[4],
		argv[5], NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetobjerrorcodeCmd --
 *
 *	This procedure implements the "testsetobjerrorcodeCmd".
 *	This tests the Tcl_SetObjErrorCode function.
 *
 * Results:
 *	A standard Tcl result. Always returns TCL_ERROR so that
 *	the error code can be tested.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetobjerrorcodeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    Tcl_SetObjErrorCode(interp, Tcl_ConcatObj(objc - 1, objv + 1));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestfeventCmd --
 *
 *	This procedure implements the "testfevent" command.  It is
 *	used for testing the "fileevent" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates and deletes interpreters.
 *
 *----------------------------------------------------------------------
 */

static int
TestfeventCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    static Tcl_Interp *interp2 = NULL;
    int code;
    Tcl_Channel chan;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg ...?", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "cmd") == 0) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " cmd script", NULL);
	    return TCL_ERROR;
	}
	if (interp2 != NULL) {
	    code = Tcl_EvalEx(interp2, argv[2], TCL_INDEX_NONE, TCL_EVAL_GLOBAL);
	    Tcl_SetObjResult(interp, Tcl_GetObjResult(interp2));
	    return code;
	} else {
	    Tcl_AppendResult(interp,
		    "called \"testfevent code\" before \"testfevent create\"",
		    NULL);
	    return TCL_ERROR;
	}
    } else if (strcmp(argv[1], "create") == 0) {
	if (interp2 != NULL) {
	    Tcl_DeleteInterp(interp2);
	}
	interp2 = Tcl_CreateInterp();
	return Tcl_Init(interp2);
    } else if (strcmp(argv[1], "delete") == 0) {
	if (interp2 != NULL) {
	    Tcl_DeleteInterp(interp2);
	}
	interp2 = NULL;
    } else if (strcmp(argv[1], "share") == 0) {
	if (interp2 != NULL) {
	    chan = Tcl_GetChannel(interp, argv[2], NULL);
	    if (chan == (Tcl_Channel) NULL) {
		return TCL_ERROR;
	    }
	    Tcl_RegisterChannel(interp2, chan);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestpanicCmd --
 *
 *	Calls the panic routine.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side effects:
 *	May exit application.
 *
 *----------------------------------------------------------------------
 */

static int
TestpanicCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    /*
     *  Put the arguments into a var args structure
     *  Append all of the arguments together separated by spaces
     */

    char *argString = Tcl_Merge(argc-1, argv+1);
    Tcl_Panic("%s", argString);
    Tcl_Free(argString);

    return TCL_OK;
}

static int
TestfileCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    Tcl_Obj *const argv[])	/* The argument objects. */
{
    int force, i, j, result;
    Tcl_Obj *error = NULL;
    const char *subcmd;

    if (argc < 3) {
	return TCL_ERROR;
    }

    force = 0;
    i = 2;
    if (strcmp(Tcl_GetString(argv[2]), "-force") == 0) {
	force = 1;
	i = 3;
    }

    if (argc - i > 2) {
	return TCL_ERROR;
    }

    for (j = i; j < argc; j++) {
	if (Tcl_FSGetNormalizedPath(interp, argv[j]) == NULL) {
	    return TCL_ERROR;
	}
    }

    subcmd = Tcl_GetString(argv[1]);

    if (strcmp(subcmd, "mv") == 0) {
	result = TclpObjRenameFile(argv[i], argv[i + 1]);
    } else if (strcmp(subcmd, "cp") == 0) {
	result = TclpObjCopyFile(argv[i], argv[i + 1]);
    } else if (strcmp(subcmd, "rm") == 0) {
	result = TclpObjDeleteFile(argv[i]);
    } else if (strcmp(subcmd, "mkdir") == 0) {
	result = TclpObjCreateDirectory(argv[i]);
    } else if (strcmp(subcmd, "cpdir") == 0) {
	result = TclpObjCopyDirectory(argv[i], argv[i + 1], &error);
    } else if (strcmp(subcmd, "rmdir") == 0) {
	result = TclpObjRemoveDirectory(argv[i], force, &error);
    } else {
	result = TCL_ERROR;
	goto end;
    }

    if (result != TCL_OK) {
	if (error != NULL) {
	    if (Tcl_GetString(error)[0] != '\0') {
		Tcl_AppendResult(interp, Tcl_GetString(error), " ", NULL);
	    }
	    Tcl_DecrRefCount(error);
	}
	Tcl_AppendResult(interp, Tcl_ErrnoId(), NULL);
    }

  end:
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetvarfullnameCmd --
 *
 *	Implements the "testgetvarfullname" cmd that is used when testing
 *	the Tcl_GetVariableFullName procedure.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetvarfullnameCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    const char *name, *arg;
    int flags = 0;
    Tcl_Namespace *namespacePtr;
    Tcl_CallFrame *framePtr;
    Tcl_Var variable;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "name scope");
	return TCL_ERROR;
    }

    name = Tcl_GetString(objv[1]);

    arg = Tcl_GetString(objv[2]);
    if (strcmp(arg, "global") == 0) {
	flags = TCL_GLOBAL_ONLY;
    } else if (strcmp(arg, "namespace") == 0) {
	flags = TCL_NAMESPACE_ONLY;
    }

    /*
     * This command, like any other created with Tcl_Create[Obj]Command, runs
     * in the global namespace. As a "namespace-aware" command that needs to
     * run in a particular namespace, it must activate that namespace itself.
     */

    if (flags == TCL_NAMESPACE_ONLY) {
	namespacePtr = Tcl_FindNamespace(interp, "::test_ns_var", NULL,
		TCL_LEAVE_ERR_MSG);
	if (namespacePtr == NULL) {
	    return TCL_ERROR;
	}
	(void) TclPushStackFrame(interp, &framePtr, namespacePtr,
		/*isProcCallFrame*/ 0);
    }

    variable = Tcl_FindNamespaceVar(interp, name, NULL,
	    (flags | TCL_LEAVE_ERR_MSG));

    if (flags == TCL_NAMESPACE_ONLY) {
	TclPopStackFrame(interp);
    }
    if (variable == (Tcl_Var) NULL) {
	return TCL_ERROR;
    }
    Tcl_GetVariableFullName(interp, variable, Tcl_GetObjResult(interp));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetTimesObjCmd --
 *
 *	This procedure implements the "gettimes" command.  It is used for
 *	computing the time needed for various basic operations such as reading
 *	variables, allocating memory, snprintf, converting variables, etc.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Allocates and frees memory, sets a variable "a" in the interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
GetTimesObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* The current interpreter. */
    TCL_UNUSED(int) /*cobjc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*cobjv*/)
{
    Interp *iPtr = (Interp *) interp;
    int i, n;
    double timePer;
    Tcl_Time start, stop;
    Tcl_Obj *objPtr, **objv;
    const char *s;
    char newString[TCL_INTEGER_SPACE];

    /* alloc & free 100000 times */
    fprintf(stderr, "alloc & free 100000 6 word items\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	objPtr = (Tcl_Obj *)Tcl_Alloc(sizeof(Tcl_Obj));
	Tcl_Free(objPtr);
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per alloc+free\n", timePer/100000);

    /* alloc 5000 times */
    fprintf(stderr, "alloc 5000 6 word items\n");
    objv = (Tcl_Obj **)Tcl_Alloc(5000 * sizeof(Tcl_Obj *));
    Tcl_GetTime(&start);
    for (i = 0;  i < 5000;  i++) {
	objv[i] = (Tcl_Obj *)Tcl_Alloc(sizeof(Tcl_Obj));
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per alloc\n", timePer/5000);

    /* free 5000 times */
    fprintf(stderr, "free 5000 6 word items\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 5000;  i++) {
	Tcl_Free(objv[i]);
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per free\n", timePer/5000);

    /* Tcl_NewObj 5000 times */
    fprintf(stderr, "Tcl_NewObj 5000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 5000;  i++) {
	objv[i] = Tcl_NewObj();
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_NewObj\n", timePer/5000);

    /* Tcl_DecrRefCount 5000 times */
    fprintf(stderr, "Tcl_DecrRefCount 5000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 5000;  i++) {
	objPtr = objv[i];
	Tcl_DecrRefCount(objPtr);
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_DecrRefCount\n", timePer/5000);
    Tcl_Free(objv);

    /* TclGetString 100000 times */
    fprintf(stderr, "Tcl_GetStringFromObj of \"12345\" 100000 times\n");
    objPtr = Tcl_NewStringObj("12345", TCL_INDEX_NONE);
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	(void) TclGetString(objPtr);
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_GetStringFromObj of \"12345\"\n",
	    timePer/100000);

    /* Tcl_GetIntFromObj 100000 times */
    fprintf(stderr, "Tcl_GetIntFromObj of \"12345\" 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	if (Tcl_GetIntFromObj(interp, objPtr, &n) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_GetIntFromObj of \"12345\"\n",
	    timePer/100000);
    Tcl_DecrRefCount(objPtr);

    /* Tcl_GetInt 100000 times */
    fprintf(stderr, "Tcl_GetInt of \"12345\" 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	if (Tcl_GetInt(interp, "12345", &n) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_GetInt of \"12345\"\n",
	    timePer/100000);

    /* snprintf 100000 times */
    fprintf(stderr, "snprintf of 12345 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	snprintf(newString, sizeof(newString), "%d", 12345);
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per snprintf of 12345\n",
	    timePer/100000);

    /* hashtable lookup 100000 times */
    fprintf(stderr, "hashtable lookup of \"gettimes\" 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	(void) Tcl_FindHashEntry(&iPtr->globalNsPtr->cmdTable, "gettimes");
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per hashtable lookup of \"gettimes\"\n",
	    timePer/100000);

    /* Tcl_SetVar 100000 times */
    fprintf(stderr, "Tcl_SetVar2 of \"12345\" 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	s = Tcl_SetVar2(interp, "a", NULL, "12345", TCL_LEAVE_ERR_MSG);
	if (s == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_SetVar of a to \"12345\"\n",
	    timePer/100000);

    /* Tcl_GetVar 100000 times */
    fprintf(stderr, "Tcl_GetVar of a==\"12345\" 100000 times\n");
    Tcl_GetTime(&start);
    for (i = 0;  i < 100000;  i++) {
	s = Tcl_GetVar2(interp, "a", NULL, TCL_LEAVE_ERR_MSG);
	if (s == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_GetTime(&stop);
    timePer = (stop.sec - start.sec)*1000000 + (stop.usec - start.usec);
    fprintf(stderr, "   %.3f usec per Tcl_GetVar of a==\"12345\"\n",
	    timePer/100000);

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NoopCmd --
 *
 *	This procedure is just used to time the overhead involved in
 *	parsing and invoking a command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NoopCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NoopObjCmd --
 *
 *	This object-based procedure is just used to time the overhead
 *	involved in parsing and invoking a command.
 *
 * Results:
 *	Returns the TCL_OK result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NoopObjCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*objv*/)
{
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TeststringbytesObjCmd --
 *	Returns bytearray value of the bytes in argument string rep
 *
 * Results:
 *	Returns the TCL_OK result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TeststringbytesObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    int n;
    const unsigned char *p;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    p = (const unsigned char *)Tcl_GetStringFromObj(objv[1], &n);
    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(p, n));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestpurebytesobjObjCmd --
 *
 *	This object-based procedure constructs a pure bytes object
 *	without type and with internal representation containing NULL's.
 *
 *	If no argument supplied it returns empty object with tclEmptyStringRep,
 *	otherwise it returns this as pure bytes object with bytes value equal
 *	string.
 *
 * Results:
 *	Returns the TCL_OK result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestpurebytesobjObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    Tcl_Obj *objPtr;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?string?");
	return TCL_ERROR;
    }
    objPtr = Tcl_NewObj();
    /*
    objPtr->internalRep.twoPtrValue.ptr1 = NULL;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    */
    memset(&objPtr->internalRep, 0, sizeof(objPtr->internalRep));
    if (objc == 2) {
	const char *s = Tcl_GetString(objv[1]);
	objPtr->length = objv[1]->length;
	objPtr->bytes = (char *)Tcl_Alloc(objPtr->length + 1);
	memcpy(objPtr->bytes, s, objPtr->length);
	objPtr->bytes[objPtr->length] = 0;
    }
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetbytearraylengthObjCmd --
 *
 *	Testing command 'testsetbytearraylength` used to test the public
 *	interface routine Tcl_SetByteArrayLength().
 *
 * Results:
 *	Returns the TCL_OK result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetbytearraylengthObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    int n;
    Tcl_Obj *obj = NULL;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value length");
	return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[2], &n)) {
	return TCL_ERROR;
    }
    obj = objv[1];
    if (Tcl_IsShared(obj)) {
	obj = Tcl_DuplicateObj(obj);
    }
    if (Tcl_SetByteArrayLength(obj, n) == NULL) {
	if (obj != objv[1]) {
	    Tcl_DecrRefCount(obj);
	}
	Tcl_AppendResult(interp, "expected bytes", NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, obj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestbytestringObjCmd --
 *
 *	This object-based procedure constructs a string which can
 *	possibly contain invalid UTF-8 bytes.
 *
 * Results:
 *	Returns the TCL_OK result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestbytestringObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* The argument objects. */
{
    size_t n = 0;
    const char *p;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytearray");
	return TCL_ERROR;
    }

    p = (const char *)Tcl_GetBytesFromObj(interp, objv[1], &n);
    if (p == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(p, n));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetCmd --
 *
 *	Implements the "testset{err,noerr}" cmds that are used when testing
 *	Tcl_Set/GetVar C Api with/without TCL_LEAVE_ERR_MSG flag
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *     Variables may be set.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetCmd(
    void *data,		/* Additional flags for Get/SetVar2. */
    Tcl_Interp *interp,/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int flags = PTR2INT(data);
    const char *value;

    if (argc == 2) {
	Tcl_AppendResult(interp, "before get", NULL);
	value = Tcl_GetVar2(interp, argv[1], NULL, flags);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, value);
	return TCL_OK;
    } else if (argc == 3) {
	Tcl_AppendResult(interp, "before set", NULL);
	value = Tcl_SetVar2(interp, argv[1], NULL, argv[2], flags);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, value);
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName ?newValue?\"", NULL);
	return TCL_ERROR;
    }
}
static int
Testset2Cmd(
    void *data,		/* Additional flags for Get/SetVar2. */
    Tcl_Interp *interp,/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int flags = PTR2INT(data);
    const char *value;

    if (argc == 3) {
	Tcl_AppendResult(interp, "before get", NULL);
	value = Tcl_GetVar2(interp, argv[1], argv[2], flags);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, value);
	return TCL_OK;
    } else if (argc == 4) {
	Tcl_AppendResult(interp, "before set", NULL);
	value = Tcl_SetVar2(interp, argv[1], argv[2], argv[3], flags);
	if (value == NULL) {
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, value);
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " varName elemName ?newValue?\"", NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestmainthreadCmd  --
 *
 *	Implements the "testmainthread" cmd that is used to test the
 *	'Tcl_GetCurrentThread' API.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestmainthreadCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,/* Current interpreter. */
    int argc,			/* Number of arguments. */
    TCL_UNUSED(const char **) /*argv*/)
{
    if (argc == 1) {
	Tcl_Obj *idObj = Tcl_NewWideIntObj((Tcl_WideInt)(size_t)Tcl_GetCurrentThread());

	Tcl_SetObjResult(interp, idObj);
	return TCL_OK;
    } else {
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MainLoop --
 *
 *	A main loop set by TestsetmainloopCmd below.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers could do anything.
 *
 *----------------------------------------------------------------------
 */

static void
MainLoop(void)
{
    while (!exitMainLoop) {
	Tcl_DoOneEvent(0);
    }
    fprintf(stdout,"Exit MainLoop\n");
    fflush(stdout);
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetmainloopCmd  --
 *
 *	Implements the "testsetmainloop" cmd that is used to test the
 *	'Tcl_SetMainLoop' API.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetmainloopCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    exitMainLoop = 0;
    Tcl_SetMainLoop(MainLoop);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestexitmainloopCmd  --
 *
 *	Implements the "testexitmainloop" cmd that is used to test the
 *	'Tcl_SetMainLoop' API.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestexitmainloopCmd(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    exitMainLoop = 1;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestChannelCmd --
 *
 *	Implements the Tcl "testchannel" debugging command and its
 *	subcommands. This is part of the testing environment.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestChannelCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter for result. */
    int argc,			/* Count of additional args. */
    const char **argv)		/* Additional arg strings. */
{
    const char *cmdName;	/* Sub command. */
    Tcl_HashTable *hTblPtr;	/* Hash table of channels. */
    Tcl_HashSearch hSearch;	/* Search variable. */
    Tcl_HashEntry *hPtr;	/* Search variable. */
    Channel *chanPtr;		/* The actual channel. */
    ChannelState *statePtr;	/* state info for channel */
    Tcl_Channel chan;		/* The opaque type. */
    size_t len;			/* Length of subcommand string. */
    int IOQueued;		/* How much IO is queued inside channel? */
    char buf[TCL_INTEGER_SPACE];/* For snprintf. */
    int mode;			/* rw mode of the channel */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" subcommand ?additional args..?\"", NULL);
	return TCL_ERROR;
    }
    cmdName = argv[1];
    len = strlen(cmdName);

    chanPtr = NULL;

    if (argc > 2) {
	if ((cmdName[0] == 's') && (strncmp(cmdName, "splice", len) == 0)) {
	    /* For splice access the pool of detached channels.
	     * Locate channel, remove from the list.
	     */

	    TestChannel **nextPtrPtr, *curPtr;

	    chan = (Tcl_Channel) NULL;
	    for (nextPtrPtr = &firstDetached, curPtr = firstDetached;
		 curPtr != NULL;
		 nextPtrPtr = &(curPtr->nextPtr), curPtr = curPtr->nextPtr) {

		if (strcmp(argv[2], Tcl_GetChannelName(curPtr->chan)) == 0) {
		    *nextPtrPtr = curPtr->nextPtr;
		    curPtr->nextPtr = NULL;
		    chan = curPtr->chan;
		    Tcl_Free(curPtr);
		    break;
		}
	    }
	} else {
	    chan = Tcl_GetChannel(interp, argv[2], &mode);
	}
	if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	chanPtr		= (Channel *) chan;
	statePtr	= chanPtr->state;
	chanPtr		= statePtr->topChanPtr;
	chan		= (Tcl_Channel) chanPtr;
    } else {
	statePtr	= NULL;
	chan		= NULL;
    }

    if ((cmdName[0] == 's') && (strncmp(cmdName, "setchannelerror", len) == 0)) {

	Tcl_Obj *msg = Tcl_NewStringObj(argv[3], TCL_INDEX_NONE);

	Tcl_IncrRefCount(msg);
	Tcl_SetChannelError(chan, msg);
	Tcl_DecrRefCount(msg);

	Tcl_GetChannelError(chan, &msg);
	Tcl_SetObjResult(interp, msg);
	Tcl_DecrRefCount(msg);
	return TCL_OK;
    }
    if ((cmdName[0] == 's') && (strncmp(cmdName, "setchannelerrorinterp", len) == 0)) {

	Tcl_Obj *msg = Tcl_NewStringObj(argv[3], TCL_INDEX_NONE);

	Tcl_IncrRefCount(msg);
	Tcl_SetChannelErrorInterp(interp, msg);
	Tcl_DecrRefCount(msg);

	Tcl_GetChannelErrorInterp(interp, &msg);
	Tcl_SetObjResult(interp, msg);
	Tcl_DecrRefCount(msg);
	return TCL_OK;
    }

    /*
     * "cut" is actually more a simplified detach facility as provided by the
     * Thread package. Without the safeguards of a regular command (no
     * checking that the command is truly cut'able, no mutexes for
     * thread-safety). Its complementary command is "splice", see below.
     */

    if ((cmdName[0] == 'c') && (strncmp(cmdName, "cut", len) == 0)) {
	TestChannel *det;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " cut channelName\"", NULL);
	    return TCL_ERROR;
	}

	Tcl_RegisterChannel(NULL, chan); /* prevent closing */
	Tcl_UnregisterChannel(interp, chan);

	Tcl_CutChannel(chan);

	/* Remember the channel in the pool of detached channels */

	det = (TestChannel *)Tcl_Alloc(sizeof(TestChannel));
	det->chan     = chan;
	det->nextPtr  = firstDetached;
	firstDetached = det;

	return TCL_OK;
    }

    if ((cmdName[0] == 'c') &&
	    (strncmp(cmdName, "clearchannelhandlers", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " clearchannelhandlers channelName\"", NULL);
	    return TCL_ERROR;
	}
	Tcl_ClearChannelHandlers(chan);
	return TCL_OK;
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "info", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " info channelName\"", NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, argv[2]);
	Tcl_AppendElement(interp, Tcl_ChannelName(chanPtr->typePtr));
	if (statePtr->flags & TCL_READABLE) {
	    Tcl_AppendElement(interp, "read");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->flags & TCL_WRITABLE) {
	    Tcl_AppendElement(interp, "write");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->flags & CHANNEL_NONBLOCKING) {
	    Tcl_AppendElement(interp, "nonblocking");
	} else {
	    Tcl_AppendElement(interp, "blocking");
	}
	if (statePtr->flags & CHANNEL_LINEBUFFERED) {
	    Tcl_AppendElement(interp, "line");
	} else if (statePtr->flags & CHANNEL_UNBUFFERED) {
	    Tcl_AppendElement(interp, "none");
	} else {
	    Tcl_AppendElement(interp, "full");
	}
	if (statePtr->flags & BG_FLUSH_SCHEDULED) {
	    Tcl_AppendElement(interp, "async_flush");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->flags & CHANNEL_EOF) {
	    Tcl_AppendElement(interp, "eof");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->flags & CHANNEL_BLOCKED) {
	    Tcl_AppendElement(interp, "blocked");
	} else {
	    Tcl_AppendElement(interp, "unblocked");
	}
	if (statePtr->inputTranslation == TCL_TRANSLATE_AUTO) {
	    Tcl_AppendElement(interp, "auto");
	    if (statePtr->flags & INPUT_SAW_CR) {
		Tcl_AppendElement(interp, "saw_cr");
	    } else {
		Tcl_AppendElement(interp, "");
	    }
	} else if (statePtr->inputTranslation == TCL_TRANSLATE_LF) {
	    Tcl_AppendElement(interp, "lf");
	    Tcl_AppendElement(interp, "");
	} else if (statePtr->inputTranslation == TCL_TRANSLATE_CR) {
	    Tcl_AppendElement(interp, "cr");
	    Tcl_AppendElement(interp, "");
	} else if (statePtr->inputTranslation == TCL_TRANSLATE_CRLF) {
	    Tcl_AppendElement(interp, "crlf");
	    if (statePtr->flags & INPUT_SAW_CR) {
		Tcl_AppendElement(interp, "queued_cr");
	    } else {
		Tcl_AppendElement(interp, "");
	    }
	}
	if (statePtr->outputTranslation == TCL_TRANSLATE_AUTO) {
	    Tcl_AppendElement(interp, "auto");
	} else if (statePtr->outputTranslation == TCL_TRANSLATE_LF) {
	    Tcl_AppendElement(interp, "lf");
	} else if (statePtr->outputTranslation == TCL_TRANSLATE_CR) {
	    Tcl_AppendElement(interp, "cr");
	} else if (statePtr->outputTranslation == TCL_TRANSLATE_CRLF) {
	    Tcl_AppendElement(interp, "crlf");
	}
	IOQueued = Tcl_InputBuffered(chan);
	TclFormatInt(buf, IOQueued);
	Tcl_AppendElement(interp, buf);

	IOQueued = Tcl_OutputBuffered(chan);
	TclFormatInt(buf, IOQueued);
	Tcl_AppendElement(interp, buf);

	TclFormatInt(buf, (int)Tcl_Tell(chan));
	Tcl_AppendElement(interp, buf);

	TclFormatInt(buf, statePtr->refCount);
	Tcl_AppendElement(interp, buf);

	return TCL_OK;
    }

    if ((cmdName[0] == 'i') &&
	    (strncmp(cmdName, "inputbuffered", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}
	IOQueued = Tcl_InputBuffered(chan);
	TclFormatInt(buf, IOQueued);
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "isshared", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	TclFormatInt(buf, Tcl_IsChannelShared(chan));
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'i') && (strncmp(cmdName, "isstandard", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	TclFormatInt(buf, Tcl_IsStandardChannel(chan));
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mode", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	if (statePtr->flags & TCL_READABLE) {
	    Tcl_AppendElement(interp, "read");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->flags & TCL_WRITABLE) {
	    Tcl_AppendElement(interp, "write");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	return TCL_OK;
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "maxmode", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	if (statePtr->maxPerms & TCL_READABLE) {
	    Tcl_AppendElement(interp, "read");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	if (statePtr->maxPerms & TCL_WRITABLE) {
	    Tcl_AppendElement(interp, "write");
	} else {
	    Tcl_AppendElement(interp, "");
	}
	return TCL_OK;
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mremove-rd", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }

	return Tcl_RemoveChannelMode(interp, chan, TCL_READABLE);
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mremove-wr", len) == 0)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "channel name required",
                    (char *) NULL);
            return TCL_ERROR;
        }

	return Tcl_RemoveChannelMode(interp, chan, TCL_WRITABLE);
    }

    if ((cmdName[0] == 'm') && (strncmp(cmdName, "mthread", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(
		(Tcl_WideInt) (size_t) Tcl_GetChannelThread(chan)));
	return TCL_OK;
    }

    if ((cmdName[0] == 'n') && (strncmp(cmdName, "name", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, statePtr->channelName, NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'o') && (strncmp(cmdName, "open", len) == 0)) {
	hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
	if (hTblPtr == NULL) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	     hPtr != NULL;
	     hPtr = Tcl_NextHashEntry(&hSearch)) {
	    Tcl_AppendElement(interp, (char *)Tcl_GetHashKey(hTblPtr, hPtr));
	}
	return TCL_OK;
    }

    if ((cmdName[0] == 'o') &&
	    (strncmp(cmdName, "outputbuffered", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	IOQueued = Tcl_OutputBuffered(chan);
	TclFormatInt(buf, IOQueued);
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'q') &&
	    (strncmp(cmdName, "queuedcr", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	Tcl_AppendResult(interp,
		(statePtr->flags & INPUT_SAW_CR) ? "1" : "0", NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'r') && (strncmp(cmdName, "readable", len) == 0)) {
	hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
	if (hTblPtr == NULL) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
	     hPtr != NULL;
	     hPtr = Tcl_NextHashEntry(&hSearch)) {
	    chanPtr  = (Channel *) Tcl_GetHashValue(hPtr);
	    statePtr = chanPtr->state;
	    if (statePtr->flags & TCL_READABLE) {
		Tcl_AppendElement(interp, (char *)Tcl_GetHashKey(hTblPtr, hPtr));
	    }
	}
	return TCL_OK;
    }

    if ((cmdName[0] == 'r') && (strncmp(cmdName, "refcount", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	TclFormatInt(buf, statePtr->refCount);
	Tcl_AppendResult(interp, buf, NULL);
	return TCL_OK;
    }

    /*
     * "splice" is actually more a simplified attach facility as provided by
     * the Thread package. Without the safeguards of a regular command (no
     * checking that the command is truly cut'able, no mutexes for
     * thread-safety). Its complementary command is "cut", see above.
     */

    if ((cmdName[0] == 's') && (strncmp(cmdName, "splice", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}

	Tcl_SpliceChannel(chan);

	Tcl_RegisterChannel(interp, chan);
	Tcl_UnregisterChannel(NULL, chan);

	return TCL_OK;
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "type", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "channel name required", NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, Tcl_ChannelName(chanPtr->typePtr), NULL);
	return TCL_OK;
    }

    if ((cmdName[0] == 'w') && (strncmp(cmdName, "writable", len) == 0)) {
	hTblPtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, "tclIO", NULL);
	if (hTblPtr == NULL) {
	    return TCL_OK;
	}
	for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&hSearch)) {
	    chanPtr = (Channel *) Tcl_GetHashValue(hPtr);
	    statePtr = chanPtr->state;
	    if (statePtr->flags & TCL_WRITABLE) {
		Tcl_AppendElement(interp, (char *)Tcl_GetHashKey(hTblPtr, hPtr));
	    }
	}
	return TCL_OK;
    }

    if ((cmdName[0] == 't') && (strncmp(cmdName, "transform", len) == 0)) {
	/*
	 * Syntax: transform channel -command command
	 */

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " transform channelId -command cmd\"", NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[3], "-command") != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[3],
		    "\": should be \"-command\"", NULL);
	    return TCL_ERROR;
	}

	return TclChannelTransform(interp, chan,
		Tcl_NewStringObj(argv[4], TCL_INDEX_NONE));
    }

    if ((cmdName[0] == 'u') && (strncmp(cmdName, "unstack", len) == 0)) {
	/*
	 * Syntax: unstack channel
	 */

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " unstack channel\"", NULL);
	    return TCL_ERROR;
	}
	return Tcl_UnstackChannel(interp, chan);
    }

    Tcl_AppendResult(interp, "bad option \"", cmdName, "\": should be "
	    "cut, clearchannelhandlers, info, isshared, mode, open, "
	    "readable, splice, writable, transform, unstack", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestChannelEventCmd --
 *
 *	This procedure implements the "testchannelevent" command. It is used
 *	to test the Tcl channel event mechanism.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates, deletes and returns channel event handlers.
 *
 *----------------------------------------------------------------------
 */

static int
TestChannelEventCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tcl_Obj *resultListPtr;
    Channel *chanPtr;
    ChannelState *statePtr;	/* state info for channel */
    EventScriptRecord *esPtr, *prevEsPtr, *nextEsPtr;
    const char *cmd;
    int index, i, mask, len;

    if ((argc < 3) || (argc > 5)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" channelName cmd ?arg1? ?arg2?\"", NULL);
	return TCL_ERROR;
    }
    chanPtr = (Channel *) Tcl_GetChannel(interp, argv[1], NULL);
    if (chanPtr == NULL) {
	return TCL_ERROR;
    }
    statePtr = chanPtr->state;

    cmd = argv[2];
    len = strlen(cmd);
    if ((cmd[0] == 'a') && (strncmp(cmd, "add", len) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " channelName add eventSpec script\"", NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[3], "readable") == 0) {
	    mask = TCL_READABLE;
	} else if (strcmp(argv[3], "writable") == 0) {
	    mask = TCL_WRITABLE;
	} else if (strcmp(argv[3], "none") == 0) {
	    mask = 0;
	} else {
	    Tcl_AppendResult(interp, "bad event name \"", argv[3],
		    "\": must be readable, writable, or none", NULL);
	    return TCL_ERROR;
	}

	esPtr = (EventScriptRecord *)Tcl_Alloc(sizeof(EventScriptRecord));
	esPtr->nextPtr = statePtr->scriptRecordPtr;
	statePtr->scriptRecordPtr = esPtr;

	esPtr->chanPtr = chanPtr;
	esPtr->interp = interp;
	esPtr->mask = mask;
	esPtr->scriptPtr = Tcl_NewStringObj(argv[4], TCL_INDEX_NONE);
	Tcl_IncrRefCount(esPtr->scriptPtr);

	Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
		TclChannelEventScriptInvoker, esPtr);

	return TCL_OK;
    }

    if ((cmd[0] == 'd') && (strncmp(cmd, "delete", len) == 0)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " channelName delete index\"", NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], &index) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (index < 0) {
	    Tcl_AppendResult(interp, "bad event index: ", argv[3],
		    ": must be nonnegative", NULL);
	    return TCL_ERROR;
	}
	for (i = 0, esPtr = statePtr->scriptRecordPtr;
	     (i < index) && (esPtr != NULL);
	     i++, esPtr = esPtr->nextPtr) {
	    /* Empty loop body. */
	}
	if (esPtr == NULL) {
	    Tcl_AppendResult(interp, "bad event index ", argv[3],
		    ": out of range", NULL);
	    return TCL_ERROR;
	}
	if (esPtr == statePtr->scriptRecordPtr) {
	    statePtr->scriptRecordPtr = esPtr->nextPtr;
	} else {
	    for (prevEsPtr = statePtr->scriptRecordPtr;
		 (prevEsPtr != NULL) &&
		     (prevEsPtr->nextPtr != esPtr);
		 prevEsPtr = prevEsPtr->nextPtr) {
		/* Empty loop body. */
	    }
	    if (prevEsPtr == NULL) {
		Tcl_Panic("TestChannelEventCmd: damaged event script list");
	    }
	    prevEsPtr->nextPtr = esPtr->nextPtr;
	}
	Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
		TclChannelEventScriptInvoker, esPtr);
	Tcl_DecrRefCount(esPtr->scriptPtr);
	Tcl_Free(esPtr);

	return TCL_OK;
    }

    if ((cmd[0] == 'l') && (strncmp(cmd, "list", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " channelName list\"", NULL);
	    return TCL_ERROR;
	}
	resultListPtr = Tcl_GetObjResult(interp);
	for (esPtr = statePtr->scriptRecordPtr;
	     esPtr != NULL;
	     esPtr = esPtr->nextPtr) {
	    if (esPtr->mask) {
		Tcl_ListObjAppendElement(interp, resultListPtr, Tcl_NewStringObj(
		    (esPtr->mask == TCL_READABLE) ? "readable" : "writable", TCL_INDEX_NONE));
	    } else {
		Tcl_ListObjAppendElement(interp, resultListPtr,
			Tcl_NewStringObj("none", TCL_INDEX_NONE));
	    }
	    Tcl_ListObjAppendElement(interp, resultListPtr, esPtr->scriptPtr);
	}
	Tcl_SetObjResult(interp, resultListPtr);
	return TCL_OK;
    }

    if ((cmd[0] == 'r') && (strncmp(cmd, "removeall", len) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " channelName removeall\"", NULL);
	    return TCL_ERROR;
	}
	for (esPtr = statePtr->scriptRecordPtr;
	     esPtr != NULL;
	     esPtr = nextEsPtr) {
	    nextEsPtr = esPtr->nextPtr;
	    Tcl_DeleteChannelHandler((Tcl_Channel) chanPtr,
		    TclChannelEventScriptInvoker, esPtr);
	    Tcl_DecrRefCount(esPtr->scriptPtr);
	    Tcl_Free(esPtr);
	}
	statePtr->scriptRecordPtr = NULL;
	return TCL_OK;
    }

    if	((cmd[0] == 's') && (strncmp(cmd, "set", len) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " channelName delete index event\"", NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], &index) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (index < 0) {
	    Tcl_AppendResult(interp, "bad event index: ", argv[3],
		    ": must be nonnegative", NULL);
	    return TCL_ERROR;
	}
	for (i = 0, esPtr = statePtr->scriptRecordPtr;
	     (i < index) && (esPtr != NULL);
	     i++, esPtr = esPtr->nextPtr) {
	    /* Empty loop body. */
	}
	if (esPtr == NULL) {
	    Tcl_AppendResult(interp, "bad event index ", argv[3],
		    ": out of range", NULL);
	    return TCL_ERROR;
	}

	if (strcmp(argv[4], "readable") == 0) {
	    mask = TCL_READABLE;
	} else if (strcmp(argv[4], "writable") == 0) {
	    mask = TCL_WRITABLE;
	} else if (strcmp(argv[4], "none") == 0) {
	    mask = 0;
	} else {
	    Tcl_AppendResult(interp, "bad event name \"", argv[4],
		    "\": must be readable, writable, or none", NULL);
	    return TCL_ERROR;
	}
	esPtr->mask = mask;
	Tcl_CreateChannelHandler((Tcl_Channel) chanPtr, mask,
		TclChannelEventScriptInvoker, esPtr);
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "bad command ", cmd, ", must be one of "
	    "add, delete, list, set, or removeall", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestSocketCmd --
 *
 *	Implements the Tcl "testsocket" debugging command and its
 *	subcommands. This is part of the testing environment.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#define TCP_ASYNC_TEST_MODE	(1<<8)	/* Async testing activated.  Do not
					 * automatically continue connection
					 * process. */

static int
TestSocketCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter for result. */
    int argc,			/* Count of additional args. */
    const char **argv)		/* Additional arg strings. */
{
    const char *cmdName;	/* Sub command. */
    size_t len;			/* Length of subcommand string. */

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" subcommand ?additional args..?\"", NULL);
	return TCL_ERROR;
    }
    cmdName = argv[1];
    len = strlen(cmdName);

    if ((cmdName[0] == 't') && (strncmp(cmdName, "testflags", len) == 0)) {
        Tcl_Channel hChannel;
        int modePtr;
        int testMode;
        TcpState *statePtr;
        /* Set test value in the socket driver
         */
        /* Check for argument "channel name"
         */
        if (argc < 4) {
            Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                    " testflags channel flags\"", NULL);
            return TCL_ERROR;
        }
        hChannel = Tcl_GetChannel(interp, argv[2], &modePtr);
        if ( NULL == hChannel ) {
            Tcl_AppendResult(interp, "unknown channel:", argv[2], NULL);
            return TCL_ERROR;
        }
        statePtr = (TcpState *)Tcl_GetChannelInstanceData(hChannel);
        if ( NULL == statePtr) {
            Tcl_AppendResult(interp, "No channel instance data:", argv[2],
                    NULL);
            return TCL_ERROR;
        }
        if (Tcl_GetBoolean(interp, argv[3], &testMode) != TCL_OK) {
            return TCL_ERROR;
        }
        if (testMode) {
            statePtr->flags |= TCP_ASYNC_TEST_MODE;
        } else {
            statePtr->flags &= ~TCP_ASYNC_TEST_MODE;
        }
        return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad option \"", cmdName, "\": should be "
	    "testflags", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TestServiceModeCmd --
 *
 *	This procedure implements the "testservicemode" command which gets or
 *      sets the current Tcl ServiceMode.  There are several tests which open
 *      a file and assign various handlers to it.  For these tests to be
 *      deterministic it is important that file events not be processed until
 *      all of the handlers are in place.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May change the ServiceMode setting.
 *
 *----------------------------------------------------------------------
 */

static int
TestServiceModeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    int newmode, oldmode;
    if (argc > 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
                         " ?newmode?\"", NULL);
        return TCL_ERROR;
    }
    oldmode = (Tcl_GetServiceMode() != TCL_SERVICE_NONE);
    if (argc == 2) {
        if (Tcl_GetInt(interp, argv[1], &newmode) == TCL_ERROR) {
            return TCL_ERROR;
        }
        if (newmode == 0) {
            Tcl_SetServiceMode(TCL_SERVICE_NONE);
        } else {
            Tcl_SetServiceMode(TCL_SERVICE_ALL);
        }
    }
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(oldmode));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestWrongNumArgsObjCmd --
 *
 *	Test the Tcl_WrongNumArgs function.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Sets interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
TestWrongNumArgsObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Size i, length;
    const char *msg;

    if (objc + 1 < 4) {
	goto insufArgs;
    }

    if (Tcl_GetIntForIndex(interp, objv[1], TCL_INDEX_NONE, &i) != TCL_OK) {
	return TCL_ERROR;
    }

    msg = Tcl_GetStringFromObj(objv[2], &length);
    if (length == 0) {
	msg = NULL;
    }

    if (i > objc - 3) {
	/*
	 * Asked for more arguments than were given.
	 */
    insufArgs:
	Tcl_AppendResult(interp, "insufficient arguments", NULL);
	return TCL_ERROR;
    }

    Tcl_WrongNumArgs(interp, i, &(objv[3]), msg);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestGetIndexFromObjStructObjCmd --
 *
 *	Test the Tcl_GetIndexFromObjStruct function.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Sets interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
TestGetIndexFromObjStructObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    const char *const ary[] = {
	"a", "b", "c", "d", "ee", "ff", NULL, NULL
    };
    int target, flags = 0;
    signed char idx[8];

    if (objc != 3 && objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "argument targetvalue ?flags?");
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[2], &target) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((objc > 3) && (Tcl_GetIntFromObj(interp, objv[3], &flags) != TCL_OK)) {
	return TCL_ERROR;
    }
    memset(idx, 85, sizeof(idx));
    if (Tcl_GetIndexFromObjStruct(interp, (Tcl_GetString(objv[1])[0] ? objv[1] : NULL), ary, 2*sizeof(char *),
	    "dummy", flags, &idx[1]) != TCL_OK) {
	return TCL_ERROR;
    }
    if (idx[0] != 85 || idx[2] != 85) {
	Tcl_AppendResult(interp, "Tcl_GetIndexFromObjStruct overwrites bytes near index variable", NULL);
	return TCL_ERROR;
    } else if (idx[1] != target) {
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%d", idx[1]);
	Tcl_AppendResult(interp, "index value comparison failed: got ",
		buffer, NULL);
	snprintf(buffer, sizeof(buffer), "%d", target);
	Tcl_AppendResult(interp, " when ", buffer, " expected", NULL);
	return TCL_ERROR;
    }
    Tcl_WrongNumArgs(interp, objc, objv, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestFilesystemObjCmd --
 *
 *	This procedure implements the "testfilesystem" command. It is used to
 *	test Tcl_FSRegister, Tcl_FSUnregister, and can be used to test that
 *	the pluggable filesystem works.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Inserts or removes a filesystem from Tcl's stack.
 *
 *----------------------------------------------------------------------
 */

static int
TestFilesystemObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int res, boolVal;
    const char *msg;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "boolean");
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[1], &boolVal) != TCL_OK) {
	return TCL_ERROR;
    }
    if (boolVal) {
	res = Tcl_FSRegister(interp, &testReportingFilesystem);
	msg = (res == TCL_OK) ? "registered" : "failed";
    } else {
	res = Tcl_FSUnregister(&testReportingFilesystem);
	msg = (res == TCL_OK) ? "unregistered" : "failed";
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(msg , TCL_INDEX_NONE));
    return res;
}

static int
TestReportInFilesystem(
    Tcl_Obj *pathPtr,
    void **clientDataPtr)
{
    static Tcl_Obj *lastPathPtr = NULL;
    Tcl_Obj *newPathPtr;

    if (pathPtr == lastPathPtr) {
	/* Reject all files second time around */
	return -1;
    }

    /* Try to claim all files first time around */

    newPathPtr = Tcl_DuplicateObj(pathPtr);
    lastPathPtr = newPathPtr;
    Tcl_IncrRefCount(newPathPtr);
    if (Tcl_FSGetFileSystemForPath(newPathPtr) == NULL) {
	/* Nothing claimed it. Therefore we don't either */
	Tcl_DecrRefCount(newPathPtr);
	lastPathPtr = NULL;
	return -1;
    }
    lastPathPtr = NULL;
    *clientDataPtr = newPathPtr;
    return TCL_OK;
}

/*
 * Simple helper function to extract the native vfs representation of a path
 * object, or NULL if no such representation exists.
 */

static Tcl_Obj *
TestReportGetNativePath(
    Tcl_Obj *pathPtr)
{
    return (Tcl_Obj*) Tcl_FSGetInternalRep(pathPtr, &testReportingFilesystem);
}

static void
TestReportFreeInternalRep(
    void *clientData)
{
    Tcl_Obj *nativeRep = (Tcl_Obj *) clientData;

    if (nativeRep != NULL) {
	/* Free the path */
	Tcl_DecrRefCount(nativeRep);
    }
}

static void *
TestReportDupInternalRep(
    void *clientData)
{
    Tcl_Obj *original = (Tcl_Obj *) clientData;

    Tcl_IncrRefCount(original);
    return clientData;
}

static void
TestReport(
    const char *cmd,
    Tcl_Obj *path,
    Tcl_Obj *arg2)
{
    Tcl_Interp *interp = (Tcl_Interp *) Tcl_FSData(&testReportingFilesystem);

    if (interp == NULL) {
	/* This is bad, but not much we can do about it */
    } else {
	Tcl_Obj *savedResult;
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, "lappend filesystemReport ", TCL_INDEX_NONE);
	Tcl_DStringStartSublist(&ds);
	Tcl_DStringAppendElement(&ds, cmd);
	if (path != NULL) {
	    Tcl_DStringAppendElement(&ds, Tcl_GetString(path));
	}
	if (arg2 != NULL) {
	    Tcl_DStringAppendElement(&ds, Tcl_GetString(arg2));
	}
	Tcl_DStringEndSublist(&ds);
	savedResult = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(savedResult);
	Tcl_SetObjResult(interp, Tcl_NewObj());
	Tcl_EvalEx(interp, Tcl_DStringValue(&ds), TCL_INDEX_NONE, 0);
	Tcl_DStringFree(&ds);
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, savedResult);
	Tcl_DecrRefCount(savedResult);
    }
}

static int
TestReportStat(
    Tcl_Obj *path,		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *buf)		/* Filled with results of stat call. */
{
    TestReport("stat", path, NULL);
    return Tcl_FSStat(TestReportGetNativePath(path), buf);
}

static int
TestReportLstat(
    Tcl_Obj *path,		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *buf)		/* Filled with results of stat call. */
{
    TestReport("lstat", path, NULL);
    return Tcl_FSLstat(TestReportGetNativePath(path), buf);
}

static int
TestReportAccess(
    Tcl_Obj *path,		/* Path of file to access (in current CP). */
    int mode)			/* Permission setting. */
{
    TestReport("access", path, NULL);
    return Tcl_FSAccess(TestReportGetNativePath(path), mode);
}

static Tcl_Channel
TestReportOpenFileChannel(
    Tcl_Interp *interp,		/* Interpreter for error reporting; can be
				 * NULL. */
    Tcl_Obj *fileName,		/* Name of file to open. */
    int mode,			/* POSIX open mode. */
    int permissions)		/* If the open involves creating a file, with
				 * what modes to create it? */
{
    TestReport("open", fileName, NULL);
    return TclpOpenFileChannel(interp, TestReportGetNativePath(fileName),
	    mode, permissions);
}

static int
TestReportMatchInDirectory(
    Tcl_Interp *interp,		/* Interpreter for error messages. */
    Tcl_Obj *resultPtr,		/* Object to lappend results. */
    Tcl_Obj *dirPtr,		/* Contains path to directory to search. */
    const char *pattern,	/* Pattern to match against. */
    Tcl_GlobTypeData *types)	/* Object containing list of acceptable types.
				 * May be NULL. */
{
    if (types != NULL && types->type & TCL_GLOB_TYPE_MOUNT) {
	TestReport("matchmounts", dirPtr, NULL);
	return TCL_OK;
    } else {
	TestReport("matchindirectory", dirPtr, NULL);
	return Tcl_FSMatchInDirectory(interp, resultPtr,
		TestReportGetNativePath(dirPtr), pattern, types);
    }
}

static int
TestReportChdir(
    Tcl_Obj *dirName)
{
    TestReport("chdir", dirName, NULL);
    return Tcl_FSChdir(TestReportGetNativePath(dirName));
}

static int
TestReportLoadFile(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Obj *fileName,		/* Name of the file containing the desired
				 * code. */
    Tcl_LoadHandle *handlePtr,	/* Filled with token for dynamically loaded
				 * file which will be passed back to
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr)
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for
				 * this file. */
{
    TestReport("loadfile", fileName, NULL);
    return Tcl_FSLoadFile(interp, TestReportGetNativePath(fileName), NULL,
	    NULL, NULL, NULL, handlePtr, unloadProcPtr);
}

static Tcl_Obj *
TestReportLink(
    Tcl_Obj *path,		/* Path of file to readlink or link */
    Tcl_Obj *to,		/* Path of file to link to, or NULL */
    int linkType)
{
    TestReport("link", path, to);
    return Tcl_FSLink(TestReportGetNativePath(path), to, linkType);
}

static int
TestReportRenameFile(
    Tcl_Obj *src,		/* Pathname of file or dir to be renamed
				 * (UTF-8). */
    Tcl_Obj *dst)		/* New pathname of file or directory
				 * (UTF-8). */
{
    TestReport("renamefile", src, dst);
    return Tcl_FSRenameFile(TestReportGetNativePath(src),
	    TestReportGetNativePath(dst));
}

static int
TestReportCopyFile(
    Tcl_Obj *src,		/* Pathname of file to be copied (UTF-8). */
    Tcl_Obj *dst)		/* Pathname of file to copy to (UTF-8). */
{
    TestReport("copyfile", src, dst);
    return Tcl_FSCopyFile(TestReportGetNativePath(src),
	    TestReportGetNativePath(dst));
}

static int
TestReportDeleteFile(
    Tcl_Obj *path)		/* Pathname of file to be removed (UTF-8). */
{
    TestReport("deletefile", path, NULL);
    return Tcl_FSDeleteFile(TestReportGetNativePath(path));
}

static int
TestReportCreateDirectory(
    Tcl_Obj *path)		/* Pathname of directory to create (UTF-8). */
{
    TestReport("createdirectory", path, NULL);
    return Tcl_FSCreateDirectory(TestReportGetNativePath(path));
}

static int
TestReportCopyDirectory(
    Tcl_Obj *src,		/* Pathname of directory to be copied
				 * (UTF-8). */
    Tcl_Obj *dst,		/* Pathname of target directory (UTF-8). */
    Tcl_Obj **errorPtr)		/* If non-NULL, to be filled with UTF-8 name
				 * of file causing error. */
{
    TestReport("copydirectory", src, dst);
    return Tcl_FSCopyDirectory(TestReportGetNativePath(src),
	    TestReportGetNativePath(dst), errorPtr);
}

static int
TestReportRemoveDirectory(
    Tcl_Obj *path,		/* Pathname of directory to be removed
				 * (UTF-8). */
    int recursive,		/* If non-zero, removes directories that
				 * are nonempty.  Otherwise, will only remove
				 * empty directories. */
    Tcl_Obj **errorPtr)		/* If non-NULL, to be filled with UTF-8 name
				 * of file causing error. */
{
    TestReport("removedirectory", path, NULL);
    return Tcl_FSRemoveDirectory(TestReportGetNativePath(path), recursive,
	    errorPtr);
}

static const char *const *
TestReportFileAttrStrings(
    Tcl_Obj *fileName,
    Tcl_Obj **objPtrRef)
{
    TestReport("fileattributestrings", fileName, NULL);
    return Tcl_FSFileAttrStrings(TestReportGetNativePath(fileName), objPtrRef);
}

static int
TestReportFileAttrsGet(
    Tcl_Interp *interp,		/* The interpreter for error reporting. */
    int index,			/* index of the attribute command. */
    Tcl_Obj *fileName,		/* filename we are operating on. */
    Tcl_Obj **objPtrRef)	/* for output. */
{
    TestReport("fileattributesget", fileName, NULL);
    return Tcl_FSFileAttrsGet(interp, index,
	    TestReportGetNativePath(fileName), objPtrRef);
}

static int
TestReportFileAttrsSet(
    Tcl_Interp *interp,		/* The interpreter for error reporting. */
    int index,			/* index of the attribute command. */
    Tcl_Obj *fileName,		/* filename we are operating on. */
    Tcl_Obj *objPtr)		/* for input. */
{
    TestReport("fileattributesset", fileName, objPtr);
    return Tcl_FSFileAttrsSet(interp, index,
	    TestReportGetNativePath(fileName), objPtr);
}

static int
TestReportUtime(
    Tcl_Obj *fileName,
    struct utimbuf *tval)
{
    TestReport("utime", fileName, NULL);
    return Tcl_FSUtime(TestReportGetNativePath(fileName), tval);
}

static int
TestReportNormalizePath(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *pathPtr,
    int nextCheckpoint)
{
    TestReport("normalizepath", pathPtr, NULL);
    return nextCheckpoint;
}

static int
SimplePathInFilesystem(
    Tcl_Obj *pathPtr,
    TCL_UNUSED(void **))
{
    const char *str = Tcl_GetString(pathPtr);

    if (strncmp(str, "simplefs:/", 10)) {
	return -1;
    }
    return TCL_OK;
}

/*
 * This is a slightly 'hacky' filesystem which is used just to test a few
 * important features of the vfs code: (1) that you can load a shared library
 * from a vfs, (2) that when copying files from one fs to another, the 'mtime'
 * is preserved. (3) that recursive cross-filesystem directory copies have the
 * correct behaviour with/without -force.
 *
 * It treats any file in 'simplefs:/' as a file, which it routes to the
 * current directory. The real file it uses is whatever follows the trailing
 * '/' (e.g. 'foo' in 'simplefs:/foo'), and that file exists or not according
 * to what is in the native pwd.
 *
 * Please do not consider this filesystem a model of how things are to be
 * done. It is quite the opposite!  But, it does allow us to test some
 * important features.
 */

static int
TestSimpleFilesystemObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int res, boolVal;
    const char *msg;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "boolean");
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[1], &boolVal) != TCL_OK) {
	return TCL_ERROR;
    }
    if (boolVal) {
	res = Tcl_FSRegister(interp, &simpleFilesystem);
	msg = (res == TCL_OK) ? "registered" : "failed";
    } else {
	res = Tcl_FSUnregister(&simpleFilesystem);
	msg = (res == TCL_OK) ? "unregistered" : "failed";
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(msg , TCL_INDEX_NONE));
    return res;
}

/*
 * Treats a file name 'simplefs:/foo' by using the file 'foo' in the current
 * (native) directory.
 */

static Tcl_Obj *
SimpleRedirect(
    Tcl_Obj *pathPtr)		/* Name of file to copy. */
{
    int len;
    const char *str;
    Tcl_Obj *origPtr;

    /*
     * We assume the same name in the current directory is ok.
     */

    str = Tcl_GetStringFromObj(pathPtr, &len);
    if (len < 10 || strncmp(str, "simplefs:/", 10)) {
	/* Probably shouldn't ever reach here */
	Tcl_IncrRefCount(pathPtr);
	return pathPtr;
    }
    origPtr = Tcl_NewStringObj(str+10, TCL_INDEX_NONE);
    Tcl_IncrRefCount(origPtr);
    return origPtr;
}

static int
SimpleMatchInDirectory(
    Tcl_Interp *interp,		/* Interpreter for error
				 * messages. */
    Tcl_Obj *resultPtr,		/* Object to lappend results. */
    Tcl_Obj *dirPtr,		/* Contains path to directory to search. */
    const char *pattern,	/* Pattern to match against. */
    Tcl_GlobTypeData *types)	/* Object containing list of acceptable types.
				 * May be NULL. */
{
    int res;
    Tcl_Obj *origPtr;
    Tcl_Obj *resPtr;

    /* We only provide a new volume, therefore no mounts at all */
    if (types != NULL && types->type & TCL_GLOB_TYPE_MOUNT) {
	return TCL_OK;
    }

    /*
     * We assume the same name in the current directory is ok.
     */
    resPtr = Tcl_NewObj();
    Tcl_IncrRefCount(resPtr);
    origPtr = SimpleRedirect(dirPtr);
    res = Tcl_FSMatchInDirectory(interp, resPtr, origPtr, pattern, types);
    if (res == TCL_OK) {
	size_t gLength, j;
	Tcl_ListObjLength(NULL, resPtr, &gLength);
	for (j = 0; j < gLength; j++) {
	    Tcl_Obj *gElt, *nElt;
	    Tcl_ListObjIndex(NULL, resPtr, j, &gElt);
	    nElt = Tcl_NewStringObj("simplefs:/",10);
	    Tcl_AppendObjToObj(nElt, gElt);
	    Tcl_ListObjAppendElement(NULL, resultPtr, nElt);
	}
    }
    Tcl_DecrRefCount(origPtr);
    Tcl_DecrRefCount(resPtr);
    return res;
}

static Tcl_Channel
SimpleOpenFileChannel(
    Tcl_Interp *interp,		/* Interpreter for error reporting; can be
				 * NULL. */
    Tcl_Obj *pathPtr,		/* Name of file to open. */
    int mode,			/* POSIX open mode. */
    int permissions)		/* If the open involves creating a file, with
				 * what modes to create it? */
{
    Tcl_Obj *tempPtr;
    Tcl_Channel chan;

    if ((mode != 0) && !(mode & O_RDONLY)) {
	Tcl_AppendResult(interp, "read-only", NULL);
	return NULL;
    }

    tempPtr = SimpleRedirect(pathPtr);
    chan = Tcl_FSOpenFileChannel(interp, tempPtr, "r", permissions);
    Tcl_DecrRefCount(tempPtr);
    return chan;
}

static int
SimpleAccess(
    Tcl_Obj *pathPtr,		/* Path of file to access (in current CP). */
    int mode)			/* Permission setting. */
{
    Tcl_Obj *tempPtr = SimpleRedirect(pathPtr);
    int res = Tcl_FSAccess(tempPtr, mode);

    Tcl_DecrRefCount(tempPtr);
    return res;
}

static int
SimpleStat(
    Tcl_Obj *pathPtr,		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *bufPtr)	/* Filled with results of stat call. */
{
    Tcl_Obj *tempPtr = SimpleRedirect(pathPtr);
    int res = Tcl_FSStat(tempPtr, bufPtr);

    Tcl_DecrRefCount(tempPtr);
    return res;
}

static Tcl_Obj *
SimpleListVolumes(void)
{
    /* Add one new volume */
    Tcl_Obj *retVal;

    retVal = Tcl_NewStringObj("simplefs:/", TCL_INDEX_NONE);
    Tcl_IncrRefCount(retVal);
    return retVal;
}

/*
 * Used to check operations of Tcl_UtfNext.
 *
 * Usage: testutfnext -bytestring $bytes
 */

static int
TestUtfNextCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    size_t numBytes;
    char *bytes;
    const char *result, *first;
    char buffer[32];
    static const char tobetested[] = "A\xA0\xC0\xC1\xC2\xD0\xE0\xE8\xF2\xF7\xF8\xFE\xFF";
    const char *p = tobetested;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?-bytestring? bytes");
	return TCL_ERROR;
    }
	bytes = Tcl_GetStringFromObj(objv[1], &numBytes);

    if (numBytes + 4U > sizeof(buffer)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"\"testutfnext\" can only handle %" TCL_Z_MODIFIER "u bytes",
		sizeof(buffer) - 4));
	return TCL_ERROR;
    }

    memcpy(buffer + 1, bytes, numBytes);
    buffer[0] = buffer[numBytes + 1] = buffer[numBytes + 2] = buffer[numBytes + 3] = '\xA0';

    first = result = Tcl_UtfNext(buffer + 1);
    while ((buffer[0] = *p++) != '\0') {
	/* Run Tcl_UtfNext with many more possible bytes at src[-1], all should give the same result */
	result = Tcl_UtfNext(buffer + 1);
	if (first != result) {
	    Tcl_AppendResult(interp, "Tcl_UtfNext is not supposed to read src[-1]", NULL);
	    return TCL_ERROR;
	}
    }
    p = tobetested;
    while ((buffer[numBytes + 1] = *p++) != '\0') {
	/* Run Tcl_UtfNext with many more possible bytes at src[end], all should give the same result */
	result = Tcl_UtfNext(buffer + 1);
	if (first != result) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "Tcl_UtfNext is not supposed to read src[end]\n"
		    "Different result when src[end] is %#x", UCHAR(p[-1])));
	    return TCL_ERROR;
	}
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(first - buffer - 1));

    return TCL_OK;
}
/*
 * Used to check operations of Tcl_UtfPrev.
 *
 * Usage: testutfprev $bytes $offset
 */

static int
TestUtfPrevCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Size numBytes, offset;
    char *bytes;
    const char *result;

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytes ?offset?");
	return TCL_ERROR;
    }

    bytes = Tcl_GetStringFromObj(objv[1], &numBytes);

    if (objc == 3) {
	if (TCL_OK != Tcl_GetIntForIndex(interp, objv[2], numBytes, &offset)) {
	    return TCL_ERROR;
	}
	if (offset == TCL_INDEX_NONE) {
	    offset = 0;
	}
	if (offset > numBytes) {
	    offset = numBytes;
	}
    } else {
	offset = numBytes;
    }
    result = Tcl_UtfPrev(bytes + offset, bytes);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(result - bytes));
    return TCL_OK;
}

/*
 * Used to check correct string-length determining in Tcl_NumUtfChars
 */

static int
TestNumUtfCharsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc > 1) {
	Tcl_Size numBytes, len, limit = TCL_INDEX_NONE;
	const char *bytes = Tcl_GetStringFromObj(objv[1], &numBytes);

	if (objc > 2) {
	    if (Tcl_GetIntForIndex(interp, objv[2], numBytes, &limit) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (limit > numBytes + 1) {
		limit = numBytes + 1;
	    }
	}
	len = Tcl_NumUtfChars(bytes, limit);
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(len));
    }
    return TCL_OK;
}

/*
 * Used to check correct operation of Tcl_UtfFindFirst
 */

static int
TestFindFirstCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc > 1) {
	int len = -1;

	if (objc > 2) {
	    (void) Tcl_GetIntFromObj(interp, objv[2], &len);
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_UtfFindFirst(Tcl_GetString(objv[1]), len), TCL_INDEX_NONE));
    }
    return TCL_OK;
}

/*
 * Used to check correct operation of Tcl_UtfFindLast
 */

static int
TestFindLastCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc > 1) {
	int len = -1;

	if (objc > 2) {
	    (void) Tcl_GetIntFromObj(interp, objv[2], &len);
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_UtfFindLast(Tcl_GetString(objv[1]), len), TCL_INDEX_NONE));
    }
    return TCL_OK;
}

static int
TestGetIntForIndexCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Size result;
    Tcl_WideInt endvalue;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "index endvalue");
	return TCL_ERROR;
    }

    if (Tcl_GetWideIntFromObj(interp, objv[2], &endvalue) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetIntForIndex(interp, objv[1], endvalue, &result) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Make sure that (size_t)-2 is output as "-2" and (size_t)-3 as "-3", even for 32-bit */
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(result + 3U)) - 3));
    return TCL_OK;
}



#if defined(HAVE_CPUID) && !defined(MAC_OSX_TCL)
/*
 *----------------------------------------------------------------------
 *
 * TestcpuidCmd --
 *
 *	Retrieves CPU ID information.
 *
 * Usage:
 *	testwincpuid <eax>
 *
 * Parameters:
 *	eax - The value to pass in the EAX register to a CPUID instruction.
 *
 * Results:
 *	Returns a four-element list containing the values from the EAX, EBX,
 *	ECX and EDX registers returned from the CPUID instruction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestcpuidCmd(
    TCL_UNUSED(void *),
    Tcl_Interp* interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const * objv)	/* Parameter vector */
{
    int status, index, i;
    int regs[4];
    Tcl_Obj *regsObjs[4];

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "eax");
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[1], &index) != TCL_OK) {
	return TCL_ERROR;
    }
    status = TclWinCPUID(index, regs);
    if (status != TCL_OK) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("operation not available", TCL_INDEX_NONE));
	return status;
    }
    for (i=0 ; i<4 ; ++i) {
	regsObjs[i] = Tcl_NewWideIntObj(regs[i]);
    }
    Tcl_SetObjResult(interp, Tcl_NewListObj(4, regsObjs));
    return TCL_OK;
}
#endif

/*
 * Used to do basic checks of the TCL_HASH_KEY_SYSTEM_HASH flag
 */

static int
TestHashSystemHashCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const Tcl_HashKeyType hkType = {
	TCL_HASH_KEY_TYPE_VERSION, TCL_HASH_KEY_SYSTEM_HASH,
	NULL, NULL, NULL, NULL
    };
    Tcl_HashTable hash;
    Tcl_HashEntry *hPtr;
    int i, isNew, limit = 100;

    if (objc>1 && Tcl_GetIntFromObj(interp, objv[1], &limit)!=TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_InitCustomHashTable(&hash, TCL_CUSTOM_TYPE_KEYS, &hkType);

    if (hash.numEntries != 0) {
	Tcl_AppendResult(interp, "non-zero initial size", NULL);
	Tcl_DeleteHashTable(&hash);
	return TCL_ERROR;
    }

    for (i=0 ; i<limit ; i++) {
	hPtr = Tcl_CreateHashEntry(&hash, INT2PTR(i), &isNew);
	if (!isNew) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(i));
	    Tcl_AppendToObj(Tcl_GetObjResult(interp)," creation problem", TCL_INDEX_NONE);
	    Tcl_DeleteHashTable(&hash);
	    return TCL_ERROR;
	}
	Tcl_SetHashValue(hPtr, INT2PTR(i+42));
    }

    if (hash.numEntries != (Tcl_Size)limit) {
	Tcl_AppendResult(interp, "unexpected maximal size", NULL);
	Tcl_DeleteHashTable(&hash);
	return TCL_ERROR;
    }

    for (i=0 ; i<limit ; i++) {
	hPtr = Tcl_FindHashEntry(&hash, (char *) INT2PTR(i));
	if (hPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(i));
	    Tcl_AppendToObj(Tcl_GetObjResult(interp)," lookup problem", TCL_INDEX_NONE);
	    Tcl_DeleteHashTable(&hash);
	    return TCL_ERROR;
	}
	if (PTR2INT(Tcl_GetHashValue(hPtr)) != i+42) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(i));
	    Tcl_AppendToObj(Tcl_GetObjResult(interp)," value problem", TCL_INDEX_NONE);
	    Tcl_DeleteHashTable(&hash);
	    return TCL_ERROR;
	}
	Tcl_DeleteHashEntry(hPtr);
    }

    if (hash.numEntries != 0) {
	Tcl_AppendResult(interp, "non-zero final size", NULL);
	Tcl_DeleteHashTable(&hash);
	return TCL_ERROR;
    }

    Tcl_DeleteHashTable(&hash);
    Tcl_AppendResult(interp, "OK", NULL);
    return TCL_OK;
}

/*
 * Used for testing Tcl_GetInt which is no longer used directly by the
 * core very much.
 */
static int
TestgetintCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int argc,
    const char **argv)
{
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    } else {
	int val, i, total=0;

	for (i=1 ; i<argc ; i++) {
	    if (Tcl_GetInt(interp, argv[i], &val) != TCL_OK) {
		return TCL_ERROR;
	    }
	    total += val;
	}
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(total));
	return TCL_OK;
    }
}

/*
 * Used for determining sizeof(long) at script level.
 */
static int
TestlongsizeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int argc,
    TCL_UNUSED(const char **) /*argv*/)
{
    if (argc != 1) {
	Tcl_AppendResult(interp, "wrong # args", NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(sizeof(long)));
    return TCL_OK;
}

static int
NREUnwind_callback(
    void *data[],
    Tcl_Interp *interp,
    TCL_UNUSED(int) /*result*/)
{
    int none;

    if (data[0] == INT2PTR(-1)) {
        Tcl_NRAddCallback(interp, NREUnwind_callback, &none, INT2PTR(-1),
                INT2PTR(-1), NULL);
    } else if (data[1] == INT2PTR(-1)) {
        Tcl_NRAddCallback(interp, NREUnwind_callback, data[0], &none,
                INT2PTR(-1), NULL);
    } else if (data[2] == INT2PTR(-1)) {
        Tcl_NRAddCallback(interp, NREUnwind_callback, data[0], data[1],
                &none, NULL);
    } else {
        Tcl_Obj *idata[3];
        idata[0] = Tcl_NewWideIntObj(((char *) data[1] - (char *) data[0]));
        idata[1] = Tcl_NewWideIntObj(((char *) data[2] - (char *) data[0]));
        idata[2] = Tcl_NewWideIntObj(((char *) &none   - (char *) data[0]));
        Tcl_SetObjResult(interp, Tcl_NewListObj(3, idata));
    }
    return TCL_OK;
}

static int
TestNREUnwind(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*objv*/)
{
    /*
     * Insure that callbacks effectively run at the proper level during the
     * unwinding of the NRE stack.
     */

    Tcl_NRAddCallback(interp, NREUnwind_callback, INT2PTR(-1), INT2PTR(-1),
            INT2PTR(-1), NULL);
    return TCL_OK;
}


static int
TestNRELevels(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *) /*objv*/)
{
    Interp *iPtr = (Interp *) interp;
    static ptrdiff_t *refDepth = NULL;
    ptrdiff_t depth;
    Tcl_Obj *levels[6];
    size_t i = 0;
    NRE_callback *cbPtr = iPtr->execEnvPtr->callbackPtr;

    if (refDepth == NULL) {
	refDepth = &depth;
    }

    depth = (refDepth - &depth);

    levels[0] = Tcl_NewWideIntObj(depth);
    levels[1] = Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(iPtr->numLevels + 1U)) - 1);
    levels[2] = Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(iPtr->cmdFramePtr->level + 1U)) - 1);
    levels[3] = Tcl_NewWideIntObj((Tcl_WideInt)((Tcl_WideUInt)(iPtr->varFramePtr->level + 1U)) - 1);
    levels[4] = Tcl_NewWideIntObj(iPtr->execEnvPtr->execStackPtr->tosPtr
	    - iPtr->execEnvPtr->execStackPtr->stackWords);

    while (cbPtr) {
	i++;
	cbPtr = cbPtr->nextPtr;
    }
    levels[5] = Tcl_NewWideIntObj(i);

    Tcl_SetObjResult(interp, Tcl_NewListObj(6, levels));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestconcatobjCmd --
 *
 *	This procedure implements the "testconcatobj" command. It is used
 *	to test that Tcl_ConcatObj does indeed return a fresh Tcl_Obj in all
 *	cases and thet it never corrupts its arguments. In other words, that
 *	[Bug 1447328] was fixed properly.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestconcatobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*argc*/,
    TCL_UNUSED(const char **) /*argv*/)
{
    Tcl_Obj *list1Ptr, *list2Ptr, *emptyPtr, *concatPtr, *tmpPtr;
    int result = TCL_OK;
    size_t len;
    Tcl_Obj *objv[3];

    /*
     * Set the start of the error message as obj result; it will be cleared at
     * the end if no errors were found.
     */

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj("Tcl_ConcatObj is unsafe:", TCL_INDEX_NONE));

    emptyPtr = Tcl_NewObj();

    list1Ptr = Tcl_NewStringObj("foo bar sum", TCL_INDEX_NONE);
    Tcl_ListObjLength(NULL, list1Ptr, &len);
    Tcl_InvalidateStringRep(list1Ptr);

    list2Ptr = Tcl_NewStringObj("eeny meeny", TCL_INDEX_NONE);
    Tcl_ListObjLength(NULL, list2Ptr, &len);
    Tcl_InvalidateStringRep(list2Ptr);

    /*
     * Verify that concat'ing a list obj with one or more empty strings does
     * return a fresh Tcl_Obj (see also [Bug 2055782]).
     */

    tmpPtr = Tcl_DuplicateObj(list1Ptr);

    objv[0] = tmpPtr;
    objv[1] = emptyPtr;
    concatPtr = Tcl_ConcatObj(2, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (a) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (a) concatObj is not a new obj ",
		NULL);
	switch (tmpPtr->refCount) {
	case 0:
	    Tcl_AppendResult(interp, "(no new refCount)", NULL);
	    break;
	case 1:
	    Tcl_AppendResult(interp, "(refCount added)", NULL);
	    break;
	default:
	    Tcl_AppendResult(interp, "(more than one refCount added!)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[0] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    Tcl_IncrRefCount(tmpPtr);
    concatPtr = Tcl_ConcatObj(2, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (b) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (b) concatObj is not a new obj ",
		NULL);
	switch (tmpPtr->refCount) {
	case 0:
	    Tcl_AppendResult(interp, "(refCount removed?)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	    break;
	case 1:
	    Tcl_AppendResult(interp, "(no new refCount)", NULL);
	    break;
	case 2:
	    Tcl_AppendResult(interp, "(refCount added)", NULL);
	    Tcl_DecrRefCount(tmpPtr);
	    break;
	default:
	    Tcl_AppendResult(interp, "(more than one refCount added!)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[0] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    objv[0] = emptyPtr;
    objv[1] = tmpPtr;
    objv[2] = emptyPtr;
    concatPtr = Tcl_ConcatObj(3, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (c) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (c) concatObj is not a new obj ",
		NULL);
	switch (tmpPtr->refCount) {
	case 0:
	    Tcl_AppendResult(interp, "(no new refCount)", NULL);
	    break;
	case 1:
	    Tcl_AppendResult(interp, "(refCount added)", NULL);
	    break;
	default:
	    Tcl_AppendResult(interp, "(more than one refCount added!)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[1] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    Tcl_IncrRefCount(tmpPtr);
    concatPtr = Tcl_ConcatObj(3, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (d) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (d) concatObj is not a new obj ",
		NULL);
	switch (tmpPtr->refCount) {
	case 0:
	    Tcl_AppendResult(interp, "(refCount removed?)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	    break;
	case 1:
	    Tcl_AppendResult(interp, "(no new refCount)", NULL);
	    break;
	case 2:
	    Tcl_AppendResult(interp, "(refCount added)", NULL);
	    Tcl_DecrRefCount(tmpPtr);
	    break;
	default:
	    Tcl_AppendResult(interp, "(more than one refCount added!)", NULL);
	    Tcl_Panic("extremely unsafe behaviour by Tcl_ConcatObj()");
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[1] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    /*
     * Verify that an unshared list is not corrupted when concat'ing things to
     * it.
     */

    objv[0] = tmpPtr;
    objv[1] = list2Ptr;
    concatPtr = Tcl_ConcatObj(2, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (e) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (e) concatObj is not a new obj ",
		NULL);

	(void) Tcl_ListObjLength(NULL, concatPtr, &len);
	switch (tmpPtr->refCount) {
	case 3:
	    Tcl_AppendResult(interp, "(failed to concat)", NULL);
	    break;
	default:
	    Tcl_AppendResult(interp, "(corrupted input!)", NULL);
	}
	if (Tcl_IsShared(tmpPtr)) {
	    Tcl_DecrRefCount(tmpPtr);
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[0] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    objv[0] = tmpPtr;
    objv[1] = list2Ptr;
    Tcl_IncrRefCount(tmpPtr);
    concatPtr = Tcl_ConcatObj(2, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (f) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (f) concatObj is not a new obj ",
		NULL);

	(void) Tcl_ListObjLength(NULL, concatPtr, &len);
	switch (tmpPtr->refCount) {
	case 3:
	    Tcl_AppendResult(interp, "(failed to concat)", NULL);
	    break;
	default:
	    Tcl_AppendResult(interp, "(corrupted input!)", NULL);
	}
	if (Tcl_IsShared(tmpPtr)) {
	    Tcl_DecrRefCount(tmpPtr);
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[0] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    objv[0] = tmpPtr;
    objv[1] = list2Ptr;
    Tcl_IncrRefCount(tmpPtr);
    Tcl_IncrRefCount(tmpPtr);
    concatPtr = Tcl_ConcatObj(2, objv);
    if (concatPtr->refCount != 0) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp,
		"\n\t* (g) concatObj does not have refCount 0", NULL);
    }
    if (concatPtr == tmpPtr) {
	result = TCL_ERROR;
	Tcl_AppendResult(interp, "\n\t* (g) concatObj is not a new obj ",
		NULL);

	(void) Tcl_ListObjLength(NULL, concatPtr, &len);
	switch (tmpPtr->refCount) {
	case 3:
	    Tcl_AppendResult(interp, "(failed to concat)", NULL);
	    break;
	default:
	    Tcl_AppendResult(interp, "(corrupted input!)", NULL);
	}
	Tcl_DecrRefCount(tmpPtr);
	if (Tcl_IsShared(tmpPtr)) {
	    Tcl_DecrRefCount(tmpPtr);
	}
	tmpPtr = Tcl_DuplicateObj(list1Ptr);
	objv[0] = tmpPtr;
    }
    Tcl_DecrRefCount(concatPtr);

    /*
     * Clean everything up. Note that we don't actually know how many
     * references there are to tmpPtr here; in the no-error case, it should be
     * five... [Bug 2895367]
     */

    Tcl_DecrRefCount(list1Ptr);
    Tcl_DecrRefCount(list2Ptr);
    Tcl_DecrRefCount(emptyPtr);
    while (tmpPtr->refCount > 1) {
	Tcl_DecrRefCount(tmpPtr);
    }
    Tcl_DecrRefCount(tmpPtr);

    if (result == TCL_OK) {
	Tcl_ResetResult(interp);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TestgetencpathObjCmd --
 *
 *	This function implements the "testgetencpath" command. It is used to
 *	test Tcl_GetEncodingSearchPath().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestgetencpathObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)		/* Argument strings. */
{
    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_GetEncodingSearchPath());
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsetencpathCmd --
 *
 *	This function implements the "testsetencpath" command. It is used to
 *	test Tcl_SetDefaultEncodingDir().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestsetencpathObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument strings. */
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "defaultDir");
        return TCL_ERROR;
    }

    Tcl_SetEncodingSearchPath(objv[1]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestparseargsCmd --
 *
 *	This procedure implements the "testparseargs" command. It is used to
 *	test that Tcl_ParseArgsObjv does indeed return the right number of
 *	arguments. In other words, that [Bug 3413857] was fixed properly.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestparseargsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Arguments. */
{
    static int foo = 0;
    size_t count = objc;
    Tcl_Obj **remObjv, *result[3];
    Tcl_ArgvInfo argTable[] = {
        {TCL_ARGV_CONSTANT, "-bool", INT2PTR(1), &foo, "booltest", NULL},
        TCL_ARGV_AUTO_REST, TCL_ARGV_AUTO_HELP, TCL_ARGV_TABLE_END
    };

    foo = 0;
    if (Tcl_ParseArgsObjv(interp, argTable, &count, objv, &remObjv)!=TCL_OK) {
        return TCL_ERROR;
    }
    result[0] = Tcl_NewWideIntObj(foo);
    result[1] = Tcl_NewWideIntObj(count);
    result[2] = Tcl_NewListObj(count, remObjv);
    Tcl_SetObjResult(interp, Tcl_NewListObj(3, result));
    Tcl_Free(remObjv);
    return TCL_OK;
}

/**
 * Test harness for command and variable resolvers.
 */

static int
InterpCmdResolver(
    Tcl_Interp *interp,
    const char *name,
    TCL_UNUSED(Tcl_Namespace *),
    TCL_UNUSED(int) /* flags */,
    Tcl_Command *rPtr)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *varFramePtr = iPtr->varFramePtr;
    Proc *procPtr = (varFramePtr->isProcCallFrame & FRAME_IS_PROC) ?
            varFramePtr->procPtr : NULL;
    Namespace *callerNsPtr = varFramePtr->nsPtr;
    Tcl_Command resolvedCmdPtr = NULL;

    /*
     * Just do something special on a cmd literal "z" in two cases:
     *  A)  when the caller is a proc "x", and the proc is either in "::" or in "::ns2".
     *  B) the caller's namespace is "ctx1" or "ctx2"
     */
    if ( (name[0] == 'z') && (name[1] == '\0') ) {
        Namespace *ns2NsPtr = (Namespace *) Tcl_FindNamespace(interp, "::ns2", NULL, 0);

        if (procPtr != NULL
            && ((procPtr->cmdPtr->nsPtr == iPtr->globalNsPtr)
                || (ns2NsPtr != NULL && procPtr->cmdPtr->nsPtr == ns2NsPtr)
                )
            ) {
            /*
             * Case A)
             *
             *    - The context, in which this resolver becomes active, is
             *      determined by the name of the caller proc, which has to be
             *      named "x".
             *
             *    - To determine the name of the caller proc, the proc is taken
             *      from the topmost stack frame.
             *
             *    - Note that the context is NOT provided during byte-code
             *      compilation (e.g. in TclProcCompileProc)
             *
             *   When these conditions hold, this function resolves the
             *   passed-in cmd literal into a cmd "y", which is taken from the
             *   the global namespace (for simplicity).
             */

            const char *callingCmdName =
                Tcl_GetCommandName(interp, (Tcl_Command) procPtr->cmdPtr);

            if ( callingCmdName[0] == 'x' && callingCmdName[1] == '\0' ) {
                resolvedCmdPtr = Tcl_FindCommand(interp, "y", NULL, TCL_GLOBAL_ONLY);
            }
        } else if (callerNsPtr != NULL) {
            /*
             * Case B)
             *
             *    - The context, in which this resolver becomes active, is
             *      determined by the name of the parent namespace, which has
             *      to be named "ctx1" or "ctx2".
             *
             *    - To determine the name of the parent namesace, it is taken
             *      from the 2nd highest stack frame.
             *
             *    - Note that the context can be provided during byte-code
             *      compilation (e.g. in TclProcCompileProc)
             *
             *   When these conditions hold, this function resolves the
             *   passed-in cmd literal into a cmd "y" or "Y" depending on the
             *   context. The resolved procs are taken from the the global
             *   namespace (for simplicity).
             */

            CallFrame *parentFramePtr = varFramePtr->callerPtr;
            const char *context = parentFramePtr != NULL ? parentFramePtr->nsPtr->name : "(NULL)";

            if (strcmp(context, "ctx1") == 0 && (name[0] == 'z') && (name[1] == '\0')) {
                resolvedCmdPtr = Tcl_FindCommand(interp, "y", NULL, TCL_GLOBAL_ONLY);
                /* fprintf(stderr, "... y ==> %p\n", resolvedCmdPtr);*/

            } else if (strcmp(context, "ctx2") == 0 && (name[0] == 'z') && (name[1] == '\0')) {
                resolvedCmdPtr = Tcl_FindCommand(interp, "Y", NULL, TCL_GLOBAL_ONLY);
                /*fprintf(stderr, "... Y ==> %p\n", resolvedCmdPtr);*/
            }
        }

        if (resolvedCmdPtr != NULL) {
            *rPtr = resolvedCmdPtr;
            return TCL_OK;
        }
    }
    return TCL_CONTINUE;
}

static int
InterpVarResolver(
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(const char *),
    TCL_UNUSED(Tcl_Namespace *),
    TCL_UNUSED(int),
    TCL_UNUSED(Tcl_Var *))
{
    /*
     * Don't resolve the variable; use standard rules.
     */

    return TCL_CONTINUE;
}

typedef struct MyResolvedVarInfo {
    Tcl_ResolvedVarInfo vInfo;  /* This must be the first element. */
    Tcl_Var var;
    Tcl_Obj *nameObj;
} MyResolvedVarInfo;

static inline void
HashVarFree(
    Tcl_Var var)
{
    if (VarHashRefCount(var) < 2) {
        Tcl_Free(var);
    } else {
        VarHashRefCount(var)--;
    }
}

static void
MyCompiledVarFree(
    Tcl_ResolvedVarInfo *vInfoPtr)
{
    MyResolvedVarInfo *resVarInfo = (MyResolvedVarInfo *) vInfoPtr;

    Tcl_DecrRefCount(resVarInfo->nameObj);
    if (resVarInfo->var) {
        HashVarFree(resVarInfo->var);
    }
    Tcl_Free(vInfoPtr);
}

#define TclVarHashGetValue(hPtr) \
    ((Var *) ((char *)hPtr - offsetof(VarInHash, entry)))

static Tcl_Var
MyCompiledVarFetch(
    Tcl_Interp *interp,
    Tcl_ResolvedVarInfo *vinfoPtr)
{
    MyResolvedVarInfo *resVarInfo = (MyResolvedVarInfo *) vinfoPtr;
    Tcl_Var var = resVarInfo->var;
    int isNewVar;
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;

    if (var != NULL) {
        if (!(((Var *) var)->flags & VAR_DEAD_HASH)) {
            /*
             * The cached variable is valid, return it.
             */

            return var;
        }

        /*
         * The variable is not valid anymore. Clean it up.
         */

        HashVarFree(var);
    }

    hPtr = Tcl_CreateHashEntry((Tcl_HashTable *) &iPtr->globalNsPtr->varTable,
            resVarInfo->nameObj, &isNewVar);
    if (hPtr) {
        var = (Tcl_Var) TclVarHashGetValue(hPtr);
    } else {
        var = NULL;
    }
    resVarInfo->var = var;

    /*
     * Increment the reference counter to avoid Tcl_Free() of the variable in
     * Tcl's FreeVarEntry(); for cleanup, we provide our own HashVarFree();
     */

    VarHashRefCount(var)++;
    return var;
}

static int
InterpCompiledVarResolver(
    TCL_UNUSED(Tcl_Interp *),
    const char *name,
    TCL_UNUSED(Tcl_Size) /* length */,
    TCL_UNUSED(Tcl_Namespace *),
    Tcl_ResolvedVarInfo **rPtr)
{
    if (*name == 'T') {
 	MyResolvedVarInfo *resVarInfo = (MyResolvedVarInfo *)Tcl_Alloc(sizeof(MyResolvedVarInfo));

 	resVarInfo->vInfo.fetchProc = MyCompiledVarFetch;
 	resVarInfo->vInfo.deleteProc = MyCompiledVarFree;
 	resVarInfo->var = NULL;
 	resVarInfo->nameObj = Tcl_NewStringObj(name, TCL_INDEX_NONE);
 	Tcl_IncrRefCount(resVarInfo->nameObj);
 	*rPtr = &resVarInfo->vInfo;
 	return TCL_OK;
    }
    return TCL_CONTINUE;
}

static int
TestInterpResolverCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *const table[] = {
        "down", "up", NULL
    };
    int idx;
#define RESOLVER_KEY "testInterpResolver"

    if ((objc < 2) || (objc > 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "up|down ?interp?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	interp = Tcl_GetChild(interp, Tcl_GetString(objv[2]));
	if (interp == NULL) {
	    Tcl_AppendResult(interp, "provided interpreter not found", NULL);
	    return TCL_ERROR;
	}
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], table, "operation", TCL_EXACT,
            &idx) != TCL_OK) {
        return TCL_ERROR;
    }
    switch (idx) {
    case 1: /* up */
        Tcl_AddInterpResolvers(interp, RESOLVER_KEY, InterpCmdResolver,
                InterpVarResolver, InterpCompiledVarResolver);
        break;
    case 0: /*down*/
        if (!Tcl_RemoveInterpResolvers(interp, RESOLVER_KEY)) {
            Tcl_AppendResult(interp, "could not remove the resolver scheme",
                    NULL);
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * TestApplyLambdaObjCmd --
 *
 *	Implements the Tcl command testapplylambda. This tests the apply
 *	implementation handling of a lambda where the lambda has a list
 *	internal representation where the second element's internal
 *	representation is already a byte code object.
 *
 * Results:
 *	TCL_OK    - Success. Caller should check result is 42
 *	TCL_ERROR - Error.
 *
 * Side effects:
 *	In the presence of the apply bug, may panic. Otherwise
 *	Interpreter result holds result or error message.
 *
 *------------------------------------------------------------------------
 */
int TestApplyLambdaObjCmd (
    TCL_UNUSED(void*),
    Tcl_Interp *interp,    /* Current interpreter. */
    TCL_UNUSED(int),       /* objc. */
    TCL_UNUSED(Tcl_Obj *const *)) /* objv. */
{
    Tcl_Obj *lambdaObjs[2];
    Tcl_Obj *evalObjs[2];
    Tcl_Obj *lambdaObj;
    int result;

    /* Create a lambda {{} {set a 42}} */
    lambdaObjs[0] = Tcl_NewObj(); /* No parameters */
    lambdaObjs[1] = Tcl_NewStringObj("set a 42", TCL_INDEX_NONE); /* Body */
    lambdaObj = Tcl_NewListObj(2, lambdaObjs);
    Tcl_IncrRefCount(lambdaObj);

    /* Create the command "apply {{} {set a 42}" */
    evalObjs[0] = Tcl_NewStringObj("apply", TCL_INDEX_NONE);
    Tcl_IncrRefCount(evalObjs[0]);
    /*
     * NOTE: IMPORTANT TO EXHIBIT THE BUG. We duplicate the lambda because
     * it will get shimmered to a Lambda internal representation but we
     * want to hold on to our list representation.
     */
    evalObjs[1] = Tcl_DuplicateObj(lambdaObj);
    Tcl_IncrRefCount(evalObjs[1]);

    /* Evaluate it */
    result = Tcl_EvalObjv(interp, 2, evalObjs, TCL_EVAL_GLOBAL);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(evalObjs[0]);
	Tcl_DecrRefCount(evalObjs[1]);
	return result;
    }
    /*
     * So far so good. At this point,
     * - evalObjs[1] has an internal representation of Lambda
     * - lambdaObj[1] ({set a 42}) has been shimmered to
     * an internal representation of ByteCode.
     */
    Tcl_DecrRefCount(evalObjs[1]); /* Don't need this anymore */
    /*
     * The bug trigger. Repeating the command but:
     *  - we are calling apply with a lambda that is a list (as BEFORE),
     *    BUT
     *  - The body of the lambda (lambdaObjs[1]) ALREADY has internal
     *    representation of ByteCode and thus will not be compiled again
     */
    evalObjs[1] = lambdaObj; /* lambdaObj already has a ref count so
     				no need for IncrRef */
    result = Tcl_EvalObjv(interp, 2, evalObjs, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(evalObjs[0]);
    Tcl_DecrRefCount(lambdaObj);

    return result;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 */

