#ifndef LIBRARIES_H

#define LIBSRARIES_H

#define _XOPEN_SOURCE_EXTENDED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdarg.h>

#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>

#include "common.h"

#define STR_BUF_SIZE 256
#define USER_NAME_MAX_LENGTH 10
#define CHATBOX_BOTTOM_SPAN 4
#define MAX_FAILS 5
#define WAIT_TIME 50
#define FAIL -1

#define INF INT_MAX
#define NONAME "{{{}}}"

#define REP(i, n) int i; for(i = 0; i < (n); i++)
#define FOR(i, a, b) int i; for(i = (a); i < (b); i++)

#endif
