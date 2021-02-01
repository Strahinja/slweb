/*
 *    slweb - Simple static webpage generator
 *    Copyright (C) 2020, 2021 Страхиња Радић
 *
 *    This program is free software: you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the Free
 *    Software Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *    for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef __DEFS_H
#define __DEFS_H

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unistr.h>
#include <unistdio.h>
#include <uniwidth.h>

#define PROGRAMNAME   "slweb"
#define VERSION       "0.3.7"
#define COPYRIGHTYEAR "2020, 2021"

#define BUFSIZE       1024
#define KEYSIZE       256
#define SMALL_ARGSIZE 256
#define DATEBUFSIZE   12

static const char timestamp_format[]     = "d.m.y";
static const char timestamp_output_ext[] = ".html";

static const char CMD_GIT_LOG[]          = "xargs";
static const char* CMD_GIT_LOG_ARGS[]    
    = { "xargs", "-I{}", "git", "log", "-1", 
        "--pretty=format:{} %h %ci (%cn) %d", NULL };

static const char CMD_KATEX[]               = "katex";
static const char* CMD_KATEX_INLINE_ARGS[]  = { "katex", NULL };
static const char* CMD_KATEX_DISPLAY_ARGS[] = { "katex", "-d", NULL };

typedef enum
{
    FALSE = 0,
    TRUE  = 1
} BOOL;

typedef unsigned char UBYTE;
typedef unsigned long ULONG;

typedef enum
{
    CMD_NONE,
    CMD_BODY_ONLY,
    CMD_BASEDIR,
    CMD_HELP,
    CMD_VERSION
} Command;

typedef struct
{
    uint8_t* key;
    uint8_t* value;
    size_t   value_size;
    BOOL     seen;    /* for use with macros */
} KeyValue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#define MAX_HEADING_LEVEL  4
#define MAX_CSV_REGISTERS  9

#define ST_NONE              0
#define ST_YAML              1
#define ST_YAML_VAL          (1 << 1)
#define ST_PARA_OPEN         (1 << 2)
#define ST_TAG               (1 << 3)
#define ST_HEADING           (1 << 4)
#define ST_HEADING_TEXT      (1 << 5)
#define ST_BOLD              (1 << 6)
#define ST_ITALIC            (1 << 7)
#define ST_PRE               (1 << 8)
#define ST_CODE              (1 << 9)
#define ST_BLOCKQUOTE        (1 << 10)
#define ST_LINK              (1 << 11)
#define ST_LINK_SECOND_ARG   (1 << 12)
#define ST_LINK_SPAN         (1 << 13)
#define ST_LINK_MACRO        (1 << 14)
#define ST_IMAGE             (1 << 15)
#define ST_IMAGE_SECOND_ARG  (1 << 16)
#define ST_MACRO_BODY        (1 << 17)
#define ST_CSV_BODY          (1 << 18)
#define ST_HTML_TAG          (1 << 19)
#define ST_KBD               (1 << 20)
#define ST_LIST              (1 << 21)
#define ST_FOOTNOTE          (1 << 22)
#define ST_FOOTNOTE_TEXT     (1 << 23)
#define ST_INLINE_FOOTNOTE   (1 << 24)
#define ST_FORMULA           (1 << 25)
#define ST_DISPLAY_FORMULA   (1 << 26)

#define ST_CS_NONE          0
#define ST_CS_HEADER        1
#define ST_CS_REGISTER      (1 << 2)
#define ST_CS_QUOTE         (1 << 3)
#define ST_CS_COND          (1 << 4)
#define ST_CS_COND_NONEMPTY (1 << 5)
#define ST_CS_COND_EMPTY    (1 << 6)
#define ST_CS_ESCAPE        (1 << 7)
#pragma GCC diagnostic pop

#endif /* __DEFS_H */

