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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unistr.h>
#include <unistdio.h>
#include <uniwidth.h>

#define PROGRAMNAME   "slweb"
#define VERSION       "0.3.4"
#define COPYRIGHTYEAR "2020, 2021"

#define BUFSIZE       1024
#define DATEBUFSIZE   12

const char* timestamp_format     = "d.m.y";
const char* timestamp_output_ext = ".html";

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
    BOOL seen;    /* for use with macros */
} KeyValue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
const UBYTE MAX_HEADING_LEVEL = 4;
const UBYTE MAX_CSV_REGISTERS = 9;

const ULONG ST_NONE             = 0;
const ULONG ST_YAML             = 1;
const ULONG ST_YAML_VAL         = 1 << 1;
const ULONG ST_PARA_OPEN        = 1 << 2;
const ULONG ST_TAG              = 1 << 3;
const ULONG ST_HEADING          = 1 << 4;
const ULONG ST_BOLD             = 1 << 5;
const ULONG ST_ITALIC           = 1 << 6;
const ULONG ST_PRE              = 1 << 7;
const ULONG ST_CODE             = 1 << 8;
const ULONG ST_BLOCKQUOTE       = 1 << 9;
const ULONG ST_LINK             = 1 << 10;
const ULONG ST_LINK_SECOND_ARG  = 1 << 11;
const ULONG ST_LINK_SPAN        = 1 << 12;
const ULONG ST_LINK_MACRO       = 1 << 13;
const ULONG ST_IMAGE            = 1 << 14;
const ULONG ST_IMAGE_SECOND_ARG = 1 << 15;
const ULONG ST_MACRO_BODY       = 1 << 16;
const ULONG ST_CSV_BODY         = 1 << 17;

const UBYTE ST_CS_NONE         = 0;
const UBYTE ST_CS_HEADER       = 1;
const UBYTE ST_CS_REGISTER     = 1 << 2;
const UBYTE ST_CS_QUOTE        = 1 << 3;
#pragma GCC diagnostic pop

#endif /* __DEFS_H */

