#include <janet.h>
#include <unistd.h>
#include <assert.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <wordexp.h>
#include <linenoise.h>

#define panic_errno(NAME, e) \
  do { \
    janet_setdyn("errno", janet_wrap_integer(e));\
    janet_panicf(NAME ": %s", strerror(e));\
  } while(0)

static Janet jfork_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 0);
  pid_t pid = fork();
  if (pid == -1)
    panic_errno("fork", errno);
  assert(sizeof(int32_t) == sizeof(pid_t));
  return janet_wrap_integer(pid);
}

static Janet isatty_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  int fd = janet_getnumber(argv, 0);
  int r = isatty(fd);
  if (r == 0 && errno != ENOTTY)
    panic_errno("fork", errno);
  return janet_wrap_boolean(r);
}

static Janet getpid_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 0);
  pid_t pid = getpid();
  if (pid == -1)
    panic_errno("getpid", errno);
  return janet_wrap_integer(pid);
}

static Janet setpgid_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  pid_t pid = setpgid((pid_t)janet_getnumber(argv, 0), (pid_t)janet_getnumber(argv, 1));
  if (pid == -1)
     panic_errno("setpgid", errno);
  return janet_wrap_nil();
}

static Janet getpgrp_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 0);
  pid_t pid = getpgrp();
  if (pid == -1)
    panic_errno("getpgrp", errno);
  return janet_wrap_integer(pid);
}

static Janet tcgetpgrp_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  pid_t pid = tcgetpgrp((pid_t)janet_getnumber(argv, 0));
  if (pid == -1)
    panic_errno("tcgetpgrp", errno);
  return janet_wrap_integer(pid);
}

static Janet tcsetpgrp_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  pid_t pid = tcsetpgrp((int)janet_getnumber(argv, 0), (pid_t)janet_getnumber(argv, 1));
  if (pid == -1)
    panic_errno("tcsetpgrp", errno);
  return janet_wrap_nil();
}

static Janet signal_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  void *h = signal((pid_t)janet_getnumber(argv, 0), (__sighandler_t)janet_getpointer(argv, 1));
  return janet_wrap_pointer(h);
}

static Janet kill_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  pid_t pid = kill((pid_t)janet_getnumber(argv, 0), (int)janet_getnumber(argv, 1));
  if (pid == -1)
    panic_errno("kill", errno);
  return janet_wrap_integer(pid);
}

static Janet exec(int32_t argc, Janet *argv) {
    janet_arity(argc, 1, -1);
    const char **child_argv = malloc(sizeof(char *) * (argc + 1));
    if (!child_argv)
      abort();
    for (int32_t i = 0; i < argc; i++)
        child_argv[i] = janet_getcstring(argv, i);
    child_argv[argc] = NULL;
    execvp(child_argv[0], (char **)child_argv);
    int e = errno;
    free(child_argv);
    panic_errno("execvp", e);
    abort();
}

static Janet dup2_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  
  if (dup2((int)janet_getnumber(argv, 0), (int)janet_getnumber(argv, 1)) == -1)
     panic_errno("dup2", errno);
  
  return janet_wrap_nil();
}

static Janet pipe_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 0);

  int mypipe[2];
  if (pipe(mypipe) < 0)
    panic_errno("pipe", errno);

  Janet *t = janet_tuple_begin(2);
  t[0] = janet_wrap_number((int)mypipe[0]);
  t[1] = janet_wrap_number((int)mypipe[1]);
  return janet_wrap_tuple(janet_tuple_end(t));
}

static Janet open_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 3);
  int fd = open(janet_getcstring(argv, 0), janet_getinteger(argv, 1), janet_getinteger(argv, 2));
  if (fd == -1)
    panic_errno("open", errno);
    
  return janet_wrap_integer(fd);
}

static Janet read_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  int fd = janet_getinteger(argv, 0);
  JanetBuffer *buf = janet_getbuffer(argv, 1);
  int n = read(fd, buf->data, buf->count);
  if (fd == -1)
    panic_errno("read", errno);
    
  return janet_wrap_integer(n);
}

static Janet close_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  if (close((int)janet_getnumber(argv, 0)) == -1)
    panic_errno("close", errno);
  return janet_wrap_nil();
}

static Janet waitpid_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  int status;
  pid_t pid = waitpid(janet_getinteger(argv, 0), &status, janet_getinteger(argv, 1));
  if (pid == -1)
    panic_errno("waitpid", errno);
  
  Janet *t = janet_tuple_begin(2);
  t[0] = janet_wrap_number(pid);
  t[1] = janet_wrap_number(status);
  return janet_wrap_tuple(janet_tuple_end(t));
}

static Janet wordexp_(int32_t argc, Janet *argv) {
  wordexp_t p;

  janet_fixarity(argc, 1);

  switch (wordexp(janet_getcstring(argv, 0), &p, WRDE_NOCMD | WRDE_UNDEF)) {
  case 0:
    break;
  case WRDE_BADCHAR:
    janet_panic("wordexp: Illegal occurrence of newline or one of |, &, ;, <, >, (, ),{, }.");
  case WRDE_BADVAL:
    janet_panic("wordexp: An undefined shell variable was referenced.");
  case WRDE_CMDSUB:
    janet_panic("wordexp: Command substitution not supported.");
  case WRDE_NOSPACE:
    janet_panic("wordexp: Out of memory.");
  case WRDE_SYNTAX:
    janet_panic("wordexp: Syntax error.");
  default:
    janet_panic("wordexp: Unknown error.");
  }

  char **w = p.we_wordv;
  
  JanetArray *a = janet_array(p.we_wordc);

  for (int i = 0; i < p.we_wordc; i++)
     janet_array_push(a, janet_cstringv(w[i]));
  
  wordfree(&p);

  return janet_wrap_array(a);
}

static struct JanetAbstractType Termios_jt = {
    "unixy.termios",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static Janet tcgetattr_(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);
    struct termios *t = janet_abstract(&Termios_jt, sizeof(struct termios));
    if(tcgetattr(janet_getinteger(argv, 0), t) == -1)
        janet_panic("tcgetattr error");
    return janet_wrap_abstract(t);
}

static Janet tcsetattr_(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 3);
    int fd = janet_getinteger(argv, 0);
    int actions = janet_getinteger(argv, 1);
    struct termios *t = janet_getabstract(argv, 2, &Termios_jt);
    if(tcsetattr(fd, actions, t) == -1)
        janet_panic("tcsetattr error");
    return janet_wrap_nil();
}

static Janet linenoise_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  char *ln = linenoise(janet_getcstring(argv, 0));
  if (!ln)
    return janet_wrap_nil();
  Janet jln = janet_cstringv(ln);
  linenoiseFree(ln);
  return jln;
}

static Janet linenoiseSetMultiLine_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  linenoiseSetMultiLine(janet_getboolean(argv, 0));
  return janet_wrap_nil();
}


static Janet linenoiseClearScreen_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 0);
  linenoiseClearScreen();
  return janet_wrap_nil();
}

static Janet linenoiseHistoryAdd_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  return janet_wrap_integer(linenoiseHistoryAdd(janet_getcstring(argv, 0)));
}

static Janet linenoiseHistorySetMaxLen_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  if (!linenoiseHistorySetMaxLen(janet_getinteger(argv, 0)))
    janet_panic("linenoiseHistorySetMaxLen: error");
  return janet_wrap_nil();
}

static Janet linenoiseHistorySave_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  if (linenoiseHistorySave(janet_getcstring(argv, 0)) != 0)
    janet_panic("linenoiseHistorySave: error");
  return janet_wrap_nil();
}

static Janet linenoiseHistoryLoad_(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  if (linenoiseHistoryLoad(janet_getcstring(argv, 0)) != 0)
    janet_panic("linenoiseHistoryLoad: error");
  return janet_wrap_nil();
}

#define STATUS_FUNC_INT(X) static Janet X##_(int32_t argc, Janet *argv) { \
  janet_fixarity(argc, 1); \
  return janet_wrap_integer(X(janet_getinteger(argv, 0))); \
}

STATUS_FUNC_INT(WEXITSTATUS);

#define STATUS_FUNC_BOOL(X) static Janet X##_(int32_t argc, Janet *argv) { \
  janet_fixarity(argc, 1); \
  return janet_wrap_boolean(X(janet_getinteger(argv, 0))); \
}

STATUS_FUNC_BOOL(WIFEXITED);
STATUS_FUNC_BOOL(WIFSIGNALED);
STATUS_FUNC_BOOL(WIFSTOPPED);

static const JanetReg cfuns[] = {
    // Unistd
    {"fork", jfork_, NULL},
    {"exec", exec, NULL},
    {"isatty", isatty_, NULL},
    {"getpgrp", getpgrp_, NULL},
    {"getpid", getpid_, NULL},
    {"setpgid", setpgid_, NULL},
    {"signal", signal_, NULL},
    {"tcgetpgrp", tcgetpgrp_, NULL},
    {"tcsetpgrp", tcsetpgrp_, NULL},
    {"dup2", dup2_, NULL},
    {"kill", kill_, NULL},
    {"open", open_, NULL},
    {"read", read_, NULL},
    {"close", close_, NULL},
    {"pipe", pipe_, NULL},
    {"waitpid", waitpid_, NULL},
    {"WIFEXITED", WIFEXITED_, NULL},
    {"WIFSIGNALED", WIFSIGNALED_, NULL},
    {"WEXITSTATUS", WEXITSTATUS_, NULL},
    {"WIFSTOPPED", WIFSTOPPED_, NULL},
    
    // Termios    
    {"tcgetattr", tcgetattr_, NULL},
    {"tcsetattr", tcsetattr_, NULL},
    
    // linenoise - Slightly renamed to make it look nicer.
    {"ln/get-line", linenoise_, NULL},
    {"ln/clear-screen", linenoiseClearScreen_, NULL},
    {"ln/set-multiline", linenoiseSetMultiLine_, NULL},
    {"ln/history-set-max-len", linenoiseHistorySetMaxLen_, NULL},
    {"ln/history-add", linenoiseHistoryAdd_, NULL},
    {"ln/history-load", linenoiseHistoryLoad_, NULL},
    {"ln/history-save", linenoiseHistorySave_, NULL},

    // libc
    {"wordexp", wordexp_, NULL},
    {NULL, NULL, NULL}
};

JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns(env, "shlib", cfuns);

    // This code assumes pid_t will fit in a janet number.
    assert(sizeof(int32_t) == sizeof(pid_t));

    #define DEF_CONSTANT_INT(X) janet_def(env, #X, janet_wrap_integer(X), NULL)
    DEF_CONSTANT_INT(STDIN_FILENO);
    DEF_CONSTANT_INT(STDERR_FILENO);
    DEF_CONSTANT_INT(STDOUT_FILENO);
    
    DEF_CONSTANT_INT(SIGINT);
    DEF_CONSTANT_INT(SIGCONT);
    DEF_CONSTANT_INT(SIGQUIT);
    DEF_CONSTANT_INT(SIGTSTP);
    DEF_CONSTANT_INT(SIGTTIN);
    DEF_CONSTANT_INT(SIGTTOU);
    DEF_CONSTANT_INT(SIGCHLD);
    DEF_CONSTANT_INT(SIGTERM);

    DEF_CONSTANT_INT(O_RDONLY);
    DEF_CONSTANT_INT(O_WRONLY);
    DEF_CONSTANT_INT(O_RDWR);
    DEF_CONSTANT_INT(O_APPEND);
    DEF_CONSTANT_INT(O_CREAT);
    DEF_CONSTANT_INT(O_TRUNC);

    DEF_CONSTANT_INT(S_IWUSR);
    DEF_CONSTANT_INT(S_IRUSR);
    DEF_CONSTANT_INT(S_IRGRP);

    DEF_CONSTANT_INT(TCSADRAIN);

    DEF_CONSTANT_INT(WUNTRACED);
    DEF_CONSTANT_INT(WNOHANG);

    DEF_CONSTANT_INT(ECHILD);

    #undef DEF_CONSTANT_INT

    #define DEF_CONSTANT_PTR(X) janet_def(env, #X, janet_wrap_pointer(X), NULL)
    DEF_CONSTANT_PTR(SIG_IGN);
    DEF_CONSTANT_PTR(SIG_DFL);
    #undef DEF_CONSTANT_PTR
}