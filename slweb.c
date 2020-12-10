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

#include "defs.h"
#include <stdlib.h>

int
version()
{
    printf("%s v%s\n", PROGRAMNAME, VERSION);
    return 0;
}

int
usage()
{
    printf("Usage: %s [-b|--body-only] [-d|--basedir <dir>] [-h|--help]"
        " [-v|--version] [filename]\n", PROGRAMNAME);
    return 0;
}

int
error(int code, uint8_t* fmt, ...)
{
    uint8_t    buf[BUFSIZE];
    va_list args;
    va_start(args, fmt);
    u8_vsnprintf(buf, sizeof(buf), (const char*)fmt, args);
    va_end(args);
    fprintf(stderr, "%s: %s", PROGRAMNAME, buf);
    return code;
}

char*
substr(char* src, int start, int finish)
{
    int len = strlen(src);
    if (finish > len)
        finish = len;
    int substr_len = finish-start;
    if (substr_len < 0)
        substr_len = 0;
    char* result = (char*) calloc(substr_len+1, sizeof(char));
    char* presult = result;

    for (int i = start; i < finish && *(src+i) != '\0'; i++)
    {
        *presult++ = *(src+i);
    }
    *presult = '\0';

    return result;
}

int
set_basedir(char* arg, char** basedir)
{
    if (*basedir)
        free(*basedir);

    if (strlen(arg) < 1)
    {
        return error(1, (uint8_t*)"--basedir: Argument required\n");
    }

    *basedir = (char*) calloc(strlen(arg)+1, sizeof(char));

    if (!*basedir) 
        return 1;

    strcpy(*basedir, arg);

    return 0;
}

int
main(int argc, char** argv)
{
    char* arg;
    Command cmd = CMD_NONE;
    char* filename = NULL;
    BOOL body_only = FALSE;
    char* basedir = NULL;

    basedir = (char*) calloc(2, sizeof(char));
    basedir[0] = '.';

    while ((arg = *++argv))
    {
        if (*arg == '-')
        {
            arg++;
            char c = *arg++;
            int result;

            if (c == '-')
            {
                if (!strcmp(arg, "version"))
                {
                    cmd = CMD_VERSION;
                }
                else if (!strcmp(arg, "body-only"))
                {
                    arg += strlen("body-only");
                    body_only = TRUE;
                }
                else if (!strcmp(substr(arg, 0, strlen("basedir")), "basedir"))
                {
                    arg += strlen("basedir");
                    result = set_basedir(arg, &basedir);
                    if (result)
                        return result;
                }
                else if (!strcmp(arg, "help"))
                {
                    return usage();
                }
                else
                {
                    error(1, (uint8_t*)"Invalid argument: --%s\n", arg);
                    return usage();
                }
            }
            else
            {
                switch (c)
                {
                case 'b':
                    body_only = TRUE;
                    break;
                case 'd':
                    cmd = CMD_BASEDIR;
                    break;
                case 'h':
                    return usage();
                    break;
                case 'v':
                    cmd = CMD_VERSION;
                    break;
                default:
                    error(1, (uint8_t*)"Invalid argument: -%c\n", c);
                    return usage();
                }
            }
        }
        else
        {
            int result;

            if (cmd == CMD_BASEDIR)
            {
                result = set_basedir(arg, &basedir);
                if (result)
                    return result;
            }
            else
            {
                filename = arg;
            }
            cmd = CMD_NONE;
        }
    }

    if (cmd == CMD_BASEDIR)
        return error(1, (uint8_t*)"-d: Argument required\n");

    if (cmd == CMD_VERSION)
        return version();

    /*
     *printf("debug: basedir=%s, filename=%s, body_only=%s\n",
     *        basedir, filename ? filename : "(NULL)", body_only ? "TRUE" : "FALSE");
     */

    FILE* input = NULL;
    if (filename)
    {
        input = fopen(filename, "r");
        if (!input)
        {
            return error(ENOENT, (uint8_t*)"File not found: %s\n", filename);
        }
    }
    else
        input = stdin;

    uint8_t* line = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
    size_t line_len = 0;

    do
    {
        fgets((char*)line, sizeof(uint8_t) * BUFSIZE, input);
        line_len = u8_strlen(line);

        if (!feof(input))
        {
            uint8_t* eol = u8_strchr(line, (ucs4_t)'\n');
            if (eol)
                *eol = (uint8_t)'\0';

            printf("%s\n", line);
        }
    }
    while (!feof(input));

    return 0;
}

