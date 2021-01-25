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

#include "defs.h"
#include <stdint.h>

static size_t lineno                  = 0;
static size_t colno                   = 1;
static char* input_filename           = NULL;
static char* input_dirname            = NULL;
static char* basedir                  = NULL;
static size_t basedir_size            = 0;
static char* incdir                   = NULL;
static KeyValue* vars                 = NULL;
static KeyValue* pvars                = NULL;
static size_t vars_count              = 0;
static KeyValue* macros               = NULL;
static KeyValue* pmacros              = NULL;
static size_t macros_count            = 0;
static KeyValue* links                = NULL;
static KeyValue* plinks               = NULL;
static size_t links_count             = 0;
static KeyValue* footnotes            = NULL;
static KeyValue* pfootnotes           = NULL;
static size_t footnote_count          = 0;
static size_t current_footnote        = 0;
static u_int8_t** inline_footnotes    = NULL;
static size_t inline_footnote_count   = 0;
static size_t current_inline_footnote = 0;
static uint8_t* csv_template          = NULL;
static size_t csv_template_size       = 0;
static char* csv_filename             = NULL;
static ULONG state                    = ST_NONE;

#define CHECKEXITNOMEM(ptr) { if (!ptr) exit(error(ENOMEM, \
                (uint8_t*)"Memory allocation failed (out of memory?)\n")); }

#define CALLOC(ptr, ptrtype, nmemb) { ptr = calloc(nmemb, sizeof(ptrtype)); \
    CHECKEXITNOMEM(ptr) }

#define REALLOC(ptr, newsize) { ptr = realloc(ptr, newsize); \
    CHECKEXITNOMEM(ptr) }

#define REALLOCARRAY(ptr, membtype, newcount) { ptr = realloc(ptr, \
        newcount * sizeof(membtype)); CHECKEXITNOMEM(ptr) }

#define CHECKCOPY(token, ptoken, token_size, pline) { \
    if (ptoken - token + 1 > token_size) \
    { \
        token_size += BUFSIZE; \
        REALLOC(token, token_size) \
    } \
    *ptoken++ = *pline++; }

#define RESET_TOKEN(token, ptoken, token_size) { \
    token_size = BUFSIZE; \
    REALLOC(token, token_size) \
    *token = 0; ptoken = token; }

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
    uint8_t buf[BUFSIZE];
    va_list args;
    va_start(args, fmt);
    u8_vsnprintf(buf, sizeof(buf), (const char*)fmt, args);
    va_end(args);
    fprintf(stderr, "%s:%s:%lu:%lu: %s\n", PROGRAMNAME, 
            input_filename ? input_filename : "(stdin)", 
            lineno, colno, buf);
    return code;
}

int
warning(int code, uint8_t* fmt, ...)
{
    uint8_t buf[BUFSIZE];
    va_list args;
    va_start(args, fmt);
    u8_vsnprintf(buf, sizeof(buf), (const char*)fmt, args);
    va_end(args);
    fprintf(stderr, "Warning: %s\n", buf);
    return code;
}

int
slweb_parse(uint8_t* buffer, FILE* output, BOOL body_only, 
        BOOL read_yaml_macros_and_links);

char*
substr(const char* src, int start, int finish)
{
    int len = strlen(src);
    if (finish > len)
        finish = len;
    int substr_len = finish-start;
    if (substr_len < 0)
        substr_len = 0;
    char* result = NULL;
    CALLOC(result, char, substr_len+1)
    char* presult = result;

    for (int i = start; i < finish && *(src+i) != 0; i++)
        *presult++ = *(src+i);
    *presult = 0;

    return result;
}

BOOL
startswith(const char* s, const char* what)
{
    if (!s || !what)
        return 0;

    char* subs = substr(s, 0, strlen(what));
    BOOL result = !strcmp(subs, what);

    free(subs);

    return result;
}

uint8_t*
get_value(KeyValue* list, size_t list_count, uint8_t* key, BOOL* seen)
{
    KeyValue* plist = list;
    while (plist < list + list_count)
    {
        if (!u8_strcmp(plist->key, key))
        {
            if (seen)
            {
                *seen = plist->seen;
                plist->seen = TRUE;
            }
            return plist->value;
        }
        plist++;
    }
    return NULL;
}

int
set_basedir(char* arg, char** basedir, size_t* basedir_size)
{
    size_t basedir_len = 0;
    size_t arg_len = strlen(arg);

    if (arg_len < 1)
        exit(error(1, (uint8_t*)"--basedir: Argument required"));

    if (arg_len > *basedir_size)
    {
        *basedir_size = arg_len+1;
        REALLOC(*basedir, *basedir_size)
    }
    strncpy(*basedir, arg, *basedir_size-1);
    *(*basedir + arg_len) = 0;
    basedir_len = strlen(*basedir);
    if (*(*basedir + basedir_len - 1) == '/')
        *(*basedir + basedir_len - 1) = 0;

    return 0;
}

char*
strip_ext(const char* fn)
{
    char* newname = NULL;
    char* pnewname = NULL;
    const char* pfn = NULL;
    char* dot = NULL;
    dot = strrchr(fn, '.');
    if (!dot)
        return NULL;
    CALLOC(newname, char, strlen(fn)+1)
    pnewname = newname;
    pfn = fn;
    while (pfn != dot && *pfn)
        *pnewname++ = *pfn++;
    return newname;
}

int
print_output(FILE* output, char* fmt, ...)
{
    uint8_t buf[BUFSIZE];
    va_list args;

    if (!output || !fmt)
        exit(error(EINVAL, (uint8_t*)"print_output: Invalid argument"));

    va_start(args, fmt);
    if (state & ST_CSV_BODY)
    {
        size_t buf_len = 0;
        vsnprintf((char*)buf, BUFSIZE, fmt, args);
        buf_len = u8_strlen(buf);
        if (!csv_template)
        {
            csv_template_size = BUFSIZE;
            CALLOC(csv_template, uint8_t, csv_template_size)
            u8_strncpy(csv_template, buf, BUFSIZE-1);
            *(csv_template + buf_len) = 0;
        }
        else
        {
            if (u8_strlen(csv_template) + buf_len > csv_template_size)
            {
                csv_template_size += BUFSIZE;
                REALLOC(csv_template, csv_template_size)
            }
            u8_strncat(csv_template, buf, csv_template_size
                    - u8_strlen(csv_template) - 1);
        }
    }
    else
        vfprintf(output, fmt, args);
    va_end(args);
    return 0;
}

int
print_command(uint8_t* command, FILE* output, BOOL strip_newlines)
{
    if (!command)
        exit(error(EINVAL, (uint8_t*)"print_command: Invalid argument"));

    FILE* cmd_output = popen((char*)command, "r");

    if (!cmd_output)
    {
        perror("popen");
        return 1;
    }

    uint8_t* cmd_output_line = NULL;
    CALLOC(cmd_output_line, uint8_t, BUFSIZE)
    while (!feof(cmd_output))
    {
        if (!fgets((char*)cmd_output_line, BUFSIZE, cmd_output))
            continue;

        char* eol = strchr((char*)cmd_output_line, '\n');
        if (eol)
            *eol = 0;

        print_output(output, "%s%s", cmd_output_line,
                strip_newlines ? "" : "\n");
    }
    free(cmd_output_line);
    return pclose(cmd_output);
}

int
read_file_into_buffer(uint8_t** buffer, size_t* buffer_size, char* input_filename, 
        char** input_dirname, FILE** input)
{
    struct stat fs;
    char* slash = NULL;

    *input = fopen(input_filename, "r");
    if (!*input)
        return error(ENOENT, (uint8_t*)"No such file: %s", input_filename);

    fstat(fileno(*input), &fs);
    if (*buffer)
        free(*buffer);
    *buffer_size = fs.st_size+1;
    CALLOC(*buffer, uint8_t, *buffer_size)
    fread((void*)*buffer, 1, *buffer_size, *input);

    if (*input_dirname)
        free(*input_dirname);

    slash = strrchr(input_filename, '/');
    if (slash)
    {
        CALLOC(*input_dirname, char, strlen(input_filename)+1)
        char* pinput_dirname = *input_dirname;
        char* pinput_filename = input_filename;
        while (pinput_filename && *pinput_filename
                && pinput_filename != slash)
            *pinput_dirname++ = *pinput_filename++;
    }
    else
    {
        CALLOC(*input_dirname, char, 2)
        **input_dirname = '.';
    }

    return 0;
}

int
process_heading_start(FILE* output, UBYTE heading_level)
{
    print_output(output, "<h%d>", heading_level);
    return 0;
}

int
process_heading(uint8_t* token, FILE* output, UBYTE heading_level)
{
    if (!token || u8_strlen(token) < 1)
        warning(1, (uint8_t*)"Empty heading");

    print_output(output, "%s</h%d>", 
            token ? (char*)token : "", heading_level);

    return 0;
}

int
process_git_log(FILE* output)
{
    if (!input_filename)
        return warning(1, (uint8_t*)"Cannot use 'git-log' in stdin");

    char* basename = NULL;
    size_t basename_size = strlen(input_filename)+1;
    CALLOC(basename, char, basename_size)
    char* slash = strrchr(input_filename, '/');
    if (slash)
        strncpy(basename, slash+1, basename_size-1);
    else
        strncpy(basename, input_filename, basename_size-1);

    print_output(output, "<div id=\"git-log\">\nPrevious commit:\n");
    uint8_t* command = NULL;
    CALLOC(command, uint8_t, BUFSIZE)
    u8_snprintf(command, BUFSIZE-1,
            "git log -1 --pretty=format:\"%s %%h %%ci (%%cn) %%d\""
            " || echo \"(Not in a Git repository)\"",
        basename);
    if (print_command(command, output, FALSE))
    {
        print_output(output, "</div><!--git-log-->\n");
        return error(1, (uint8_t*)"git-log: Cannot run git");
    }
    print_output(output, "</div><!--git-log-->\n");

    free(command);
    free(basename);

    return 0;
}

int
print_csv_row(FILE* output, uint8_t** csv_header, uint8_t** csv_register)
{
    uint8_t* pcsv_template = csv_template;
    UBYTE csv_state        = ST_CS_NONE;
    UBYTE num              = 0;

    while (*pcsv_template)
    {
        switch (*pcsv_template)
        {
        case '$':
            if (csv_state & ST_CS_REGISTER)
            {
                fprintf(output, "$");
                csv_state &= ST_CS_REGISTER;
            }
            else
                csv_state |= ST_CS_REGISTER;
            pcsv_template++;
            break;
        case '#':
            if (csv_state & ST_CS_REGISTER)
            {
                if (csv_state & ST_CS_HEADER)
                {
                    error(1, (uint8_t*)"csv: Invalid header register mark");
                    csv_state &= ~(ST_CS_REGISTER | ST_CS_HEADER);
                }
                else
                    csv_state |= ST_CS_HEADER;
            }
            else
                fprintf(output, "#");
            pcsv_template++;
            break;
        case '1': case '2': case '3': case '4': case '5': 
        case '6': case '7': case '8': case '9':
            if (csv_state & ST_CS_HEADER)
            {
                num = *pcsv_template - '0';
                fprintf(output, "%s", csv_header[num-1]);
                csv_state &= ~(ST_CS_REGISTER | ST_CS_HEADER);
            }
            else if (csv_state & ST_CS_REGISTER)
            {
                num = *pcsv_template - '0';
                fprintf(output, "%s", csv_register[num-1]);
                csv_state &= ~ST_CS_REGISTER;
            }
            else
                fprintf(output, "%c", *pcsv_template);
            pcsv_template++;
            break;
        default:
            if (csv_state & ST_CS_REGISTER)
            {
                error(1, (uint8_t*)"csv: Invalid register mark");
                csv_state &= ~ST_CS_REGISTER;
            }
            else
                fprintf(output, "%c", *pcsv_template);
            pcsv_template++;
        }
    }
    return 0;
}

int
process_csv(uint8_t* arg_token, FILE* output, BOOL read_yaml_macros_and_links, 
        BOOL end_tag)
{
    if (read_yaml_macros_and_links)
        return 0;

    if (end_tag)
    {
        state &= ~ST_CSV_BODY;

        FILE* csv                                 = fopen(csv_filename, "rt");
        size_t csv_lineno                         = 0;
        uint8_t* bufline                          = NULL;
        uint8_t* pbufline                         = NULL;
        uint8_t* token                            = NULL;
        uint8_t* ptoken                           = NULL;
        size_t token_size                         = 0;
        UBYTE csv_state                           = ST_CS_NONE;
        uint8_t* csv_header[MAX_CSV_REGISTERS];
        UBYTE current_header                      = 0;
        uint8_t* csv_register[MAX_CSV_REGISTERS];
        UBYTE current_register                    = 0;
        uint8_t* csv_delimiter                    = get_value(vars, vars_count,
                (uint8_t*)"csv-delimiter", NULL);

        if (!csv)
            exit(error(ENOENT, (uint8_t*)"csv: No such file: %s", csv_filename));

        CALLOC(bufline, uint8_t, BUFSIZE)
        token_size = BUFSIZE;
        CALLOC(token, uint8_t, token_size)
        for (UBYTE i = 0; i < MAX_CSV_REGISTERS; i++)
            CALLOC(csv_header[i], uint8_t, BUFSIZE)
        for (UBYTE i = 0; i < MAX_CSV_REGISTERS; i++)
            CALLOC(csv_register[i], uint8_t, BUFSIZE)

        while (!feof(csv))
        {
            uint8_t* eol = NULL;
            if (!fgets((char*)bufline, BUFSIZE, csv))
                break;
            eol = u8_strchr(bufline, (ucs4_t)'\n');
            if (eol)
                *eol = 0;

            pbufline = bufline;
            RESET_TOKEN(token, ptoken, token_size)
            current_register = 0;
            while (*pbufline)
            {
                switch (*pbufline)
                {
                case '"':
                    if (csv_state & ST_CS_QUOTE)
                        csv_state &= ~ST_CS_QUOTE;
                    else
                        csv_state |= ST_CS_QUOTE;
                    pbufline++;
                    break;
                case ';':
                case ',':
                    if (csv_state & ST_CS_QUOTE)
                        CHECKCOPY(token, ptoken, token_size, pbufline)
                    else
                    {
                        *ptoken = 0;
                        if (csv_lineno > 0)
                        {
                            if (current_register < MAX_CSV_REGISTERS)
                                u8_strncpy(csv_register[current_register++], token,
                                        BUFSIZE-1);
                        }
                        else
                        {
                            if (current_header < MAX_CSV_REGISTERS)
                                u8_strncpy(csv_header[current_header++], token, 
                                        BUFSIZE-1);
                        }
                        RESET_TOKEN(token, ptoken, token_size)
                        pbufline++;
                    }
                    break;
                default:
                    if (csv_state & ST_CS_QUOTE)
                        CHECKCOPY(token, ptoken, token_size, pbufline)
                    else
                    {
                        if (csv_delimiter && *pbufline == *csv_delimiter)
                        {
                            *ptoken = 0;
                            if (csv_lineno > 0)
                            {
                                if (current_register < MAX_CSV_REGISTERS)
                                    u8_strncpy(csv_register[current_register++], token,
                                            BUFSIZE-1);
                            }
                            else
                            {
                                if (current_header < MAX_CSV_REGISTERS)
                                    u8_strncpy(csv_header[current_header++], token, 
                                            BUFSIZE-1);
                            }
                            RESET_TOKEN(token, ptoken, token_size)
                            pbufline++;
                        }
                        else
                            CHECKCOPY(token, ptoken, token_size, pbufline)
                    }
                }
            }
            *ptoken = 0;
            if (csv_lineno > 0)
            {
                if (current_register < MAX_CSV_REGISTERS)
                    u8_strncpy(csv_register[current_register++], token, 
                            BUFSIZE-1);
            }
            else
            {
                if (current_header < MAX_CSV_REGISTERS)
                    u8_strncpy(csv_header[current_header++], token, BUFSIZE-1);
            }
            RESET_TOKEN(token, ptoken, token_size)

            if (csv_lineno > 0 && pbufline != bufline)
                print_csv_row(output, csv_header, csv_register);

            for (UBYTE i = 0; i < MAX_CSV_REGISTERS; i++)
                *csv_register[i] = 0;
            csv_lineno++;
        }
        fclose(csv);
        for (UBYTE i = MAX_CSV_REGISTERS; i > 0; i--)
            free(csv_register[i-1]);
        for (UBYTE i = MAX_CSV_REGISTERS; i > 0; i--)
            free(csv_header[i-1]);
        free(token);
        free(bufline);

        free(csv_template);
        csv_template = NULL;
        csv_template_size = 0;
    }
    else
    {
        if (state & ST_CSV_BODY)
            exit(error(1, (uint8_t*)"Can't nest csv directives"));

        state |= ST_CSV_BODY;
        uint8_t* saveptr = NULL;
        uint8_t* args = u8_strtok(arg_token, (uint8_t*)" ", &saveptr);
        args = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
        if (!args)
            exit(error(EINVAL, (uint8_t*)"csv: Arguments required"));
        size_t args_len = u8_strlen(args);
        if (*args != '"' || *(args + args_len - 1) != '"')
            exit(error(EINVAL, (uint8_t*)"csv: First argument must be a string"));
        if (!csv_filename)
            CALLOC(csv_filename, uint8_t, BUFSIZE)
        uint8_t* args_base = u8_strdup(args+1);
        *(args_base + u8_strlen(args_base) - 1) = 0;
        snprintf(csv_filename, BUFSIZE, "%s/%s.csv", input_dirname, (char*)args_base);
        free(args_base);
    }

    return 0;
}

int
process_include(uint8_t* token, FILE* output, BOOL read_yaml_macros_and_links)
{
    if (read_yaml_macros_and_links)
        return 0;

    if (!input_filename)
        return warning(1, (uint8_t*)"Cannot use 'include' in stdin");

    uint8_t* ptoken = u8_strchr(token, (ucs4_t)' ');
    char* include_filename = NULL;
    char* pinclude_filename = NULL;
    
    if (!ptoken)
        exit(error(1, (uint8_t*)"Directive 'include' requires"
                " an argument"));

    fflush(output);
    pid_t pid = fork();
    int pstatus = 0;

    if (pid > 0)
        wait(&pstatus);
    else if (pid == 0)
    {
        CALLOC(include_filename, char, BUFSIZE)
        pinclude_filename = include_filename;
        ptoken++;
        while (ptoken && *ptoken)
            if (*ptoken != '"')
                *pinclude_filename++ = *ptoken++;
            else 
                ptoken++;

        if (!strcmp(basedir, "."))
            set_basedir(input_dirname, &basedir, &basedir_size);

        CALLOC(input_filename, char, BUFSIZE)
        snprintf(input_filename, BUFSIZE, "%s/%s.slw", basedir, include_filename);
        free(include_filename);

        FILE* input        = NULL;
        FILE* output       = stdout;
        uint8_t* buffer    = NULL;
        size_t buffer_size = 0;
        int result         = 0;
        ULONG saved_state  = ST_NONE;

        read_file_into_buffer(&buffer, &buffer_size, input_filename, 
                &input_dirname, &input);

        free(links);
        CALLOC(links, KeyValue, 1)
        links->key = NULL;
        links->value = NULL;
        links->value_size = 0;
        links_count = 0;

        free(footnotes);
        CALLOC(footnotes, KeyValue, 1)
        footnotes->key = NULL;
        footnotes->value = NULL;
        footnotes->value_size = 0;
        footnote_count = 0;
        current_footnote = 0;

        free(inline_footnotes);
        CALLOC(inline_footnotes, uint8_t*, 1)
        *inline_footnotes = NULL;
        inline_footnote_count = 0;
        current_inline_footnote = 0;

        saved_state = state;
        state = ST_NONE;

        /* First pass: read YAML, macros and links */
        result = slweb_parse(buffer, output, TRUE, TRUE);

        if (result)
        {
            free(input_filename);
            free(buffer);
            return result;
        }

        state = ST_NONE;
        current_footnote = 0;
        current_inline_footnote = 0;

        /* Second pass: parse and output */
        result = slweb_parse(buffer, output, TRUE, FALSE);

        state = saved_state;

        free(input_filename);
        free(buffer);
        exit(result);
    }
    else
        exit(error(1, (uint8_t*)"Fork failed"));


    return 0;
}

int
process_list_start(FILE* output)
{
    fprintf(output, "<ul>");
    return 0;
}

int
process_list_item_start(FILE* output)
{
    fprintf(output, "\n<li><p>");
    state |= ST_PARA_OPEN;
    return 0;
}

int
process_list_item_end(FILE* output)
{
    if (state & ST_PARA_OPEN)
    {
        state &= ~ST_PARA_OPEN;
        fprintf(output, "</p>");
    }
    fprintf(output, "</li>");
    return 0;
}

int
process_list_end(FILE* output)
{
    fprintf(output, "</ul>\n");
    return 0;
}

int
filter_subdirs(const struct dirent* node)
{
    if (!node || !strcmp(node->d_name, ".") || !strcmp(node->d_name, ".."))
        return 0;

    struct stat st;
    char* nodename = NULL;
    
    CALLOC(nodename, char, BUFSIZE)
    snprintf(nodename, BUFSIZE, "%s/%s", incdir, node->d_name);
    
    if (lstat(nodename, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        free(nodename);
        return 0;
    }

    free(nodename);

    return 1;
}

int
filter_slw(const struct dirent* node)
{
    if (!node || !strcmp(node->d_name, ".") || !strcmp(node->d_name, ".."))
        return 0;

    size_t node_len = strlen(node->d_name);
    size_t slw_len = strlen(".slw");

    if (slw_len >= node_len)
        return 0;
    return !strcmp(node->d_name + strlen(node->d_name) - slw_len, ".slw");
}

int 
reverse_alphacompare(const struct dirent** a, const struct dirent** b)
{
    if (!a || !b)
        return 0;

    return -1 * strcmp((*a)->d_name, (*b)->d_name); 
}

int
process_incdir_subdir(const char* subdirname, FILE* output, BOOL details_open,
        uint8_t* macro_body)
{
    print_output(output, "<li>\n<details%s>\n<summary>", 
            details_open ? " open" : "");
    if (macro_body)
        fprintf(stdout, "%s", macro_body);
    print_output(output, "%s</summary>\n<div>\n", subdirname);

    struct dirent** namelist;
    struct dirent** pnamelist;
    long names_total;
    long names_output;
    char* abs_subdirname = NULL;

    CALLOC(abs_subdirname, char, BUFSIZE)
    snprintf(abs_subdirname, BUFSIZE, "%s/%s", incdir, subdirname);

    if ((names_total = scandir(abs_subdirname, &namelist, &filter_slw, 
                &reverse_alphacompare)) < 0)
    {
        perror("scandir");
        exit(error(errno, (uint8_t*)"incdir_subdir: scandir error"));
    }

    pnamelist = namelist;
    names_output = 0;
    while (names_output < names_total && pnamelist && *pnamelist)
    {
        fflush(output);
        pid_t pid = fork();
        int pstatus = 0;

        if (pid > 0)
            wait(&pstatus);
        else if (pid == 0)
        {
            set_basedir(abs_subdirname, &basedir, &basedir_size);
            snprintf(input_filename, BUFSIZE, "%s/%s", abs_subdirname, 
                    (*pnamelist)->d_name);

            FILE* input        = NULL;
            FILE* output       = stdout;
            uint8_t* buffer    = NULL;
            size_t buffer_size = 0;
            int result         = 0;
            ULONG saved_state  = ST_NONE;

            read_file_into_buffer(&buffer, &buffer_size, input_filename, 
                    &input_dirname, &input);

            free(links);
            CALLOC(links, KeyValue, 1)
            links->key = NULL;
            links->value = NULL;
            links->value_size = 0;
            links_count = 0;

            free(footnotes);
            CALLOC(footnotes, KeyValue, 1)
            footnotes->key = NULL;
            footnotes->value = NULL;
            footnotes->value_size = 0;
            footnote_count = 0;
            current_footnote = 0;

            free(inline_footnotes);
            CALLOC(inline_footnotes, uint8_t*, 1)
            *inline_footnotes = NULL;
            inline_footnote_count = 0;
            current_inline_footnote = 0;

            saved_state = state;
            state = ST_NONE;

            saved_state = state;
            state = ST_NONE;

            /* First pass: read YAML, macros and links */
            result = slweb_parse(buffer, output, TRUE, TRUE);

            if (result)
            {
                free(buffer);
                return result;
            }

            state = ST_NONE;
            current_footnote = 0;
            current_inline_footnote = 0;

            /* Second pass: parse and output */
            result = slweb_parse(buffer, output, TRUE, FALSE);

            state = saved_state;

            fflush(output);
            free(buffer);
            exit(result);
        }
        else
            exit(error(1, (uint8_t*)"Fork failed"));

        pnamelist++;
        names_output++;
    }

    free(abs_subdirname);

    print_output(output, "</div>\n</details>\n</li>\n");
    return 0;
}

int
process_incdir(uint8_t* token, FILE* output, BOOL read_yaml_macros_and_links)
{
    if (read_yaml_macros_and_links)
        return 0;

    uint8_t* saveptr = NULL;
    /* skipping the first token (incdir) */
    uint8_t* arg = u8_strtok(token, (uint8_t*)" ", &saveptr);
    size_t arg_len = 0;
    long num = 5;
    uint8_t* macro_body = NULL;
    struct dirent** namelist;
    struct dirent** pnamelist;
    long names_output;
    BOOL details_open = TRUE;


    arg = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
    if (!arg)
        exit(error(1, (uint8_t*)"incdir: Arguments required"));

    arg_len = u8_strlen(arg);

    if (*arg != '"' || *(arg + arg_len - 1) != '"')
        exit(error(1, (uint8_t*)"incdir: First argument not string"));

    incdir = strdup((char*)(arg+1));
    *(incdir + strlen(incdir) - 1) = 0;

    arg = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
    if (!arg)
        exit(error(1, (uint8_t*)"incdir: Second argument required"));

    if (*arg == '=')
        macro_body = get_value(macros, macros_count, arg+1, NULL);
    else
    {
        uint8_t* parg = arg;
        while (parg && *parg)
        {
            if (*parg < '0' || *parg > '9')
                exit(error(1, (uint8_t*)"incdir: Non-numeric argument"));
            parg++;
        }
        num = strtol((char*)arg, NULL, 10);
        if (errno)
            exit(error(errno, (uint8_t*)"incdir: Invalid parameter 'num'"));
        arg = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
        if (arg)
        {
            if (*arg != '=')
                exit(error(1, (uint8_t*)"incdir: Third argument not macro"));
            macro_body = get_value(macros, macros_count, arg+1, NULL);
        }
    }

    print_output(output, "<ul class=\"incdir\">\n");

    if (scandir(incdir, &namelist, &filter_subdirs,
            &reverse_alphacompare) < 0)
    {
        perror("scandir");
        exit(error(errno, (uint8_t*)"incdir: scandir '%s' error", incdir));
    }

    pnamelist = namelist;
    names_output = 0;
    while (names_output < num && pnamelist && *pnamelist)
    {
        process_incdir_subdir((*pnamelist)->d_name, output, details_open,
                macro_body);
        details_open = FALSE;
        pnamelist++;
        names_output++;
    }
    free(namelist);
    free(incdir);

    print_output(output, "</ul>\n");

    return 0;
}

int
process_timestamp(FILE* output, const char* link, uint8_t* permalink_macro,
        uint8_t* date)
{
    uint8_t* day = NULL;
    uint8_t* month = NULL;
    uint8_t* year = NULL;
    uint8_t* formatted_date = NULL;
    uint8_t* ptr = NULL;
    const char* ptimestamp_format = NULL;
    char* in_filename = NULL;
    char* in_line = NULL;

    CALLOC(formatted_date, uint8_t, DATEBUFSIZE)
    ptr = NULL;
    year = u8_strtok(date, (uint8_t*)"-", &ptr);
    if (year)
    {
        month = u8_strtok(NULL, (uint8_t*)"-", &ptr);
        if (month)
        {
            day = u8_strtok(NULL, (uint8_t*)"T", &ptr);
            if (day)
            {
                ptimestamp_format = timestamp_format;
                while (*ptimestamp_format)
                {
                    if (*ptimestamp_format == 'd' 
                            || *ptimestamp_format == 'D')
                        u8_strncat(formatted_date, day, 
                                DATEBUFSIZE-u8_strlen(formatted_date-1));
                    else if (*ptimestamp_format == 'm' 
                            || *ptimestamp_format == 'M')
                        u8_strncat(formatted_date, month, 
                                DATEBUFSIZE-u8_strlen(formatted_date-1));
                    else if (*ptimestamp_format == 'y' 
                            || *ptimestamp_format == 'Y')
                        u8_strncat(formatted_date, year, 
                                DATEBUFSIZE-u8_strlen(formatted_date-1));
                    else
                        *(formatted_date + u8_strlen(formatted_date)) 
                            = *ptimestamp_format;

                    ptimestamp_format++;
                }
                print_output(output, "<a href=\"%s\""
                        " class=\"timestamp\">%s%s</a>\n",
                        link,
                        permalink_macro ? (char*)permalink_macro : "",
                        formatted_date);
            }
        }
    }

    free(formatted_date);
    free(in_line);
    free(in_filename);

    return 0;
}

int
process_macro(uint8_t* token, FILE* output, BOOL read_yaml_macros_and_links, 
        BOOL end_tag)
{
    if (!end_tag)
    {
        if (state & ST_MACRO_BODY)
            exit(error(1, (uint8_t*)"Macro undefined or nested"));

        BOOL seen = FALSE;
        uint8_t* macro_body = get_value(macros, macros_count, token+1,
                read_yaml_macros_and_links ? NULL : &seen);

        if (macro_body)
        {
            if (!read_yaml_macros_and_links)
            {
                if (seen)
                    print_output(output, "%s", macro_body);
                else
                    state |= ST_MACRO_BODY;
            }
        }
        else
        {
            if (read_yaml_macros_and_links)
            {
                macros_count++;

                if (macros_count > 1)
                {
                    REALLOC(macros, macros_count * sizeof(KeyValue))
                    pmacros = macros + macros_count - 1;
                }
                CALLOC(pmacros->key, uint8_t, KEYSIZE)
                pmacros->seen = FALSE;
                u8_strncpy(pmacros->key, token+1, KEYSIZE-1);
                pmacros->value = NULL;
                pmacros->value_size = 0;
            }
            state |= ST_MACRO_BODY;
        }
    }
    else
        state &= ~ST_MACRO_BODY;

    return 0;
}

int
process_tag(uint8_t* token, FILE* output, BOOL read_yaml_macros_and_links, 
        BOOL* skip_eol, BOOL end_tag)
{
    if (!token || u8_strlen(token) < 1)
        return warning(1, (uint8_t*)"Empty tag name");

    if (!strcmp((char*)token, "git-log")
            && !read_yaml_macros_and_links)   /* {git-log} */
    {
        process_git_log(output);
    }
    else if (!strcmp((char*)token, "made-by")
            && !read_yaml_macros_and_links)   /* {made-by} */
    {
        print_output(output, "<div id=\"made-by\">\n"
                "Generated by <a href=\"https://github.com/Strahinja/slweb\">"
                "slweb</a>\n"
                "© %s Strahinya Radich.\n"
                "</div><!--made-by-->\n",
                COPYRIGHTYEAR);
    }
    else if (startswith((char*)token, "csv"))   /* {csv} */
    {
        process_csv(token, output, read_yaml_macros_and_links, end_tag);
    }
    else if (startswith((char*)token, "include"))  /* {include} */
    {
        process_include(token, output, read_yaml_macros_and_links);
        *skip_eol = TRUE;
    }
    else if (startswith((char*)token, "incdir"))   /* {incdir} */
    {
        process_incdir(token, output, read_yaml_macros_and_links);
        *skip_eol = TRUE;
    }
    else if (*token == '=')   /* {=macro} */
    {
        process_macro(token, output, read_yaml_macros_and_links, end_tag);
        *skip_eol = TRUE;
    }
    else if (!read_yaml_macros_and_links)   /* general tags */
    {
        print_output(output, "<");
        if (end_tag)
            print_output(output, "/");

        if (*token == '.' || *token == '#')
        {
            print_output(output, "div");
        }

        while (*token && *token != '#' && *token != '.')
            print_output(output, "%c", *token++);

        if (!end_tag)
        {
            if (*token == '#')
            {
                token++;
                print_output(output, " id=\"");
                while (*token && *token != '.')
                    print_output(output, "%c", *token++);
                print_output(output, "\"");
                if (*token == '.')
                {
                    token++;
                    print_output(output, " class=\"");
                    while (*token)
                        print_output(output, "%c", *token++);
                    print_output(output, "\"");
                }
            }
            else if (*token == '.')
            {
                token++;
                print_output(output, " class=\"");
                while (*token && *token != '#')
                    print_output(output, "%c", *token++);
                print_output(output, "\"");
                if (*token == '#')
                {
                    token++;
                    print_output(output, " id=\"");
                    while (*token)
                        print_output(output, "%c", *token++);
                    print_output(output, "\"");
                }
            }
        }
        print_output(output, ">");
    }

    return 0;
}

int
process_bold(FILE* output, BOOL end_tag)
{
    print_output(output, "<%sstrong>", end_tag ? "/" : "");
    return 0;
}

int
process_italic(FILE* output, BOOL end_tag)
{
    print_output(output, "<%sem>", end_tag ? "/" : "");
    return 0;
}

int
process_code(FILE* output, BOOL end_tag)
{
    print_output(output, "<%scode>", end_tag ? "/" : "");
    return 0;
}

int
process_blockquote(FILE* output, BOOL end_tag)
{
    print_output(output, "<%sblockquote>", end_tag ? "/" : "");
    return 0;
}

int
process_kbd(FILE* output, BOOL end_tag)
{
    print_output(output, "<%skbd>", end_tag ? "/" : "");
    return 0;
}

BOOL
url_is_local(char* url)
{
    return !(startswith(url, "http://")
        || startswith(url, "https://")
        || startswith(url, "ftp://")
        || startswith(url, "ftps://")
        || startswith(url, "mailto://"));
}

int
get_realpath(char** realpath, char* relativeto, char* path)
{
    uint8_t* command = NULL;
    FILE* cmd_output = NULL;

    CALLOC(command, uint8_t, BUFSIZE)
    strncpy(*realpath, ".", BUFSIZE-1);
    u8_snprintf(command, BUFSIZE-1, "realpath --relative-to=%s %s", 
            relativeto, path);

    cmd_output = popen((char*)command, "r");
    if (cmd_output)
    {
        uint8_t* cmd_output_line = NULL;
        CALLOC(cmd_output_line, uint8_t, BUFSIZE)
        while (!feof(cmd_output))
        {
            if (!fgets((char*)cmd_output_line, BUFSIZE, cmd_output))
                continue;

            char* eol = strchr((char*)cmd_output_line, '\n');
            if (eol)
                *eol = 0;

            strncpy(*realpath, (char*)cmd_output_line, BUFSIZE-1);
        }
        pclose(cmd_output);
        free(cmd_output_line);
    }
    else
        warning(1, (uint8_t*)"get_realpath: Cannot popen");

    free(command);

    return 0;
}

int
process_inline_link(uint8_t* link_text, uint8_t* link_macro_body, 
        uint8_t* link_url, FILE* output)
{
    print_output(output, "<a href=\"%s\">%s%s</a>", 
            link_url ? (char*)link_url : "", 
            link_macro_body ? (char*)link_macro_body : "",
            link_text);

    return 0;
}

int
process_link(uint8_t* link_text, uint8_t* link_macro_body, uint8_t* link_id, 
        FILE* output)
{
    uint8_t* url = get_value(links, links_count, link_id, NULL);
    return process_inline_link(link_text, link_macro_body,
            url, output);
}

int
process_inline_image(uint8_t* image_text, uint8_t* image_url, FILE* output,
        BOOL add_link)
{
    if (add_link)
        print_output(output, "<a href=\"%s\" title=\"%s\" class=\"image\""
                " target=\"_blank\">",
                image_url ? (char*)image_url : "", 
                image_text);

    print_output(output, "<img src=\"%s\" alt=\"%s\" />",
            image_url ? (char*)image_url : "", 
            image_text);

    if (add_link)
        print_output(output, "</a>");
    return 0;
}

int
process_image(uint8_t* image_text, uint8_t* image_id, FILE* output, BOOL add_link)
{
    uint8_t* url = get_value(links, links_count, image_id, NULL);
    return process_inline_image(image_text, url, output, add_link);
}

int
process_line_start(uint8_t* line, BOOL first_line_in_doc,
        BOOL previous_line_blank, BOOL processed_start_of_line,
        BOOL read_yaml_macros_and_links,  BOOL list_para,
        FILE* output, uint8_t** token, uint8_t** ptoken)
{
    if ((first_line_in_doc || previous_line_blank)
            && !processed_start_of_line
            && !(state & (ST_BLOCKQUOTE | ST_PRE)))
    {
        if (!list_para)
        {
            if (state & ST_LIST)
            {
                state &= ~ST_LIST;
                if (!read_yaml_macros_and_links)
                    process_list_end(output);
            }

            if (state & ST_FOOTNOTE_TEXT)
            {
                state &= ~ST_FOOTNOTE_TEXT;
                if (!read_yaml_macros_and_links)
                    print_output(output, "</p>\n");
            }
        }
        if (!read_yaml_macros_and_links)
            print_output(output, "<p>");
        state |= ST_PARA_OPEN;
    }
    return 0;
}

int
process_inline_footnote(uint8_t* token, BOOL read_yaml_macros_and_links, 
        FILE* output)
{
    current_inline_footnote++;

    if (read_yaml_macros_and_links)
    {
        size_t token_len = u8_strlen(token);

        inline_footnote_count++;
        if (inline_footnote_count == 1 && footnote_count > 0)
            warning(1, (u_int8_t*)"Both inline and regular footnotes present");
        else if (inline_footnote_count > 1)
            REALLOC(inline_footnotes, sizeof(uint8_t*) * inline_footnote_count)
        CALLOC(inline_footnotes[inline_footnote_count-1], uint8_t, token_len+1)

        u8_strncpy(inline_footnotes[inline_footnote_count-1], token, token_len);
        *(inline_footnotes[inline_footnote_count-1] + token_len) = 0;
    }
    else
        print_output(output, "<a href=\"#inline-footnote-%d\" id=\"inline-footnote-text-%d\">"
                "<sup>%d</sup></a>", 
                current_inline_footnote, current_inline_footnote, 
                current_inline_footnote);

    return 0;
}

int
process_footnote(uint8_t* token, BOOL footnote_definition, BOOL footnote_output,
        FILE* output)
{
    current_footnote++;

    if (footnote_definition)
    {
        footnote_count++;
        if (footnote_count == 1 && inline_footnote_count > 0)
            warning(1, (u_int8_t*)"Both inline and regular footnotes present");
        else if (footnote_count > 1)
        {
            REALLOC(footnotes, footnote_count * sizeof(KeyValue))
            pfootnotes = footnotes + footnote_count - 1;
        }
        CALLOC(pfootnotes->key, uint8_t, KEYSIZE)
        u8_strncpy(pfootnotes->key, token, KEYSIZE-1);
        pfootnotes->value = NULL;
        pfootnotes->value_size = 0;
    }
    
    if (footnote_output)
        print_output(output, "<a href=\"#footnote-%d\" id=\"footnote-text-%d\">"
                "<sup>%d</sup></a>", 
                current_footnote, current_footnote, current_footnote);

    return 0;
}

int
process_horizontal_rule(FILE* output)
{
    if (state & ST_PARA_OPEN)
        print_output(output, "</p>\n");
    print_output(output, "<hr />\n");
    if (state & ST_PARA_OPEN)
        print_output(output, "<p>\n");
    return 0;
}

int
process_text_token(uint8_t* line, BOOL first_line_in_doc,
        BOOL previous_line_blank,
        BOOL processed_start_of_line,
        BOOL read_yaml_macros_and_links, BOOL list_para,
        FILE* output, uint8_t** token,
        uint8_t** ptoken, size_t* token_size,
        BOOL add_enclosing_paragraph)
{
    if (!(state & ST_YAML))
    {
        if (add_enclosing_paragraph)
            process_line_start(line, first_line_in_doc, previous_line_blank,
                    processed_start_of_line, read_yaml_macros_and_links,
                    list_para, output, token, ptoken);
        **ptoken = 0;
        if (**token && !read_yaml_macros_and_links 
                && !(state & ST_MACRO_BODY))
            print_output(output, "%s", *token);
    }
    RESET_TOKEN(*token, *ptoken, *token_size)
    return 0;
}

int
process_formula(FILE* output, uint8_t* token, BOOL display_formula)
{
    uint8_t* command = NULL;
    int result = 0;
    CALLOC(command, uint8_t, BUFSIZE)

    u8_snprintf(command, BUFSIZE-1, "katex <<<'%s%s'", 
            display_formula ? "\\displaystyle " : "",
            token);

    result = print_command(command, output, TRUE);
    if (result)
        print_output(output, "%s", token);

    free(command);
    return result;
}

int
begin_html_and_head(FILE* output)
{
    uint8_t* site_name = get_value(vars, vars_count, (uint8_t*)"site-name", NULL);
    uint8_t* site_desc = get_value(vars, vars_count, (uint8_t*)"site-desc", NULL);
    uint8_t* favicon_url = get_value(vars, vars_count, (uint8_t*)"favicon-url", NULL);

    print_output(output, "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<title>%s</title>\n"
            "<meta charset=\"utf8\" />\n",
            site_name ? (char*)site_name : "");

    char* favicon = NULL;
    CALLOC(favicon, char, BUFSIZE)
    snprintf(favicon, BUFSIZE-1, "%s/favicon.ico", basedir);
    if (!access(favicon, R_OK))
        print_output(output, "<link rel=\"shortcut icon\" type=\"image/x-icon\""
                " href=\"%s\" />\n", 
                favicon_url ? (char*)favicon_url : "/favicon.ico");
    free(favicon);
    
    if (site_desc && *site_desc)
        print_output(output, "<meta name=\"description\" content=\"%s\" />\n",
                (char*)site_desc);

    print_output(output, "<meta name=\"viewport\" content=\"width=device-width,"
            " initial-scale=1\" />\n<meta name=\"generator\" content=\"slweb\" />\n");
    return 0;
}

int
add_css(FILE* output)
{
    KeyValue* pvars = vars;
    while (pvars < vars + vars_count)
    {
        if (!u8_strcmp(pvars->key, (uint8_t*)"stylesheet"))
            print_output(output, "<link rel=\"stylesheet\" href=\"%s\" />\n",
                    pvars->value);
        pvars++;
    }
    return 0;
}

int
end_head_start_body(FILE* output)
{
    print_output(output, "</head>\n<body>\n");

    return 0;
}

int
end_footnotes(FILE* output, BOOL add_footnote_div)
{
    size_t footnote = 0;

    if (state & ST_PARA_OPEN)
    {
        print_output(output, "</p>\n");
        state &= ~ST_PARA_OPEN;
    }

    if (add_footnote_div)
        print_output(output, "<div class=\"footnotes\">\n");

    process_horizontal_rule(output);

    for (footnote = 0; footnote < inline_footnote_count; footnote++)
        print_output(output, "<p id=\"inline-footnote-%d\">"
                "<a href=\"#inline-footnote-text-%d\">%d.</a> %s</p>\n",
                footnote+1, footnote+1, footnote+1,
                (char*)inline_footnotes[footnote]);

    KeyValue* pfootnote = footnotes;
    footnote = 0;
    while (pfootnote && footnote < footnote_count)
    {
        print_output(output, "<p id=\"footnote-%d\">"
                "<a href=\"#footnote-text-%d\">%d.</a> %s</p>\n",
                footnote+1, footnote+1, footnote+1,
                (char*)pfootnote->value);
        pfootnote++;
        footnote++;
    }

    if (add_footnote_div)
        print_output(output, "</div><!--footnotes-->\n");

    return 0;
}

int
end_body_and_html(FILE* output)
{
    print_output(output, "</body>\n</html>\n");
    return 0;
}

int
slweb_parse(uint8_t* buffer, FILE* output, BOOL body_only, 
        BOOL read_yaml_macros_and_links)
{
    uint8_t* title                     = NULL;
    uint8_t* title_heading_level       = NULL;
    uint8_t* date                      = NULL;
    uint8_t* permalink_url             = NULL;
    uint8_t* ext_in_permalink          = NULL;
    uint8_t* var_add_image_links       = NULL;
    uint8_t* var_add_footnote_div      = NULL;
    uint8_t* pbuffer                   = NULL;
    uint8_t* line                      = NULL;
    uint8_t* pline                     = NULL;
    size_t line_len                    = 0;
    uint8_t* token                     = NULL;
    uint8_t* ptoken                    = NULL;
    size_t token_size                  = 0;
    uint8_t* link_text                 = NULL;
    size_t link_size                   = 0;
    uint8_t* link_macro                = NULL;
    UBYTE heading_level                = 0;
    BOOL end_tag                       = FALSE;
    BOOL first_line_in_doc             = TRUE;
    BOOL skip_change_first_line_in_doc = FALSE;
    BOOL skip_eol                      = FALSE;
    BOOL reset_token                   = TRUE;
    BOOL previous_line_blank           = FALSE;
    BOOL processed_start_of_line       = FALSE;
    BOOL add_image_links               = TRUE;
    BOOL add_footnote_div              = FALSE;
    BOOL list_para                     = FALSE;
    BOOL yaml_parsed                   = FALSE;
    size_t pline_len                   = 0;

    if (!buffer)
        exit(error(1, (uint8_t*)"Empty buffer"));

    if (!vars)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (vars)"));

    if (!links)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (links)"));

    if (!macros)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (macros)"));

    title = get_value(vars, vars_count, (uint8_t*)"title", NULL);
    title_heading_level = get_value(vars, vars_count,
            (uint8_t*)"title-heading-level", NULL);
    date = get_value(vars, vars_count, (uint8_t*)"date", NULL);
    permalink_url = get_value(vars, vars_count, (uint8_t*)"permalink-url", NULL);
    ext_in_permalink = get_value(vars, vars_count, (uint8_t*)"ext-in-permalink", NULL);
    var_add_image_links = get_value(vars, vars_count, (uint8_t*)"add-image-links", NULL);
    add_image_links = !(var_add_image_links && *var_add_image_links == '0');
    var_add_footnote_div = get_value(vars, vars_count, (uint8_t*)"add-footnote-div", NULL);
    add_footnote_div = var_add_footnote_div && *var_add_footnote_div == '1';

    CALLOC(line, uint8_t, BUFSIZE)
    token_size = BUFSIZE;
    CALLOC(token, uint8_t, BUFSIZE)
    CALLOC(link_macro, uint8_t, BUFSIZE)

    pbuffer = buffer;
    pvars = vars;
    plinks = links;
    pmacros = macros;
    pfootnotes = footnotes;
    lineno = 0;

    if (!read_yaml_macros_and_links && !body_only)
    {
        begin_html_and_head(output);
        add_css(output);
        end_head_start_body(output);
    }

    if (title)
        print_output(output, "<h%s>%s</h%s>\n", 
                title_heading_level ? (char*)title_heading_level : "2", 
                (char*)title, 
                title_heading_level ? (char*)title_heading_level : "2");

    if (date && input_filename)
    {
        char* link = strip_ext(input_filename);
        uint8_t* samedir_permalink = get_value(vars, vars_count, 
                (uint8_t*)"samedir-permalink", NULL);
        char* real_link = NULL;
        uint8_t* permalink_macro = get_value(macros, macros_count,
                (uint8_t*)"permalink", NULL);
        CALLOC(real_link, char, BUFSIZE)

        if (ext_in_permalink && *ext_in_permalink != (ucs4_t)'0')
            strncat(link, timestamp_output_ext, BUFSIZE-strlen(link)-1);

        if (permalink_url)
            process_timestamp(output, (char*)permalink_url, permalink_macro,
                    date);
        else if (samedir_permalink && !u8_strcmp(samedir_permalink, (uint8_t*)"1"))
        {
            get_realpath(&real_link, input_dirname, link);
            process_timestamp(output, real_link, permalink_macro, date);
        }
        else
            process_timestamp(output, link, permalink_macro, date);

        free(real_link);
        free(link);
    }

    RESET_TOKEN(token, ptoken, token_size)

    do
    {
        uint8_t* eol = u8_strchr(pbuffer, (ucs4_t)'\n');
        if (!eol)
            break;

        pline = line;
        while (pbuffer != eol)
            *pline++ = *pbuffer++;
        pbuffer++;
        *pline = 0;
        pline = line;
        line_len = u8_strlen(line);

        lineno++;
        colno = 1;
        processed_start_of_line = FALSE;
        skip_eol = FALSE;
        /*list_item = FALSE;*/
        list_para = FALSE;

        while (pline && *pline)
        {
            switch (*pline)
            {
            case '-':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_MACRO_BODY | ST_PRE | ST_YAML_VAL))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                else if (colno == 1 
                        && u8_strlen(pline) > 2
                        && startswith((char*)pline, "---"))
                {
                    skip_eol = TRUE;

                    if (yaml_parsed)
                    {
                        process_horizontal_rule(output);
                        pline = NULL;
                        break;
                    }

                    state ^= ST_YAML;

                    if (!(state & ST_YAML))
                        yaml_parsed = TRUE;

                    skip_change_first_line_in_doc = TRUE;

                    pline += 3;
                    colno += 3;
                }
                else if (colno == 1 
                        && u8_strlen(pline) > 1
                        && *(pline+1) == ' ')
                {
                    if (!read_yaml_macros_and_links)
                    {
                        if (!(state & ST_LIST))
                            process_list_start(output);
                        else
                            process_list_item_end(output);
                        process_list_item_start(output);
                    }

                    processed_start_of_line = TRUE;
                    skip_eol = FALSE;

                    state |= ST_LIST;

                    pline += 2;
                    colno += 2;
                }
                else
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;
            case ':':
                if (state & ST_YAML
                        && !(state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                                | ST_HEADING | ST_MACRO_BODY | ST_PRE | ST_TAG 
                                | ST_YAML_VAL))
                        && read_yaml_macros_and_links)
                {
                    *ptoken = 0;

                    vars_count++;

                    if (vars_count > 1)
                    {
                        REALLOCARRAY(vars, KeyValue, vars_count)
                        pvars = vars + vars_count - 1;
                    }
                    CALLOC(pvars->key, uint8_t, KEYSIZE)
                    u8_strncpy(pvars->key, token, KEYSIZE-1);
                    pvars->value = NULL;
                    pvars->value_size = 0;

                    state |= ST_YAML_VAL;
                    RESET_TOKEN(token, ptoken, token_size)
                    pline++;
                    colno++;

                    while (pline && (*pline == ' ' || *pline == '\t'))
                    {
                        pline++;
                        colno++;
                    }
                }
                else {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '`':
                if (state & (ST_DISPLAY_FORMULA | ST_FORMULA | ST_MACRO_BODY))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (colno == 1 
                        && u8_strlen(pline) > 2
                        && startswith((char*)pline, "```"))
                {
                    state ^= ST_PRE;
                    
                    if (!read_yaml_macros_and_links)
                    {
                        if (state & ST_PRE)
                            print_output(output, "<pre>");
                        else
                            print_output(output, "</pre>");
                    }

                    /* Skip the rest of the line (language) */
                    pline = NULL;
                }
                else if (!(state & (ST_PRE | ST_TAG | ST_YAML)))
                {
                    /* Handle ` within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
                    {
                        uint8_t* tag = state & ST_CODE
                                ? (uint8_t*)"</code>"
                                : (uint8_t*)"<code>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to ` */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_HEADING)))
                            process_code(output, state & ST_CODE);
                    }

                    state ^= ST_CODE;

                    pline++;
                    colno++;
                }
                else
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '#':
                if (colno == 1
                        && !(state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                                | ST_MACRO_BODY | ST_PRE | ST_YAML)))
                {
                    state |= ST_HEADING;
                    heading_level = 1;
                    pline++;
                    colno++;
                }
                else if (state & ST_HEADING && *(pline-1) == '#')
                {
                    if (heading_level < MAX_HEADING_LEVEL)
                        heading_level++;
                    pline++;
                    colno++;
                }
                else 
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '_':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HTML_TAG | ST_MACRO_BODY | ST_PRE | ST_TAG 
                            | ST_YAML))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '_')
                {
                    /* Handle __ within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
                    {
                        uint8_t* tag = state & ST_BOLD
                                ? (uint8_t*)"</strong>"
                                : (uint8_t*)"<strong>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to __ */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_CODE | ST_HEADING)))
                            process_bold(output, state & ST_BOLD);
                    }

                    state ^= ST_BOLD;
                    pline += 2;
                    colno += 2;
                }
                else
                {
                    /* Handle _ within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
                    {
                        uint8_t* tag = state & ST_ITALIC
                                ? (uint8_t*)"</em>"
                                : (uint8_t*)"<em>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to _ */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_CODE | ST_HEADING)))
                            process_italic(output, state & ST_ITALIC);
                    }

                    state ^= ST_ITALIC;
                    pline++;
                    colno++;
                }
                break;

            case '*':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HTML_TAG | ST_MACRO_BODY | ST_PRE | ST_YAML))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                pline_len = u8_strlen(pline);
                if (colno == 1
                        && pline_len > 1 && *(pline+1) == '[')
                {
                    process_line_start(line, first_line_in_doc,
                            previous_line_blank, processed_start_of_line,
                            read_yaml_macros_and_links, list_para, output,
                            &token, &ptoken);

                    /* Ignore abbreviations (for now) */
                    pline = NULL;
                }
                else if (colno == 1
                        && pline_len > 2 && startswith((char*)pline, "***"))
                {
                    skip_eol = TRUE;
                    process_horizontal_rule(output);
                    pline = NULL;
                }
                else if (pline_len > 1 && *(pline+1) == '*')
                {
                    /* Handle ** within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
                    {
                        uint8_t* tag = state & ST_BOLD
                                ? (uint8_t*)"</strong>"
                                : (uint8_t*)"<strong>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to * */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_CODE | ST_HEADING)))
                            process_bold(output, state & ST_BOLD);
                    }

                    state ^= ST_BOLD;
                    pline += 2;
                    colno += 2;
                }
                else
                {
                    /* Handle * within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
 
                    {
                        uint8_t* tag = state & ST_ITALIC
                                ? (uint8_t*)"</em>"
                                : (uint8_t*)"<em>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to * */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_CODE | ST_HEADING)))
                            process_italic(output, state & ST_ITALIC);
                    }

                    state ^= ST_ITALIC;
                    pline++;
                    colno++;
                }
                break;

            case ' ':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_MACRO_BODY | ST_PRE | ST_YAML_VAL))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if ((state & ST_HEADING) && !(state & ST_HEADING_TEXT))
                {
                    if (!read_yaml_macros_and_links)
                        process_heading_start(output, heading_level);
                    state |= ST_HEADING_TEXT;
                    pline++;
                    colno++;
                    break;
                }

                if (colno == 1
                        && u8_strlen(pline) > 1
                        && startswith((char*)pline, "    "))
                {
                    list_para = TRUE;
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            list_para, output, &token, &ptoken, 
                            &token_size, TRUE);
                    processed_start_of_line = TRUE;
                    pline += 4;
                    colno += 4;
                    break;
                }

                if (u8_strlen(pline) == 2
                        && *(pline+1) == ' ')
                {
                    *ptoken = 0;
                    u8_strncat(ptoken, (uint8_t*)"<br />", 
                            BUFSIZE-u8_strlen(token)-1);
                    ptoken += strlen("<br />");
                    pline++;
                    colno++;
                }
                else if (state & ST_LINK_MACRO)
                {
                    *ptoken = 0;
                    u8_strncpy(link_macro, token, BUFSIZE-1);
                    *(link_macro + u8_strlen(token)) = 0;
                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~ST_LINK_MACRO;
                }
                else if (!(state & ST_HEADING
                        && *(pline-1) == '#'))
                    CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
                break;

            case '{':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HTML_TAG | ST_PRE | ST_YAML | ST_YAML_VAL))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (state & ST_MACRO_BODY)
                {
                    *ptoken = 0;
                    size_t token_len = u8_strlen(token);

                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pmacros->value)
                        {
                            size_t value_len = u8_strlen(pmacros->value);

                            if (pmacros->value_size < value_len + token_len + 1)
                            {
                                pmacros->value_size += token_len;
                                REALLOC(pmacros->value, pmacros->value_size)
                            }
                            u8_strncat(pmacros->value, token, pmacros->value_size-1);
                        }
                        else
                        {
                            pmacros->value_size = token_len+1;
                            CALLOC(pmacros->value, uint8_t, pmacros->value_size)
                            u8_strncpy(pmacros->value, token, pmacros->value_size-1);
                        }
                    }
                }
                else
                {
                    /* Output existing text up to { */
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank, processed_start_of_line,
                            read_yaml_macros_and_links, list_para, output,
                            &token, &ptoken, &token_size, FALSE);
                    processed_start_of_line = TRUE;
                }

                state |= ST_TAG;
                RESET_TOKEN(token, ptoken, token_size)

                pline++;
                colno++;
                break;

            case '/':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_PRE | ST_HEADING | ST_YAML_VAL))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if ((state & ST_TAG) && pline-1 && *(pline-1) == '{')
                {
                    end_tag = TRUE;
                    pline++;
                }
                else
                    CHECKCOPY(token, ptoken, token_size, pline)

                colno++;
                break;

            case '}':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_PRE | ST_YAML | ST_YAML_VAL))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (state & ST_TAG)
                {
                    state &= ~ST_TAG;
                    *ptoken = 0;

                    process_tag(token, output, read_yaml_macros_and_links,
                            &skip_eol, end_tag);

                    RESET_TOKEN(token, ptoken, token_size)
                    end_tag = FALSE;
                }

                pline++;
                colno++;
                break;

            case '|':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HTML_TAG | ST_PRE | ST_TAG | ST_YAML))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '|')
                {
                    /* Handle || within footnotes, headings and link text specially */
                    if (state & (ST_INLINE_FOOTNOTE | ST_HEADING 
                                | ST_FOOTNOTE_TEXT | ST_LINK))
                    {
                        uint8_t* tag = state & ST_KBD
                                ? (uint8_t*)"</kbd>"
                                : (uint8_t*)"<kbd>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to || */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, 
                                &token_size, TRUE);
                        processed_start_of_line = TRUE;

                        if (!read_yaml_macros_and_links 
                                && !(state & (ST_PRE | ST_CODE | ST_HEADING)))
                        {
                            state ^= ST_KBD;
                            process_kbd(output, !(state & ST_KBD));
                        }
                    }

                    pline += 2;
                    colno += 2;
                }
                else
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '<':
                if (read_yaml_macros_and_links
                        || (state & (ST_DISPLAY_FORMULA | ST_FORMULA)))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (state & (ST_CODE | ST_KBD | ST_PRE))
                {
                    size_t lt_len = u8_strlen((uint8_t*)"&lt;");
                    *ptoken = 0;
                    u8_strncat(token, (uint8_t*)"&lt;", 
                            BUFSIZE-(ptoken-token)-1);
                    ptoken += lt_len;
                    pline++;
                    colno++;
                    break;
                }

                state |= ST_HTML_TAG;
                CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
                break;

            case '>':
                if (state & ST_HTML_TAG)
                    state &= ~ST_HTML_TAG;

                if (state & (ST_DISPLAY_FORMULA | ST_FORMULA | ST_MACRO_BODY))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (!read_yaml_macros_and_links
                        && !(state & (ST_CODE | ST_HEADING | ST_PRE))
                        && colno == 1)
                {
                    if (!read_yaml_macros_and_links 
                            && !(state & ST_BLOCKQUOTE))
                        process_blockquote(output, FALSE);

                    state |= ST_BLOCKQUOTE;

                    pline++;
                    colno++;
                }
                else 
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '!':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_FOOTNOTE_TEXT | ST_HEADING 
                            | ST_INLINE_FOOTNOTE | ST_LINK | ST_MACRO_BODY 
                            | ST_PRE))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '[')
                {
                    /* Output existing text up to ! */
                    *ptoken = 0;
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank, processed_start_of_line,
                            read_yaml_macros_and_links, list_para, output,
                            &token, &ptoken, &token_size, FALSE);
                    processed_start_of_line = TRUE;

                    RESET_TOKEN(token, ptoken, token_size)

                    state |= ST_IMAGE;
                    pline += 2;
                    colno += 2;
                }
                else 
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }

                break;

            case '=':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_MACRO_BODY | ST_PRE | ST_TAG))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (colno != 1 && *(pline-1) == '[')
                {
                    state |= ST_LINK_MACRO;
                    pline++;
                    colno++;
                }
                else 
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                }
                break;

            case '[':
                pline_len = u8_strlen(pline);

                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HEADING | ST_MACRO_BODY | ST_PRE))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (pline_len > 1 && *(pline+1) == '^')
                {
                    /* Output existing text up to [^ */
                    *ptoken = 0;
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank, processed_start_of_line,
                            read_yaml_macros_and_links, list_para, output,
                            &token, &ptoken, &token_size, TRUE);
                    processed_start_of_line = TRUE;

                    RESET_TOKEN(token, ptoken, token_size)
                    state |= ST_FOOTNOTE;
                    pline += 2;
                    colno += 2;
                    break;
                }

                if (*token)
                    /* Output existing text up to [ */
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            list_para, output, &token, &ptoken, &token_size, 
                            TRUE);
                processed_start_of_line = TRUE;

                RESET_TOKEN(token, ptoken, token_size)

                state |= ST_LINK;
                *link_macro = 0;

                pline++;
                colno++;
                break;

            case '(':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_FOOTNOTE_TEXT | ST_HEADING 
                            | ST_INLINE_FOOTNOTE | ST_MACRO_BODY | ST_PRE))
                    CHECKCOPY(token, ptoken, token_size, pline)
                else if (state & ST_LINK)
                {
                    uint8_t* tag = (uint8_t*)"<span>";
                    size_t tag_len = u8_strlen(tag);
                    size_t token_len = 0;

                    *ptoken = 0;
                    token_len = u8_strlen(token);

                    if (token_len + tag_len < BUFSIZE)
                    {
                        u8_strncat(token, tag, BUFSIZE-token_len-1);
                        ptoken += tag_len;
                    }

                    state |= ST_LINK_SPAN;
                    pline++;
                }
                else
                    CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
                break;

            case ')':
                if (state & (ST_DISPLAY_FORMULA | ST_FORMULA))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (state & ST_LINK_SPAN)
                {
                    if (u8_strlen(pline) > 1
                            && *(pline+1) == ']')
                    {
                        uint8_t* tag = (uint8_t*)"</span>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = 0;
                        token_len = u8_strlen(token);

                        if (token_len + tag_len + 1 < BUFSIZE)
                        {
                            u8_strncat(token, tag, BUFSIZE-token_len-1);
                            ptoken += tag_len;
                        }
                        state &= ~ST_LINK_SPAN;
                    }
                    pline++;
                }
                else if (state & (ST_LINK_SECOND_ARG | ST_IMAGE_SECOND_ARG))
                {
                    *ptoken = 0;
                    if (!read_yaml_macros_and_links)
                    {
                        if (state & ST_LINK_SECOND_ARG)
                            process_inline_link(link_text, 
                                    get_value(macros, macros_count, 
                                        link_macro, NULL), 
                                    token, output);
                        else
                            process_inline_image(link_text, token, output, 
                                    add_image_links);
                    }
                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~(ST_LINK | ST_LINK_SECOND_ARG 
                            | ST_IMAGE | ST_IMAGE_SECOND_ARG);
                    pline++;
                }
                else
                    CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
                break;

            case ']':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_HEADING | ST_MACRO_BODY | ST_PRE))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                *ptoken = 0;
                if (state & ST_INLINE_FOOTNOTE)
                {
                    process_inline_footnote(token, read_yaml_macros_and_links,
                            output);

                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~ST_INLINE_FOOTNOTE;
                    pline++;
                    colno++;
                }
                else if (state & ST_FOOTNOTE)
                {
                    BOOL footnote_definition = (u8_strlen(pline) > 1)
                            && (*(pline+1) == ':');

                    process_footnote(token, 
                            footnote_definition && read_yaml_macros_and_links,
                            !footnote_definition && !read_yaml_macros_and_links,
                            output);

                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~ST_FOOTNOTE;
                    if (footnote_definition)
                    {
                        state |= ST_FOOTNOTE_TEXT;
                        pline++;
                        colno++;
                    }
                    pline++;
                    colno++;
                }
                else if (state & ST_LINK_SECOND_ARG)
                {
                    if (!read_yaml_macros_and_links)
                        process_link(link_text, 
                                get_value(macros, macros_count, 
                                    link_macro, NULL), 
                                token, output);
                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~(ST_LINK | ST_LINK_SECOND_ARG);
                    pline++;
                    colno++;
                }
                else if (state & ST_IMAGE_SECOND_ARG)
                {
                    if (!read_yaml_macros_and_links)
                        process_image(link_text, token, output, add_image_links);
                    RESET_TOKEN(token, ptoken, token_size)
                    state &= ~(ST_IMAGE | ST_IMAGE_SECOND_ARG);
                    pline++;
                    colno++;
                }
                else if (u8_strlen(pline) > 1)
                {
                    size_t token_len = 0;
                    switch (*(pline+1))
                    {
                        case ':':
                            links_count++;

                            if (links_count > 1)
                            {
                                REALLOCARRAY(links, KeyValue, links_count)
                                plinks = links + links_count - 1;
                            }
                            CALLOC(plinks->key, uint8_t, KEYSIZE)
                            u8_strncpy(plinks->key, token, KEYSIZE-1);
                            plinks->value = NULL;
                            plinks->value_size = 0;
                            pline += 2;
                            colno += 2;
                            while (pline && (*pline == ' ' || *pline == '\t'))
                            {
                                pline++;
                                colno++;
                            }
                            RESET_TOKEN(token, ptoken, token_size)
                            state |= ST_LINK_SECOND_ARG;
                            break;

                        case '[':
                        case '(':
                            token_len = u8_strlen(token);
                            if (link_text)
                            {
                                if (token_len + 1 > link_size)
                                {
                                    link_size = token_len+1;
                                    REALLOC(link_text, link_size)
                                }
                            }
                            else
                            {
                                link_size = token_len+1;
                                CALLOC(link_text, uint8_t, link_size)
                            }
                            u8_strncpy(link_text, token, link_size-1);
                            *(link_text + token_len) = 0;
                            pline += 2;
                            colno += 2;
                            RESET_TOKEN(token, ptoken, token_size)
                            if (state & ST_LINK)
                                state |= ST_LINK_SECOND_ARG;
                            else
                                state |= ST_IMAGE_SECOND_ARG;
                            break;
                        default:
                            CHECKCOPY(token, ptoken, token_size, pline)
                            colno++;
                            state &= ~(ST_LINK | ST_IMAGE);
                            break;
                    }
                }
                break;

            case '^':
                if (state & (ST_CODE | ST_DISPLAY_FORMULA | ST_FORMULA 
                            | ST_FOOTNOTE_TEXT | ST_INLINE_FOOTNOTE | ST_PRE 
                            | ST_YAML))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '[')
                {
                    /* Output existing text up to ^[ */
                    *ptoken = 0;
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank, processed_start_of_line,
                            read_yaml_macros_and_links, list_para, output,
                            &token, &ptoken, &token_size, TRUE);
                    processed_start_of_line = TRUE;

                    RESET_TOKEN(token, ptoken, token_size)
                    state |= ST_INLINE_FOOTNOTE;
                    pline += 2;
                    colno += 2;
                    break;
                }

                CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
                break;

            case '\\':
                if (state & (ST_DISPLAY_FORMULA | ST_FORMULA | ST_HTML_TAG 
                            | ST_MACRO_BODY))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1)
                {
                    pline++;
                    colno++;

                    CHECKCOPY(token, ptoken, token_size, pline)
                }
                else
                {
                    pline = NULL;
                    break;
                }

                colno++;
                break;

            case '$':
                if (state & (ST_CODE | ST_CSV_BODY | ST_PRE | ST_YAML))
                {
                    CHECKCOPY(token, ptoken, token_size, pline)
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1
                        && *(pline+1) == '$')
                {
                    if (state & ST_FORMULA)
                        exit(error(1, (uint8_t*)"Display formula within an"
                                    " open formula"));

                    if (!(state & ST_DISPLAY_FORMULA))
                    {
                        /* Output existing text up to $$ */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, &token_size, 
                                TRUE);
                        processed_start_of_line = TRUE;
                    }

                    state ^= ST_DISPLAY_FORMULA;

                    if (state & ST_DISPLAY_FORMULA)
                        reset_token = FALSE;
                    else
                    {
                        *ptoken = 0;

                        if (!read_yaml_macros_and_links)
                            process_formula(output, token, TRUE);

                        RESET_TOKEN(token, ptoken, token_size)
                        reset_token = TRUE;
                    }

                    pline += 2;
                    colno += 2;
                }
                else
                {
                    if (state & ST_DISPLAY_FORMULA)
                        exit(error(1, (uint8_t*)"Formula within an open"
                                    " display formula"));

                    if (!(state & ST_FORMULA))
                    {
                        /* Output existing text up to $ */
                        process_text_token(line, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                list_para, output, &token, &ptoken, &token_size, 
                                TRUE);
                        processed_start_of_line = TRUE;
                    }

                    state ^= ST_FORMULA;

                    if (state & ST_FORMULA)
                        reset_token = FALSE;
                    else
                    {
                        *ptoken = 0;

                        if (!read_yaml_macros_and_links)
                            process_formula(output, token, FALSE);

                        RESET_TOKEN(token, ptoken, token_size)
                        reset_token = TRUE;
                    }

                    pline++;
                    colno++;
                }
                break;

            default:
                CHECKCOPY(token, ptoken, token_size, pline)
                colno++;
            }
        }

        *ptoken = 0;

        if (*token && read_yaml_macros_and_links
                && (state & ST_YAML_VAL))
        {
            *ptoken = 0;
            pvars->value_size = u8_strlen(token)+1;
            CALLOC(pvars->value, uint8_t, pvars->value_size)
            u8_strncpy(pvars->value, token, pvars->value_size-1);
        }
        else if (!(state & (ST_DISPLAY_FORMULA | ST_FORMULA)))
        {
            if (*token)
            {
                *ptoken = 0;
                size_t token_len = u8_strlen(token);
                if (state & ST_MACRO_BODY)
                {
                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pmacros->value)
                        {
                            size_t value_len = u8_strlen(pmacros->value);

                            if (sizeof(pmacros->value) < value_len + token_len + 1)
                            {
                                pmacros->value_size += BUFSIZE;
                                REALLOC(pmacros->value, pmacros->value_size)
                            }
                            u8_strncat(pmacros->value, token, 
                                    pmacros->value_size-value_len-1);
                            u8_strncat(pmacros->value, (uint8_t*)"\n", 
                                    pmacros->value_size-u8_strlen(pmacros->value)-1);
                        }
                        else
                        {
                            pmacros->value_size = BUFSIZE;
                            CALLOC(pmacros->value, uint8_t, pmacros->value_size)
                            u8_strncpy(pmacros->value, token, pmacros->value_size-1);
                            u8_strncat(pmacros->value, (uint8_t*)"\n", 
                                    pmacros->value_size-u8_strlen(pmacros->value)-1);
                        }
                    }
                }
                else if (state & ST_FOOTNOTE_TEXT)
                {
                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pfootnotes->value)
                        {
                            size_t value_len = u8_strlen(pfootnotes->value);

                            if (sizeof(pfootnotes->value) < value_len + token_len + 1)
                            {
                                pfootnotes->value_size += BUFSIZE;
                                REALLOC(pfootnotes->value, pfootnotes->value_size)
                            }
                            u8_strncat(pfootnotes->value, token,
                                    pfootnotes->value_size-value_len-1);
                            u8_strncat(pfootnotes->value, (uint8_t*)"\n",
                                    pfootnotes->value_size-u8_strlen(pfootnotes->value)-1);
                        }
                        else
                        {
                            pfootnotes->value_size = BUFSIZE;
                            CALLOC(pfootnotes->value, uint8_t, pfootnotes->value_size)
                            u8_strncpy(pfootnotes->value, token, pfootnotes->value_size-1);
                            u8_strncat(pfootnotes->value, (uint8_t*)"\n",
                                    pfootnotes->value_size-u8_strlen(pfootnotes->value)-1);
                        }
                    }
                }
                else if (state & ST_LINK_SECOND_ARG)
                {
                    if (read_yaml_macros_and_links)
                    {
                        plinks->value_size = u8_strlen(token)+1;
                        CALLOC(plinks->value, uint8_t, plinks->value_size)
                        u8_strncpy(plinks->value, token, plinks->value_size-1);
                    }
                }
                else if (state & ST_HEADING)
                {
                    state &= ~(ST_HEADING | ST_HEADING_TEXT);
                    if (!read_yaml_macros_and_links)
                        process_heading(token, output, heading_level);
                    first_line_in_doc = FALSE;
                    RESET_TOKEN(token, ptoken, token_size)
                    heading_level = 0;
                }
                else 
                    process_text_token(line, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            list_para, output, &token, &ptoken, &token_size, 
                            TRUE);
            }

            if (!(state & (ST_PRE | ST_LINK_SECOND_ARG))
                        && (!*pbuffer || *pbuffer == '\n'))
            {
                if (state & ST_PARA_OPEN)
                {
                    if (!read_yaml_macros_and_links)
                        print_output(output, "</p>");
                    if (state & ST_LIST)
                        skip_eol = TRUE;
                    state &= ~ST_PARA_OPEN;
                }

                if (!*pbuffer && (state & ST_LIST))
                {
                    if (!read_yaml_macros_and_links)
                    {
                        process_list_item_end(output);
                        process_list_end(output);
                    }
                    state &= ~ST_LIST;
                }

                if (!*pbuffer && (state & ST_FOOTNOTE_TEXT))
                {
                    if (!read_yaml_macros_and_links)
                        print_output(output, "</p>\n");
                    state &= ~ST_FOOTNOTE_TEXT;

                    *ptoken = 0;
                    size_t token_len = u8_strlen(token);

                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pmacros->value)
                        {
                            size_t value_len = u8_strlen(pmacros->value);

                            if (pmacros->value_size < value_len + token_len + 1)
                            {
                                pmacros->value_size += token_len;
                                REALLOC(pmacros->value, pmacros->value_size)
                            }
                            u8_strncat(pmacros->value, token, pmacros->value_size-1);
                        }
                        else
                        {
                            pmacros->value_size = token_len+1;
                            CALLOC(pmacros->value, uint8_t, pmacros->value_size)
                            u8_strncpy(pmacros->value, token, pmacros->value_size-1);
                        }
                    }
                }
            }

            if (state & ST_BLOCKQUOTE 
                    && (!*pbuffer || *pbuffer != '>'))
            {
                state &= ~ST_BLOCKQUOTE;
                process_blockquote(output, TRUE);
            }

            previous_line_blank = FALSE;
        }

        if (!skip_eol && !read_yaml_macros_and_links 
                && !(state & (ST_YAML | ST_YAML_VAL 
                        | ST_LINK_SECOND_ARG)))
                print_output(output, "\n");

        if (reset_token)
            RESET_TOKEN(token, ptoken, token_size)

        if (!skip_change_first_line_in_doc
                && !(state & ST_YAML))
            first_line_in_doc = FALSE;

        skip_change_first_line_in_doc = FALSE; 
        if (line_len == 0)
            previous_line_blank = TRUE;

        /* Lasts until the end of line */
        state &= ~(ST_YAML_VAL | ST_LINK | ST_LINK_SECOND_ARG);
    }
    while (pbuffer && *pbuffer);

    if (!read_yaml_macros_and_links 
            && (footnote_count > 0 || inline_footnote_count > 0))
        end_footnotes(output, add_footnote_div);

    if (!read_yaml_macros_and_links && !body_only)
        end_body_and_html(output);

    if (link_text)
        free(link_text);

    free(link_macro);
    free(token);
    free(line);

    return 0;
}

int
main(int argc, char** argv)
{
    char* arg;
    Command cmd = CMD_NONE;
    BOOL body_only = FALSE;
    int result = 0;

    basedir_size = 2;
    CALLOC(basedir, char, basedir_size)
    *basedir = '.';

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
                    cmd = CMD_VERSION;
                else if (!strcmp(arg, "body-only"))
                {
                    arg += strlen("body-only");
                    body_only = TRUE;
                }
                else if (startswith(arg, "basedir"))
                {
                    arg += strlen("basedir");
                    result = set_basedir(arg, &basedir, &basedir_size);
                    if (result)
                        return result;
                }
                else if (!strcmp(arg, "help"))
                    return usage();
                else
                {
                    error(EINVAL, (uint8_t*)"Invalid argument: --%s", arg);
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
                    error(EINVAL, (uint8_t*)"Invalid argument: -%c", c);
                    return usage();
                }
            }
        }
        else
        {
            int result;

            if (cmd == CMD_BASEDIR)
            {
                result = set_basedir(arg, &basedir, &basedir_size);
                if (result)
                    return result;
            }
            else
                input_filename = arg;
            cmd = CMD_NONE;
        }
    }

    if (cmd == CMD_BASEDIR)
        return error(1, (uint8_t*)"-d: Argument required");

    if (cmd == CMD_VERSION)
        return version();

    FILE* input        = NULL;
    FILE* output       = stdout;
    uint8_t* buffer    = NULL;
    size_t buffer_size = 0;

    if (input_filename)
        read_file_into_buffer(&buffer, &buffer_size, input_filename, &input_dirname, &input);
    else
    {
        uint8_t* bufline   = NULL;
        size_t buffer_len  = 0;
        size_t buffer_size = 0;
        size_t bufline_len = 0;

        input = stdin;

        CALLOC(bufline, uint8_t, BUFSIZE)
        buffer_size = BUFSIZE;
        CALLOC(buffer, uint8_t, buffer_size)

        while (!feof(input))
        {
            uint8_t* eol = NULL;
            if (!fgets((char*)bufline, BUFSIZE-1, input))
                break;
            eol = u8_strchr(bufline, (ucs4_t)'\n');
            if (eol)
                *(eol+1) = 0;
            bufline_len = u8_strlen(bufline);
            if (buffer_len + bufline_len + 1 > buffer_size)
            {
                buffer_size += BUFSIZE;
                REALLOC(buffer, buffer_size)
            }
            u8_strncat(buffer, bufline, buffer_size-buffer_len-1);
            buffer_len += bufline_len;
        }
        free(bufline);
    }

    CALLOC(vars, KeyValue, 1)
    vars->key = NULL;
    vars->value = NULL;
    vars->value_size = 0;

    CALLOC(macros, KeyValue, 1)
    macros->key = NULL;
    macros->value = NULL;
    macros->value_size = 0;

    CALLOC(links, KeyValue, 1)
    links->key = NULL;
    links->value = NULL;
    links->value_size = 0;

    CALLOC(footnotes, KeyValue, 1)
    footnotes->key = NULL;
    footnotes->value = NULL;
    footnotes->value_size = 0;

    CALLOC(inline_footnotes, uint8_t*, 1)
    *inline_footnotes = NULL;
    inline_footnote_count = 0;

    /* First pass: read YAML, macros and links */
    result = slweb_parse(buffer, output, body_only, TRUE);

    if (result)
    {
        free(buffer);
        return result;
    }

    state = ST_NONE;
    current_footnote = 0;
    current_inline_footnote = 0;

    /* Second pass: parse and output */
    result = slweb_parse(buffer, output, body_only, FALSE);

    free(buffer);

    return result;
}

