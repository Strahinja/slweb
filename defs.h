/*
 *    slweb - Simple static webpage generator
 *    Copyright (C) 2020  Страхиња Радић
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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistr.h>
#include <unistdio.h>
#include <uniwidth.h>

#define PROGRAMNAME "slweb"
#define VERSION "0.3.0-beta"

#define BUFSIZE 1024

typedef enum
{
    FALSE = 0,
    TRUE = 1
} BOOL;

typedef unsigned char UBYTE;
typedef unsigned short USHORT;

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
} KeyValue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
const UBYTE MAX_HEADING_LEVEL = 4;

const USHORT ST_NONE       = 0;
const USHORT ST_YAML       = 1;
const USHORT ST_YAML_VAL   = 1 << 1;
const USHORT ST_PARA_OPEN  = 1 << 2;
const USHORT ST_TAG        = 1 << 3;
const USHORT ST_HEADING    = 1 << 4;
const USHORT ST_BOLD       = 1 << 5;
const USHORT ST_ITALIC     = 1 << 6;
const USHORT ST_PRE        = 1 << 7;
const USHORT ST_CODE       = 1 << 8;
const USHORT ST_BLOCKQUOTE = 1 << 9;
#pragma GCC diagnostic pop

#endif /* __DEFS_H */

