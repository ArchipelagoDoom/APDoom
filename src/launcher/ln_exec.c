
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
// ----- Windows --------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <shellapi.h>
// ----------------------------------------------------------------------------
#else
// ----- Linux / Mac / Other OSes ---------------------------------------------
#include <unistd.h>
#include <sys/wait.h>
// ----------------------------------------------------------------------------
#endif

#include "m_argv.h"
#include "m_misc.h"
#include "i_system.h"
#include "li_event.h"
#include "ln_exec.h"
#include "ln_util.h"
#include "lv_video.h"

#include "apdoom.h"

gamesettings_t exec_settings = {
    "", "archipelago.gg:", "", false,
    -1, -1, -1, -1, -1, -1, -1,
    ""
};

// ============================================================================

// One additional to guarantee NULL.
static const char *arglist[65] = {NULL};
static unsigned char argquote[64];
static int argcount = 0;

static inline void SetupArgs(const char *program)
{
    memset(argquote, 0, sizeof(argquote));
    argquote[0] = true;
    arglist[0] = program;
    arglist[1] = NULL;
    argcount = 1;
}

static inline void AddArg(const char *str)
{
    arglist[argcount++] = str;
    arglist[argcount] = NULL;
}

static inline void AddArgParam(const char *param, const char *value)
{
    arglist[argcount++] = param;
    argquote[argcount] = true;
    arglist[argcount++] = value;
    arglist[argcount] = NULL;
}

static inline void AddMultipleArgs(char *str)
{
    char *token = strtok(str, " ");
    while (token && argcount < 64)
    {
        arglist[argcount++] = token;
        token = strtok(NULL, " ");
    }
    arglist[argcount] = NULL;
}

static const char *GetBaseProgram(const char *iwad)
{
    if (!iwad)
        return "apdoom-setup";
    else if (!strcmp(iwad, "HERETIC.WAD"))
        return PROGRAM_PREFIX "heretic";
    else if (!strcmp(iwad, "HEXEN.WAD"))
        return PROGRAM_PREFIX "hexen";
    return PROGRAM_PREFIX "doom";
}

// ----------------------------------------------------------------------------

static char *strslots[10];
static int strslotused = 0;

static void FreeExecStrings(void)
{
    for (int i = 0; i < strslotused; ++i)
        free(strslots[i]);
    strslotused = 0;
}

static char *MakeHexString(const char *str)
{
    static const char hexchars[] = "0123456789ABCDEF";

    char *hexstr = malloc((strlen(str) * 2) + 1);
    char *hexp = hexstr;

    for (; *str; ++str)
    {
        *hexp++ = hexchars[(*str & 0xF0) >> 4];
        *hexp++ = hexchars[(*str & 0x0F)];
    }
    *hexp = 0;

    strslots[strslotused++] = hexstr;
    return hexstr;
}

static char *MakeIntString(int value)
{
    char *newstr = LN_allocsprintf("%d", value);
    strslots[strslotused++] = newstr;
    return newstr;
}

static char *MakeDupString(const char *str)
{
    char *newstr = M_StringDuplicate(str);
    strslots[strslotused++] = newstr;
    return newstr;
}

// ============================================================================
// Generic Execution Code
// ============================================================================

static char *tmp_initfile = NULL;
static char initfile_buf[33];

static const char *InitFileResult(void)
{
    if (initfile_buf[0])
        return initfile_buf;

    FILE *f_init = fopen(tmp_initfile, "r");
    if (!f_init)
        return NULL;

    fread(&initfile_buf, 1, 32, f_init);
    initfile_buf[32] = 0;
    fclose(f_init);

    return initfile_buf;
}

static void CommonPostExecLoop(int has_init_file, int (*waitfunc)(void))
{
    if (!has_init_file)
    {
        // If we're not checking an init file (e.g., entering setup)
        // then we have no reason to do anything but just immediately minimize
        LV_EnterMinimalMode(waitfunc);
        LI_Reset();
        return;
    }

    int waitdone = false;
    int initready = false;
    uint64_t warningtime = SDL_GetTicks64() + 16000;

    LN_OpenDialog(DIALOG_EMPTY, "Starting...", "Starting game, please wait...");
    memset(initfile_buf, 0, sizeof(initfile_buf));

    do
    {
        waitdone = waitfunc();
        initready = (InitFileResult() != NULL);

        if (warningtime && SDL_GetTicks64() > warningtime)
        {
            warningtime = 0;
            LN_OpenDialog(DIALOG_EMPTY, "Starting...",
                "Starting game, please wait...\n"
                "\n"
                "The game has been starting for an exceptionally long time, but "
                "has not reported a connection timeout yet.\n"
                "\n"
                "If you are connecting to a very large multiworld for the first "
                "time, this is normal. Otherwise, some other program (such as "
                "an anti-virus) may be preventing the game from starting.");
        }

        LI_HandleEvents();
        LV_RenderFrame();
    } while (!waitdone && !initready);

    if (!initready) // Abnormal execution failure?
        return; // Should be handled in Exec functions, to check exit code

    const char *init_result = InitFileResult();

    if (!strcmp(init_result, "OK"))
    {
        // Execution successful, drop to minimal mode to reduce resource use
        LN_CloseDialog();
        LV_EnterMinimalMode(waitfunc);
        LI_Reset();
        return;
    }

    char *error_reason;

    if (!strcmp(init_result, "ConnectFailed"))
    {
        error_reason = LN_allocsprintf(
            "Couldn't connect to the Archipelago server at \xF2%s\xF0.\n"
            "\n"
            "Check the address and port for typos, and then try again.", exec_settings.address);
    }
    else if (!strcmp(init_result, "InvalidSlot"))
    {
        error_reason = LN_allocsprintf(
            "The server reports that the slot name \xF2%s\xF0 "
            "does not match any players in the MultiWorld.\n"
            "\n"
            "Check the slot name for typos, and then try again.", exec_settings.slot_name);
    }
    else if (!strcmp(init_result, "InvalidGame"))
    {
        error_reason = LN_allocsprintf(
            "The server reports that the slot name \xF2%s\xF0 "
            "is not playing the game that you attempted to connect with.\n"
            "\n"
            "Make sure you are connecting to the correct MultiWorld and/or slot.", exec_settings.slot_name);
    }
    else if (!strcmp(init_result, "IncompatibleVersion"))
    {
        error_reason = LN_allocsprintf(
            "The server reports that the the version of the client that you are "
            "trying to connect with is incompatible with the server.\n"
            "\n"
            "You may need to update APDoom in order to connect.");
    }
    else if (!strcmp(init_result, "InvalidPassword"))
    {
        error_reason = LN_allocsprintf(
            "The server reports that the password you entered was not valid.\n"
            "\n"
            "Check the password for typos, and then try again.");
    }
    else if (!strcmp(init_result, "OldWorldVersion"))
    {
        error_reason = LN_allocsprintf(
            "You are trying to connect to a slot for an older version of APDoom, "
            "which is not supported by APDoom 2.0 or later.\n"
            "\n"
            "Please connect to this slot using APDoom 1.2.0.");
    }
    else
    {
        error_reason = LN_allocsprintf("An unknown error code was returned by APDoom.");
    }

    if (error_reason)
    {
        LN_OpenDialog(DIALOG_OK, "Error", error_reason);
        free(error_reason);
    }
}

// ============================================================================
// OS-Specific Execution Code
// ============================================================================

#ifdef _WIN32
// ----- Windows --------------------------------------------------------------

HANDLE child_process;

// utility function from setup/execute.c
static void ConcatWCString(wchar_t *buf, const char *value)
{
    MultiByteToWideChar(CP_OEMCP, 0,
                        value, strlen(value) + 1,
                        buf + wcslen(buf), strlen(value) + 1);
}

static wchar_t *BuildCommandLine(void)
{
    wchar_t exe_path[MAX_PATH];
    wchar_t *sep;
    size_t path_len = 0;

    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    // Get length of command line that we're about to make.
    // Length of each argument, plus space, plus two quote characters if needed.
    // Overestimates a bit because of exe_path, which we later trim, but whatever
    path_len = wcslen(exe_path);
    for (int i = 0; i < argcount; ++i)
        path_len += strlen(arglist[i]) + (argquote[i] ? 3 : 1);

    wchar_t* command_path = calloc(path_len, sizeof(wchar_t));

    wcscat(command_path, L"\"");
    if ((sep = wcsrchr(exe_path, DIR_SEPARATOR)) != NULL)
    {
        wcsncpy(command_path + 1, exe_path, sep - exe_path + 1);
        command_path[sep - exe_path + 2] = '\0';
    }
    ConcatWCString(command_path, arglist[0]);
    wcscat(command_path, L"\"");

    for (int i = 1; i < argcount; ++i)
    {
        wcscat(command_path, L" ");
        if (argquote[i]) wcscat(command_path, L"\"");
        ConcatWCString(command_path, arglist[i]);
        if (argquote[i]) wcscat(command_path, L"\"");
    }

    return command_path;
}

static int WaitOnChild(void)
{
    return WaitForSingleObject(child_process, 0) == WAIT_OBJECT_0;
}

static int DoExecute(int has_init_file)
{
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    wchar_t* command = BuildCommandLine();
    DWORD exit_code = -777;
    BOOL process_return;

    memset(&process_info, 0, sizeof(process_info));
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);

    process_return = CreateProcessW(
        NULL, /* lpApplicationName */
        command, /* lpCommandLine */
        NULL, /* lpProcessAttributes */
        NULL, /* lpThreadAttributes */
        FALSE, /* bInheritHandles */
        0, /* dwCreationFlags */
        NULL, /* lpEnvironment*/
        NULL, /* lpCurrentDirectory */
        &startup_info, /* lpStartupInfo */
        &process_info); /* lpProcessInformation */

    if (!process_return)
    {
        DWORD error = GetLastError();
        fprintf(stderr, "CreateProcessW failed (%i).\n", error);
        free(command);
        return -666;
    }

    child_process = process_info.hProcess;
    CommonPostExecLoop(has_init_file, WaitOnChild);
    child_process = NULL;

    GetExitCodeProcess(process_info.hProcess, &exit_code);

    free(command);
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return (int)exit_code;
}

static char *GetProgram(const char *iwad)
{
    return M_StringJoin(GetBaseProgram(iwad), ".exe", NULL);
}

// ----------------------------------------------------------------------------
#else
// ----- Linux / Mac / Other OSes ---------------------------------------------

static pid_t child_pid = 0;
static int child_status = 0;

static int WaitOnChild(void)
{
    return waitpid(child_pid, &child_status, WNOHANG) != 0;
}

static int DoExecute(int has_init_file)
{
    child_pid = fork();
    if (child_pid == 0) // Child process
    {
        execvp(arglist[0], (char **)arglist);
        _exit(0x80); // Unreachable unless exec failed
    }
    else // Parent process
    {
        CommonPostExecLoop(has_init_file, WaitOnChild);

        if (WIFSIGNALED(child_status))
            return -WTERMSIG(child_status);
        else if (WIFEXITED(child_status) && WEXITSTATUS(child_status) != 0x80)
            return WEXITSTATUS(child_status);
        else
            return -666;
    }
}

static char *GetProgram(const char *iwad)
{
    // We want to execute from the same path as the launcher, relative "./" doesn't cut it.
    char *orig_path = M_StringDuplicate(myargv[0]);
    char *separator = strrchr(orig_path, DIR_SEPARATOR);

    if (separator == NULL)
    {
        free(orig_path);
        return M_StringDuplicate(GetBaseProgram(iwad));
    }

    *separator = 0; // Turn the separator into a null terminator
    char *result = M_StringJoin(orig_path, DIR_SEPARATOR_S, GetBaseProgram(iwad), NULL);
    free(orig_path);

    return result;
}

// ----------------------------------------------------------------------------
#endif

// ============================================================================
// Public Functions
// ============================================================================

void LN_ExecuteWorld(const ap_worldinfo_t *world)
{
    char *program = GetProgram(world->iwad);

    char *tmpfilebase = LN_allocsprintf(".apdoom-init-%08x.tmp", rand());
    tmp_initfile = M_TempFile(tmpfilebase);
    free(tmpfilebase);

    if (M_FileExists(tmp_initfile))
        M_remove(tmp_initfile);

    printf("Temp file: %s\n", tmp_initfile);

    SetupArgs(program);
    AddArgParam("-apinitfile", tmp_initfile);
    AddArgParam("-game", world->shortname);

    if (exec_settings.practice_mode)
    {
        AddArg("-practice");
    }
    else
    {
        AddArgParam("-applayerhex", MakeHexString(exec_settings.slot_name));
        AddArgParam("-apserver", exec_settings.address);

        if (exec_settings.password[0])
            AddArgParam("-password", exec_settings.password);

        if (exec_settings.no_deathlink > 0)
            AddArg("-apdeathlinkoff");
    }

    if (exec_settings.skill >= 0)
        AddArgParam("-skill", MakeIntString(exec_settings.skill));
    if (exec_settings.monster_rando >= 0)
        AddArgParam("-apmonsterrando", MakeIntString(exec_settings.monster_rando));
    if (exec_settings.item_rando >= 0)
        AddArgParam("-apitemrando", MakeIntString(exec_settings.item_rando));
    if (exec_settings.music_rando >= 0)
        AddArgParam("-apmusicrando", MakeIntString(exec_settings.music_rando));
    if (exec_settings.flip_levels >= 0)
        AddArgParam("-apfliplevels", MakeIntString(exec_settings.flip_levels));
    if (exec_settings.reset_level >= 0)
        AddArgParam("-apresetlevelondeath", MakeIntString(exec_settings.reset_level));

    if (exec_settings.extra_cmdline[0])
        AddMultipleArgs(MakeDupString(exec_settings.extra_cmdline));

    int code = DoExecute(true);
    switch (code)
    {
    case 0: // Successful exit
        if (!initfile_buf[0])
        {
            // Successful exit without making an init file.
            // Likely some command that aborts early was used.
            LN_OpenDialog(DIALOG_OK, "Closed", "Your command executed successfully.");
        }
        break;
    case -666: // Couldn't execute program
        {
            char *reason = LN_allocsprintf(
                "The program \xF2%s\xF0 could not be executed.\n"
                "\n"
                "Please check your installation of APDoom for missing files, "
                "and make sure the program is not blocked from executing by "
                "the Operating System, an antivirus, or some other program.", GetBaseProgram(world->iwad));
            LN_OpenDialog(DIALOG_OK, "Error", reason);
            free(reason);
        }
        break;
    default:
        // Unexpected exit?
        // If we reached the point of making an init file, then it's likely just a crash.
        // Otherwise we failed to start, so alert.
        if (!initfile_buf[0])
        {
            char *reason = LN_allocsprintf(
                "The program \xF2%s\xF0 exited unexpectedly before initializing Archipelago.\n"
                "\n"
                "Please check your installation of APDoom for missing files, "
                "and make sure the program is not blocked from executing by "
                "the Operating System, an antivirus, or some other program.\n"
                "\n"
                "The terminal may have more information about the exact nature "
                "of the error.", GetBaseProgram(world->iwad));
            LN_OpenDialog(DIALOG_OK, "Error", reason);
            free(reason);
        }
        break;
    }

    if (M_FileExists(tmp_initfile))
        M_remove(tmp_initfile);

    while (dialog_open)
    {
        LI_HandleEvents();
        LN_HandleDialog();
        LV_RenderFrame();
    }

    FreeExecStrings();
    free(program);
    free(tmp_initfile);
    tmp_initfile = NULL;
}

void LN_ExecuteSetup(void)
{
    char *program = GetProgram(NULL);

    SetupArgs(program);
    int code = DoExecute(false);
    if (code == -666)
    {
        char *reason = LN_allocsprintf(
            "The setup program \xF2%s\xF0 could not be executed.\n"
            "\n"
            "Please check your installation of APDoom for missing files, "
            "and make sure the program is not blocked from executing by "
            "the Operating System, an anti-virus, or some other program.", GetBaseProgram(NULL));
        LN_OpenDialog(DIALOG_OK, "Error", reason);
        free(reason);
    }

    while (dialog_open)
    {
        LI_HandleEvents();
        LN_HandleDialog();
        LV_RenderFrame();
    }

    free(program);
}

void NORETURN LN_ImmediateExecute(const ap_worldinfo_t *world)
{
    char *program = GetProgram(world->iwad);
    SetupArgs(program);

    for (int i = 1; i < myargc; ++i)
        AddArg(myargv[i]);

    int code = DoExecute(false);
    if (code == -666)
        I_Error("LN_ImmediateExecute: couldn't execute %s", program);

    free(program);
    I_Quit();
}
