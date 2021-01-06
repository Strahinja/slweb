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

static size_t lineno        = 0;
static size_t colno         = 1;
static char* input_filename = NULL;
static char* input_dirname  = NULL;
static char* basedir        = NULL;
static char* arg_zero       = NULL;

#define CHECKEXITNOMEM(ptr) { if (!ptr) exit(error(ENOMEM, \
                (uint8_t*)"Memory allocation failed (out of memory?)\n")); }

#define CALLOC(ptr, ptrtype, nmemb) { \
    ptr = calloc(nmemb, sizeof(ptrtype)); \
    CHECKEXITNOMEM(ptr) }

#define REALLOC(ptr, ptrtype, newsize) { \
    ptr = realloc(ptr, newsize); \
    CHECKEXITNOMEM(ptr) }

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
    fprintf(stderr, "%s:%lu:%lu: %s", PROGRAMNAME, lineno, colno, buf);
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
    fprintf(stderr, "Warning: %s", buf);
    return code;
}

int
slweb_parse(uint8_t* buffer, FILE* output, 
        KeyValue** vars, size_t* vars_count, 
        KeyValue** macros, size_t* macros_count,
        KeyValue** links, size_t* links_count,
        BOOL body_only, BOOL read_yaml_macros_and_links);

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

    for (int i = start; i < finish && *(src+i) != '\0'; i++)
        *presult++ = *(src+i);
    *presult = '\0';

    return result;
}

BOOL
startswith(const char* s, const char* what)
{
    return s && what && !strcmp(substr(s, 0, strlen(what)), what);
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
set_basedir(char* arg, char** basedir)
{
    size_t basedir_len = 0;
    if (*basedir)
        free(*basedir);

    if (strlen(arg) < 1)
        exit(error(1, (uint8_t*)"--basedir: Argument required\n"));

    CALLOC(*basedir, char, strlen(arg)+1)
    strcpy(*basedir, arg);
    basedir_len = strlen(*basedir);
    if (*(*basedir + basedir_len - 1) == '/')
        *(*basedir + basedir_len - 1) = '\0';

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
    CALLOC(newname, char, strlen(fn))
    pnewname = newname;
    pfn = fn;
    while (pfn != dot && *pfn)
        *pnewname++ = *pfn++;
    return newname;
}

int
print_command(uint8_t* command, FILE* output)
{
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
            *eol = '\0';

        fprintf(output, "%s\n", cmd_output_line);

    }
    free(cmd_output_line);
    pclose(cmd_output);

    return 0;
}

int
read_file_into_buffer(uint8_t** buffer, char* input_filename, 
        char** input_dirname, FILE** input)
{
    struct stat fs;
    char* slash = NULL;

    *input = fopen(input_filename, "r");
    if (!*input)
        return error(ENOENT, (uint8_t*)"No such file: %s\n", input_filename);

    fstat(fileno(*input), &fs);
    CALLOC(*buffer, uint8_t, fs.st_size)
    fread((void*)*buffer, sizeof(char), fs.st_size, *input);

    slash = strrchr(input_filename, '/');
    if (slash)
    {
        CALLOC(*input_dirname, char, BUFSIZE)
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
process_heading(uint8_t* token, FILE* output, UBYTE heading_level)
{
    if (!token || u8_strlen(token) < 1)
        warning(1, (uint8_t*)"Empty heading\n");

    fprintf(output, "<h%d>%s</h%d>", heading_level, token ? (char*)token : "",
            heading_level);

    return 0;
}

int
process_git_log(FILE* output)
{
    if (!input_filename)
        return warning(1, (uint8_t*)"Cannot use 'git-log' in stdin\n");

    char* basename = NULL;
    CALLOC(basename, char, strlen(input_filename))
    char* slash = strrchr(input_filename, '/');
    if (slash)
        strncpy(basename, slash+1, strlen(slash+1));
    else
        strncpy(basename, input_filename, strlen(input_filename));

    fprintf(output, "<div id=\"git-log\">\nPrevious commit:\n");
    uint8_t* command = NULL;
    CALLOC(command, uint8_t, BUFSIZE)
    u8_snprintf(command, BUFSIZE,
            "git log -1 --pretty=format:\"%s %%h %%ci (%%cn) %%d\""
            " || echo \"(Not in a Git repository)\"",
        basename);
    if (print_command(command, output))
    {
        fprintf(output, "</div><!--git-log-->\n");
        return error(1, (uint8_t*)"git-log: Cannot run git\n");
    }
    fprintf(output, "</div><!--git-log-->\n");

    free(command);
    free(basename);

    return 0;
}

int
process_include(uint8_t* token, FILE* output, ULONG* state, 
        KeyValue** vars, size_t* vars_count,
        KeyValue** macros, size_t* macros_count,
        KeyValue** links, size_t* links_count,
        BOOL read_yaml_macros_and_links)
{
    if (read_yaml_macros_and_links)
        return 0;

    if (!arg_zero)
        exit(error(EINVAL, (uint8_t*)"Invalid argument zero\n"));

    if (!input_filename)
        return warning(1, (uint8_t*)"Cannot use 'include' in stdin\n");

    uint8_t* ptoken = u8_strchr(token, (ucs4_t)' ');
    char* include_filename = NULL;
    char* pinclude_filename = NULL;
    CALLOC(include_filename, char, BUFSIZE)
    pinclude_filename = include_filename;
    
    if (!ptoken)
        return warning(1, (uint8_t*)"Directive 'include' requires"
                " an argument\n");

    ptoken++;
    while (ptoken && *ptoken)
        if (*ptoken != '"')
            *pinclude_filename++ = *ptoken++;
        else 
            ptoken++;

    fflush(output);
    pid_t pid = fork();
    int pstatus = 0;

    if (pid > 0)
        wait(&pstatus);
    else if (pid == 0)
    {
        if (!strcmp(basedir, "."))
            strncpy(basedir, input_dirname, strlen(input_dirname));

        sprintf(input_filename, "%s/%s.slw",
                basedir,
                include_filename);

        FILE* input = NULL;
        FILE* output = stdout;
        uint8_t* buffer = NULL;

        read_file_into_buffer(&buffer, input_filename, &input_dirname, &input);

        free(*links);
        CALLOC(*links, KeyValue, 1)
        (*links)->key = NULL;
        (*links)->value = NULL;
        *links_count = 0;

        /* First pass: read YAML, macros and links */
        slweb_parse(buffer, output, vars, vars_count, macros, macros_count,
                links, links_count, TRUE, TRUE);

        /* Second pass: parse and output */
        slweb_parse(buffer, output, vars, vars_count, macros, macros_count, 
                links, links_count, TRUE, FALSE);

        exit(0);
    }
    else
        exit(error(1, (uint8_t*)"Fork failed\n"));

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
    snprintf(nodename, BUFSIZE, "%s/%s", basedir, node->d_name);
    
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

    return (!strcmp(substr(node->d_name, 
                    node_len - slw_len,
                    node_len), ".slw"));
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
        uint8_t* macro_body, ULONG* state,
        KeyValue** vars, size_t* vars_count,
        KeyValue** macros, size_t* macros_count,
        KeyValue** links, size_t* links_count)
{
    fprintf(output, "<li>\n<details%s>\n<summary>", details_open ? " open" : "");
    if (macro_body)
        fprintf(stdout, "%s", macro_body);
    fprintf(output, "%s</summary>\n<div>\n", subdirname);

    struct dirent** namelist;
    struct dirent** pnamelist;
    long names_total;
    long names_output;
    char* abs_subdirname = NULL;

    CALLOC(abs_subdirname, char, BUFSIZE)
    snprintf(abs_subdirname, BUFSIZE, "%s/%s", basedir, subdirname);

    if ((names_total = scandir(abs_subdirname, &namelist, &filter_slw, 
                &reverse_alphacompare)) < 0)
    {
        perror("scandir");
        exit(error(errno, (uint8_t*)"incdir_subdir: scandir error\n"));
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
            strncpy(basedir, abs_subdirname, strlen(abs_subdirname));
            sprintf(input_filename, "%s/%s", abs_subdirname, 
                    (*pnamelist)->d_name);

            FILE* input = NULL;
            FILE* output = stdout;
            uint8_t* buffer = NULL;

            read_file_into_buffer(&buffer, input_filename, &input_dirname, 
                    &input);

            free(*links);
            CALLOC(*links, KeyValue, 1)
            (*links)->key = NULL;
            (*links)->value = NULL;
            *links_count = 0;

            /* First pass: read YAML, macros and links */
            slweb_parse(buffer, output, vars, vars_count, macros, macros_count,
                    links, links_count, TRUE, TRUE);

            /* Second pass: parse and output */
            slweb_parse(buffer, output, vars, vars_count, macros, macros_count, 
                    links, links_count, TRUE, FALSE);
            exit(0);
        }
        else
            exit(error(1, (uint8_t*)"Fork failed\n"));

        pnamelist++;
        names_output++;
    }

    free(abs_subdirname);

    fprintf(output, "</div>\n</details>\n</li>\n");
    return 0;
}

int
process_incdir(uint8_t* token, FILE* output, KeyValue** vars, 
        size_t* vars_count, KeyValue** macros, size_t* macros_count, 
        KeyValue** pmacros, KeyValue** links, size_t* links_count, ULONG* state, 
        BOOL read_yaml_macros_and_links)
{
    if (read_yaml_macros_and_links)
        return 0;

    uint8_t* saveptr = NULL;
    /* skipping the first token (incdir) */
    uint8_t* arg = u8_strtok(token, (uint8_t*)" ", &saveptr);
    long num = 5;
    uint8_t* macro_body = NULL;
    struct dirent** namelist;
    struct dirent** pnamelist;
    long names_output;
    BOOL details_open = TRUE;

    arg = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
    if (!arg)
        return warning(1, (uint8_t*)"incdir: Arguments required\n");
    if (*arg == '=')
        macro_body = get_value(*macros, *macros_count, arg+1, NULL);
    else
    {
        uint8_t* parg = arg;
        while (parg && *parg)
        {
            if (*parg < '0' || *parg > '9')
                return warning(1, (uint8_t*)"incdir: Non-numeric argument\n");
            parg++;
        }
        num = strtol((char*)arg, NULL, 10);
        if (errno)
            return warning(errno, (uint8_t*)"incdir: Invalid num\n");
        arg = u8_strtok(NULL, (uint8_t*)" ", &saveptr);
        if (*arg != '=')
            return warning(1, (uint8_t*)"incdir: Second argument not macro\n");
        macro_body = get_value(*macros, *macros_count, arg+1, NULL);
    }

    fprintf(output, "<ul class=\"incdir\">\n");

    if (scandir(basedir, &namelist, &filter_subdirs,
            &reverse_alphacompare) < 0)
    {
        perror("scandir");
        exit(error(errno, (uint8_t*)"incdir: scandir error\n"));
    }

    pnamelist = namelist;
    names_output = 0;
    while (names_output < num && pnamelist && *pnamelist)
    {
        process_incdir_subdir((*pnamelist)->d_name, output, details_open,
                macro_body, state,
                vars, vars_count, macros, macros_count, links, links_count);
        details_open = FALSE;
        pnamelist++;
        names_output++;
    }
    free(namelist);

    fprintf(output, "</ul>\n");

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
                        u8_strncat(formatted_date, day, u8_strlen(day));
                    else if (*ptimestamp_format == 'm' 
                            || *ptimestamp_format == 'M')
                        u8_strncat(formatted_date, month, u8_strlen(month));
                    else if (*ptimestamp_format == 'y' 
                            || *ptimestamp_format == 'Y')
                        u8_strncat(formatted_date, year, u8_strlen(year));
                    else
                        *(formatted_date + u8_strlen(formatted_date)) 
                            = *ptimestamp_format;

                    ptimestamp_format++;
                }
                fprintf(output, "<a href=\"%s\" class=\"timestamp\">%s%s</a>\n",
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
process_macro(uint8_t* token, FILE* output, KeyValue** macros, 
        size_t* macros_count, KeyValue** pmacros, ULONG* state, 
        BOOL read_yaml_macros_and_links, BOOL end_tag)
{
    if (!end_tag)
    {
        BOOL seen = FALSE;

        uint8_t* macro_body = get_value(*macros, *macros_count, token+1,
                read_yaml_macros_and_links ? NULL : &seen);

        if (macro_body)
        {
            if (!read_yaml_macros_and_links)
            {
                if (seen)
                    fprintf(output, "%s", macro_body);
                else
                    *state |= ST_MACRO_BODY;
            }
        }
        else
        {
            if (read_yaml_macros_and_links)
            {
                (*macros_count)++;

                if (*macros_count > 1)
                {
                    REALLOC(*macros, KeyValue, 
                            *macros_count * sizeof(KeyValue))
                    *pmacros = *macros + *macros_count - 1;
                }
                CALLOC((*pmacros)->key, uint8_t, u8_strlen(token+1))
                (*pmacros)->seen = FALSE;
                u8_strcpy((*pmacros)->key, token+1);
                (*pmacros)->value = NULL;
            }
            *state |= ST_MACRO_BODY;
        }
    }
    else
        *state &= ~ST_MACRO_BODY;

    return 0;
}

int
process_tag(uint8_t* token, FILE* output, KeyValue** macros, 
        size_t* macros_count, KeyValue** pmacros, KeyValue** vars,
        size_t* vars_count, KeyValue** links, size_t* links_count,
        ULONG* state, BOOL read_yaml_macros_and_links, 
        BOOL* skip_eol, BOOL end_tag)
{
    if (!token || u8_strlen(token) < 1)
        return warning(1, (uint8_t*)"Empty tag name\n");

    if (!strcmp((char*)token, "git-log")
            && !read_yaml_macros_and_links)   /* {git-log} */
    {
        process_git_log(output);
    }
    else if (!strcmp((char*)token, "made-by")
            && !read_yaml_macros_and_links)   /* {made-by} */
    {
        fprintf(output, "<div id=\"made-by\">\n"
                "Generated by <a href=\"https://github.com/Strahinja/slweb\">"
                "slweb</a>\n"
                "© %s Strahinya Radich.\n"
                "</div><!--made-by-->\n",
                COPYRIGHTYEAR);
    }
    else if (startswith((char*)token, "include"))  /* {include} */
    {
        process_include(token, output, state, vars, vars_count,
                macros, macros_count, links, links_count,
                read_yaml_macros_and_links);
        *skip_eol = TRUE;
    }
    else if (startswith((char*)token, "incdir"))   /* {incdir} */
    {
        process_incdir(token, output, vars, vars_count, macros, macros_count,
                pmacros, links, links_count, state,
                read_yaml_macros_and_links);
        *skip_eol = TRUE;
    }
    else if (*token == '=')   /* {=macro} */
    {
        process_macro(token, output, macros, macros_count, pmacros, state,
                read_yaml_macros_and_links, end_tag);
        *skip_eol = TRUE;
    }
    else if (!read_yaml_macros_and_links)   /* general tags */
    {
        fprintf(output, "<");
        if (end_tag)
            fprintf(output, "/");

        if (*token == '.' || *token == '#')
        {
            fprintf(output, "div");
        }

        while (*token && *token != '#' && *token != '.')
            fprintf(output, "%c", *token++);

        if (!end_tag)
        {
            if (*token == '#')
            {
                token++;
                fprintf(output, " id=\"");
                while (*token && *token != '.')
                    fprintf(output, "%c", *token++);
                fprintf(output, "\"");
                if (*token == '.')
                {
                    token++;
                    fprintf(output, " class=\"");
                    while (*token)
                        fprintf(output, "%c", *token++);
                    fprintf(output, "\"");
                }
            }
            else if (*token == '.')
            {
                token++;
                fprintf(output, " class=\"");
                while (*token && *token != '#')
                    fprintf(output, "%c", *token++);
                fprintf(output, "\"");
                if (*token == '#')
                {
                    token++;
                    fprintf(output, " id=\"");
                    while (*token)
                        fprintf(output, "%c", *token++);
                    fprintf(output, "\"");
                }
            }
        }
        fprintf(output, ">");
    }

    return 0;
}

int
process_bold(FILE* output, BOOL end_tag)
{
    fprintf(output, "<%sstrong>", end_tag ? "/" : "");
    return 0;
}

int
process_italic(FILE* output, BOOL end_tag)
{
    fprintf(output, "<%sem>", end_tag ? "/" : "");
    return 0;
}

int
process_code(FILE* output, BOOL end_tag)
{
    fprintf(output, "<%scode>", end_tag ? "/" : "");
    return 0;
}

int
process_blockquote(FILE* output, BOOL end_tag)
{
    fprintf(output, "<%sblockquote>", end_tag ? "/" : "");
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
    strcpy(*realpath, ".");
    u8_snprintf(command, BUFSIZE, "realpath --relative-to=%s %s", 
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
                *eol = '\0';

            strcpy(*realpath, (char*)cmd_output_line);
        }
        pclose(cmd_output);
        free(cmd_output_line);
    }
    else
        warning(1, (uint8_t*)"get_realpath: Cannot popen\n");

    free(command);

    return 0;
}

int
process_inline_link(uint8_t* link_text, uint8_t* link_macro_body, 
        uint8_t* link_url, FILE* output)
{
    fprintf(output, "<a href=\"%s\">%s%s</a>", 
            link_url ? (char*)link_url : "", 
            link_macro_body ? (char*)link_macro_body : "",
            link_text);

    return 0;
}

int
process_link(uint8_t* link_text, uint8_t* link_macro_body, uint8_t* link_id, 
        KeyValue* links, size_t links_count, FILE* output)
{
    uint8_t* url = get_value(links, links_count, link_id, NULL);
    return process_inline_link(link_text, link_macro_body,
            url, output);
}

int
process_inline_image(uint8_t* image_text, uint8_t* image_url, FILE* output)
{
    fprintf(output, "<img src=\"%s\" alt=\"%s\" />",
            image_url ? (char*)image_url : "", 
            image_text);

    return 0;
}

int
process_image(uint8_t* image_text, uint8_t* image_id, KeyValue* links, 
        size_t links_count, FILE* output)
{
    uint8_t* url = get_value(links, links_count, image_id, NULL);
    return process_inline_image(image_text, url, output);
}

int
process_line_start(ULONG* state, BOOL first_line_in_doc,
        BOOL previous_line_blank, BOOL processed_start_of_line,
        BOOL read_yaml_macros_and_links, FILE* output,
        uint8_t** token,
        uint8_t** ptoken)
{
    if ((first_line_in_doc || previous_line_blank)
            && !processed_start_of_line
            && !(*state & (ST_PRE | ST_BLOCKQUOTE)))
    {
        if (!read_yaml_macros_and_links)
            fprintf(output, "<p>");
        *state |= ST_PARA_OPEN;
    }
    return 0;
}

int
process_text_token(ULONG* state, 
        BOOL first_line_in_doc,
        BOOL previous_line_blank,
        BOOL processed_start_of_line,
        BOOL read_yaml_macros_and_links,
        FILE* output,
        uint8_t** token,
        uint8_t** ptoken,
        BOOL add_enclosing_paragraph)
{
    if (!read_yaml_macros_and_links && !(*state & ST_YAML))
    {
        if (add_enclosing_paragraph)
            process_line_start(state, first_line_in_doc, previous_line_blank,
                    processed_start_of_line, read_yaml_macros_and_links,
                    output, token, ptoken);
        **ptoken = '\0';
        if (**token && !(*state & ST_MACRO_BODY))
            fprintf(output, "%s", *token);
    }
    **token = '\0';
    *ptoken = *token;
    return 0;
}

int
begin_html_and_head(FILE* output, KeyValue* vars, size_t vars_count)
{
    uint8_t* site_name = get_value(vars, vars_count, (uint8_t*)"site-name", NULL);
    uint8_t* site_desc = get_value(vars, vars_count, (uint8_t*)"site-desc", NULL);
    uint8_t* favicon_url = get_value(vars, vars_count, (uint8_t*)"favicon-url", NULL);

    fprintf(output, "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<title>%s</title>\n"
            "<meta charset=\"utf8\" />\n",
            site_name ? (char*)site_name : "");

    char* favicon = NULL;
    CALLOC(favicon, char, BUFSIZE)
    sprintf(favicon, "%s/favicon.ico", basedir);
    if (!access(favicon, R_OK))
        fprintf(output, "<link rel=\"shortcut icon\" type=\"image/x-icon\""
                " href=\"%s\" />\n", 
                favicon_url ? (char*)favicon_url : "/favicon.ico");
    free(favicon);
    
    if (site_desc && *site_desc)
        fprintf(output, "<meta name=\"description\" content=\"%s\" />\n",
                (char*)site_desc);

    fprintf(output, "<meta name=\"viewport\" content=\"width=device-width,"
            " initial-scale=1\" />\n<meta name=\"generator\" content=\"slweb\" />\n");
    return 0;
}

int
add_css(FILE* output, KeyValue* vars, size_t vars_count)
{
    KeyValue* pvars = vars;
    while (pvars < vars + vars_count)
    {
        if (!u8_strcmp(pvars->key, (uint8_t*)"stylesheet"))
            fprintf(output, "<link rel=\"stylesheet\" href=\"%s\" />\n",
                    pvars->value);
        pvars++;
    }
    return 0;
}

int
end_head_start_body(FILE* output, KeyValue* vars, size_t vars_count)
{
    fprintf(output, "</head>\n<body>\n");

    return 0;
}

int
end_body_and_html(FILE* output)
{
    fprintf(output, "</body>\n</html>\n");
    return 0;
}

int
slweb_parse(uint8_t* buffer, FILE* output, 
        KeyValue** vars, size_t* vars_count, 
        KeyValue** macros, size_t* macros_count,
        KeyValue** links, size_t* links_count,
        BOOL body_only, BOOL read_yaml_macros_and_links)
{
    uint8_t* title                     = NULL;
    uint8_t* title_heading_level       = NULL;
    uint8_t* date                      = NULL;
    uint8_t* permalink_url             = NULL;
    uint8_t* ext_in_permalink          = NULL;
    uint8_t* pbuffer                   = NULL;
    uint8_t* line                      = NULL;
    uint8_t* pline                     = NULL;
    size_t line_len                    = 0;
    uint8_t* token                     = NULL;
    uint8_t* ptoken                    = NULL;
    uint8_t* link_text                 = NULL;
    uint8_t* link_macro                = NULL;
    KeyValue* pvars                    = NULL;
    KeyValue* plinks                   = NULL;
    KeyValue* pmacros                  = NULL;
    ULONG state                        = ST_NONE;
    UBYTE heading_level                = 0;
    BOOL end_tag                       = FALSE;
    BOOL first_line_in_doc             = TRUE;
    BOOL skip_change_first_line_in_doc = FALSE;
    BOOL skip_eol                      = FALSE;
    BOOL previous_line_blank           = FALSE;
    BOOL processed_start_of_line       = FALSE;

    if (!buffer)
        exit(error(1, (uint8_t*)"Empty buffer\n"));

    if (!vars || !*vars)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (vars)\n"));

    if (!links || !*links)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (links)\n"));

    if (!macros || !*macros)
        exit(error(EINVAL, (uint8_t*)"Invalid argument (macros)\n"));

    title = get_value(*vars, *vars_count, (uint8_t*)"title", NULL);
    title_heading_level = get_value(*vars, *vars_count,
            (uint8_t*)"title-heading-level", NULL);
    date = get_value(*vars, *vars_count, (uint8_t*)"date", NULL);
    permalink_url = get_value(*vars, *vars_count, (uint8_t*)"permalink-url", NULL);
    ext_in_permalink = get_value(*vars, *vars_count, (uint8_t*)"ext-in-permalink", NULL);

    CALLOC(line, uint8_t, BUFSIZE)
    CALLOC(token, uint8_t, BUFSIZE)
    CALLOC(link_macro, uint8_t, BUFSIZE)

    pbuffer = buffer;
    pvars = *vars;
    plinks = *links;
    pmacros = *macros;
    lineno = 0;

    if (!read_yaml_macros_and_links && !body_only)
    {
        begin_html_and_head(output, *vars, *vars_count);
        add_css(output, *vars, *vars_count);
        end_head_start_body(output, *vars, *vars_count);
    }

    if (title)
        fprintf(output, "<h%s>%s</h%s>\n", 
                title_heading_level ? (char*)title_heading_level : "2", 
                (char*)title, 
                title_heading_level ? (char*)title_heading_level : "2");

    if (date && input_filename)
    {
        char* link = strip_ext(input_filename);
        uint8_t* samedir_permalink = get_value(*vars, *vars_count, 
                (uint8_t*)"samedir-permalink", NULL);
        char* real_link = NULL;
        uint8_t* permalink_macro = get_value(*macros, *macros_count,
                (uint8_t*)"permalink", NULL);
        CALLOC(real_link, char, BUFSIZE)

        if (ext_in_permalink && *ext_in_permalink != (ucs4_t)'0')
            strncat(link, timestamp_output_ext, strlen(timestamp_output_ext));

        if (permalink_url)
            process_timestamp(output, (char*)permalink_url, permalink_macro, date);
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

    do
    {
        uint8_t* eol = u8_strchr(pbuffer, (ucs4_t)'\n');
        if (!eol)
            break;

        pline = line;
        while (pbuffer != eol)
            *pline++ = *pbuffer++;
        pbuffer++;
        *pline = '\0';
        pline = line;
        line_len = u8_strlen(line);
        *token = '\0';
        ptoken = token;

        lineno++;
        colno = 1;
        processed_start_of_line = FALSE;
        skip_eol = FALSE;

        while (pline && *pline)
        {
            switch (*pline)
            {
            case '-':
                if (colno == 1 
                        && !(state & (ST_MACRO_BODY | ST_PRE))
                        && u8_strlen(pline) > 2
                        && !strcmp(substr((char*)pline, 0, 3), "---"))
                {
                    state ^= ST_YAML;

                    skip_change_first_line_in_doc = TRUE;
                    skip_eol = TRUE;

                    pline += 3;
                    colno += 3;
                }
                else
                {
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case ':':
                if (state & ST_YAML
                        && !(state & (ST_MACRO_BODY | ST_YAML_VAL))
                        && read_yaml_macros_and_links)
                {
                    *ptoken = '\0';

                    (*vars_count)++;

                    if (*vars_count > 1)
                    {
                        REALLOC(*vars, KeyValue,
                                *vars_count * sizeof(KeyValue))
                        pvars = *vars + *vars_count - 1;
                    }
                    CALLOC(pvars->key, uint8_t, u8_strlen(token)+1)
                    u8_strcpy(pvars->key, token);
                    pvars->value = NULL;

                    state |= ST_YAML_VAL;
                    *token = '\0';
                    ptoken = token;
                    pline++;
                    colno++;

                    while (pline && (*pline == ' ' || *pline == '\t'))
                    {
                        pline++;
                        colno++;
                    }
                }
                else {
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case '`':
                if (state & ST_MACRO_BODY)
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (colno == 1 
                        && u8_strlen(pline) > 2
                        && !strcmp(substr((char*)pline, 0, 3), "```"))
                {
                    state ^= ST_PRE;
                    
                    if (!read_yaml_macros_and_links)
                    {
                        if (state & ST_PRE)
                            fprintf(output, "<pre>");
                        else
                            fprintf(output, "</pre>");
                    }

                    pline += 3;
                    colno += 3;
                }
                else if (!(state & (ST_TAG | ST_HEADING | ST_YAML | ST_PRE)))
                {
                    /* Handle ` within link text specially */
                    if (state & ST_LINK)
                    {
                        uint8_t* tag = state & ST_CODE
                                ? (uint8_t*)"</code>"
                                : (uint8_t*)"<code>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to ` */
                        process_text_token(&state, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                output,
                                &token,
                                &ptoken,
                                TRUE);
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
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case '#':
                if (state & ST_MACRO_BODY)
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (colno == 1
                        && !(state & (ST_PRE | ST_YAML)))
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
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case '_':
                if (read_yaml_macros_and_links
                        || state & (ST_MACRO_BODY | ST_TAG | ST_PRE 
                            | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '_')
                {
                    /* Handle __ within link text specially */
                    if (state & ST_LINK)
                    {
                        uint8_t* tag = state & ST_BOLD
                                ? (uint8_t*)"</strong>"
                                : (uint8_t*)"<strong>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to __ */
                        process_text_token(&state, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                output,
                                &token,
                                &ptoken,
                                TRUE);
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
                    /* Handle _ within link text specially */
                    if (state & ST_LINK)
                    {
                        uint8_t* tag = state & ST_ITALIC
                                ? (uint8_t*)"</em>"
                                : (uint8_t*)"<em>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to _ */
                        process_text_token(&state, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                output,
                                &token,
                                &ptoken,
                                TRUE);
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
                if (read_yaml_macros_and_links
                        || state & (ST_MACRO_BODY | ST_TAG | ST_PRE 
                            | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '*')
                {
                    /* Handle ** within link text specially */
                    if (state & ST_LINK)
                    {
                        uint8_t* tag = state & ST_BOLD
                                ? (uint8_t*)"</strong>"
                                : (uint8_t*)"<strong>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to * */
                        process_text_token(&state, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                output,
                                &token,
                                &ptoken,
                                TRUE);
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
                    /* Handle * within link text specially */
                    if (state & ST_LINK)
                    {
                        uint8_t* tag = state & ST_ITALIC
                                ? (uint8_t*)"</em>"
                                : (uint8_t*)"<em>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                    }
                    else
                    {
                        /* Output existing text up to * */
                        process_text_token(&state, first_line_in_doc,
                                previous_line_blank,
                                processed_start_of_line,
                                read_yaml_macros_and_links,
                                output,
                                &token,
                                &ptoken,
                                TRUE);
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
                if (state & ST_MACRO_BODY)
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (u8_strlen(pline) == 2
                        && *(pline+1) == ' ')
                {
                    *ptoken = '\0';
                    u8_strncat(ptoken, (uint8_t*)"<br />", strlen("<br />"));
                    ptoken += strlen("<br />");
                    pline++;
                    colno++;
                }
                else if (state & ST_LINK_MACRO)
                {
                    *ptoken = '\0';
                    u8_strcpy(link_macro, token);
                    *token = '\0';
                    ptoken = token;
                    state &= ~ST_LINK_MACRO;
                }
                else if (!(state & ST_HEADING
                        && *(pline-1) == '#'))
                    *ptoken++ = *pline;
                pline++;
                colno++;
                break;

            case '{':
                if (state & (ST_PRE | ST_CODE | ST_YAML | ST_YAML_VAL 
                            | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (state & ST_MACRO_BODY)
                {
                    *ptoken = '\0';
                    size_t token_len = u8_strlen(token);

                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pmacros->value)
                        {
                            size_t value_len = u8_strlen(pmacros->value);

                            if (sizeof(pmacros->value) < value_len + token_len + 1)
                            {
                                REALLOC(pmacros->value, uint8_t,
                                        sizeof(pmacros->value) 
                                        + BUFSIZE * sizeof(uint8_t))
                            }
                            u8_strncat(pmacros->value, token, token_len);
                        }
                        else
                        {
                            CALLOC(pmacros->value, uint8_t, BUFSIZE)
                            u8_strcpy(pmacros->value, token);
                        }
                    }
                }
                else
                {
                    /* Output existing text up to { */
                    process_text_token(&state, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            output,
                            &token,
                            &ptoken,
                            FALSE);
                    processed_start_of_line = TRUE;
                }

                state |= ST_TAG;
                *token = '\0';
                ptoken = token;

                pline++;
                colno++;
                break;

            case '/':
                if (state & (ST_PRE | ST_CODE | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if ((state & ST_TAG) && pline-1 && *(pline-1) == '{')
                    end_tag = TRUE;
                else
                    *ptoken++ = *pline;

                pline++;
                colno++;
                break;

            case '}':
                if (state & (ST_PRE | ST_CODE | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                state &= ~ST_TAG;
                *ptoken = '\0';

                process_tag(token, output, macros, macros_count,
                        &pmacros, vars, vars_count, links, links_count,
                        &state, read_yaml_macros_and_links, &skip_eol, end_tag);

                *token = '\0';
                ptoken = token;
                end_tag = FALSE;

                pline++;
                colno++;
                break;

            case '>':
                if (state & ST_MACRO_BODY)
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (!read_yaml_macros_and_links
                        && !(state & (ST_PRE | ST_HEADING))
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
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case '!':
                if (state & (ST_MACRO_BODY | ST_PRE | ST_HEADING | ST_CODE
                            | ST_LINK))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                /* Output existing text up to ! */
                *ptoken = '\0';
                if (!read_yaml_macros_and_links)
                    fprintf(output, "%s", token);
                *token = '\0';
                ptoken = token;

                if (u8_strlen(pline) > 1 && *(pline+1) == '[')
                {
                    state |= ST_IMAGE;
                    pline += 2;
                    colno += 2;
                }
                else 
                {
                    *ptoken++ = *pline++;
                    colno++;
                }

                break;

            case '=':
                if (state & (ST_MACRO_BODY | ST_PRE | ST_CODE | ST_HEADING
                            | ST_TAG))
                {
                    *ptoken++ = *pline++;
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
                    *ptoken++ = *pline++;
                    colno++;
                }
                break;

            case '[':
                if (state & (ST_MACRO_BODY | ST_PRE | ST_HEADING | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (*token)
                    /* Output existing text up to [ */
                    process_text_token(&state, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            output,
                            &token,
                            &ptoken,
                            TRUE);
                processed_start_of_line = TRUE;

                *token = '\0';
                ptoken = token;

                state |= ST_LINK;
                *link_macro = '\0';

                pline++;
                colno++;
                break;

            case '(':
                if (state & (ST_MACRO_BODY | ST_PRE | ST_CODE | ST_HEADING))
                    *ptoken++ = *pline;
                else if (state & ST_LINK)
                {
                    uint8_t* tag = (uint8_t*)"<span>";
                    size_t tag_len = u8_strlen(tag);
                    size_t token_len = 0;

                    *ptoken = '\0';
                    token_len = u8_strlen(token);

                    if (token_len + tag_len < BUFSIZE)
                    {
                        u8_strncat(token, tag, tag_len);
                        ptoken += tag_len;
                    }

                    state |= ST_LINK_SPAN;
                }
                else
                    *ptoken++ = *pline;
                pline++;
                colno++;
                break;

            case ')':
                if (state & ST_LINK_SPAN)
                {
                    if (u8_strlen(pline) > 1
                            && *(pline+1) == ']')
                    {
                        uint8_t* tag = (uint8_t*)"</span>";
                        size_t tag_len = u8_strlen(tag);
                        size_t token_len = 0;

                        *ptoken = '\0';
                        token_len = u8_strlen(token);

                        if (token_len + tag_len < BUFSIZE)
                        {
                            u8_strncat(token, tag, tag_len);
                            ptoken += tag_len;
                        }
                        state &= ~ST_LINK_SPAN;
                    }
                }
                else if (state & (ST_LINK_SECOND_ARG | ST_IMAGE_SECOND_ARG))
                {
                    *ptoken = '\0';
                    if (!read_yaml_macros_and_links)
                    {
                        if (state & ST_LINK_SECOND_ARG)
                            process_inline_link(link_text, 
                                    get_value(*macros, *macros_count, 
                                        link_macro, NULL), 
                                    token, output);
                        else
                            process_inline_image(link_text, token, output);
                    }
                    *token = '\0';
                    ptoken = token;
                    state &= ~(ST_LINK | ST_LINK_SECOND_ARG 
                            | ST_IMAGE | ST_IMAGE_SECOND_ARG);
                }
                else
                    *ptoken++ = *pline;
                pline++;
                colno++;
                break;

            case ']':
                if (state & (ST_MACRO_BODY | ST_PRE | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                *ptoken = '\0';
                if (state & ST_LINK_SECOND_ARG)
                {
                    if (!read_yaml_macros_and_links)
                        process_link(link_text, 
                                get_value(*macros, *macros_count, 
                                    link_macro, NULL), 
                                token, *links, *links_count, output);
                    *token = '\0';
                    ptoken = token;
                    state &= ~(ST_LINK | ST_LINK_SECOND_ARG);
                    pline++;
                    colno++;
                }
                else if (state & ST_IMAGE_SECOND_ARG)
                {
                    if (!read_yaml_macros_and_links)
                        process_image(link_text, token, *links, *links_count, 
                                output);
                    *token = '\0';
                    ptoken = token;
                    state &= ~(ST_IMAGE | ST_IMAGE_SECOND_ARG);
                    pline++;
                    colno++;
                }
                else if (u8_strlen(pline) > 1)
                {
                    switch (*(pline+1))
                    {
                        case ':':
                            (*links_count)++;

                            if (*links_count > 1)
                            {
                                REALLOC(*links, KeyValue,
                                        *links_count * sizeof(KeyValue))
                                plinks = *links + *links_count - 1;
                            }
                            CALLOC(plinks->key, uint8_t, u8_strlen(token)+1)
                            u8_strcpy(plinks->key, token);
                            plinks->value = NULL;
                            pline += 2;
                            colno += 2;
                            while (pline && (*pline == ' ' || *pline == '\t'))
                            {
                                pline++;
                                colno++;
                            }
                            *token = '\0';
                            ptoken = token;
                            state |= ST_LINK_SECOND_ARG;
                            break;

                        case '[':
                        case '(':
                            if (link_text)
                            {
                                size_t token_len = u8_strlen(token);
                                if (token_len > u8_strlen(link_text))
                                    REALLOC(link_text, uint8_t, 
                                            token_len * sizeof(uint8_t))
                            }
                            else
                                CALLOC(link_text, uint8_t, u8_strlen(token)+1)
                            u8_strcpy(link_text, token);
                            pline += 2;
                            colno += 2;
                            *token = '\0';
                            ptoken = token;
                            if (state & ST_LINK)
                                state |= ST_LINK_SECOND_ARG;
                            else
                                state |= ST_IMAGE_SECOND_ARG;
                            break;
                        default:
                            *ptoken++ = *pline++;
                            colno++;
                            state &= ~(ST_LINK | ST_IMAGE);
                            break;
                    }
                }
                break;

            case '\\':
                if (state & ST_MACRO_BODY)
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                /* TODO: Cases? */
                pline++;
                colno++;
                break;

            default:
                *ptoken++ = *pline++;
                colno++;
            }
        }

        if (*token && read_yaml_macros_and_links
                && (state & ST_YAML_VAL))
        {
            *ptoken = '\0';
            CALLOC(pvars->value, uint8_t, u8_strlen(token)+1)
            u8_strcpy(pvars->value, token);
        }
        else 
        {
            if (*token)
            {
                if (state & ST_MACRO_BODY)
                {
                    *ptoken = '\0';
                    size_t token_len = u8_strlen(token);

                    skip_eol = TRUE;
                    if (read_yaml_macros_and_links)
                    {
                        if (pmacros->value)
                        {
                            size_t value_len = u8_strlen(pmacros->value);

                            if (sizeof(pmacros->value) < value_len + token_len + 1)
                            {
                                REALLOC(pmacros->value, uint8_t,
                                        sizeof(pmacros->value) 
                                        + BUFSIZE * sizeof(uint8_t))
                            }
                            u8_strncat(pmacros->value, token, token_len);
                            u8_strncat(pmacros->value, (uint8_t*)"\n", 1);
                        }
                        else
                        {
                            CALLOC(pmacros->value, uint8_t, BUFSIZE)
                            u8_strcpy(pmacros->value, token);
                            u8_strncat(pmacros->value, (uint8_t*)"\n", 1);
                        }
                    }
                }
                else if (state & ST_LINK_SECOND_ARG)
                {
                    *ptoken = '\0';
                    if (read_yaml_macros_and_links)
                    {
                        CALLOC(plinks->value, uint8_t, u8_strlen(token)+1)
                        u8_strcpy(plinks->value, token);
                    }
                }
                else if (state & ST_HEADING)
                {
                    state &= ~ST_HEADING;
                    *ptoken = '\0';
                    if (!read_yaml_macros_and_links)
                        process_heading(token, output, heading_level);
                    first_line_in_doc = FALSE;
                    *token = '\0';
                    ptoken = token;
                    heading_level = 0;
                }
                else 
                    process_text_token(&state, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            output,
                            &token,
                            &ptoken,
                            TRUE);
            }

            if ((state & ST_PARA_OPEN)
                        && !(state & (ST_PRE | ST_LINK_SECOND_ARG))
                        && (!*pbuffer || *pbuffer == '\n'))
            {
                if (!read_yaml_macros_and_links)
                    fprintf(output, "</p>");
                state &= ~ST_PARA_OPEN;
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
                fprintf(output, "\n");

        *token = '\0';
        ptoken = token;

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

    if (!read_yaml_macros_and_links && !body_only)
        end_body_and_html(output);

    free(line);
    free(token);

    return 0;
}

int
main(int argc, char** argv)
{
    char* arg;
    Command cmd = CMD_NONE;
    BOOL body_only = FALSE;

    CALLOC(basedir, char, 2)
    *basedir = '.';

    arg_zero = *argv;

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
                else if (!strcmp(substr(arg, 0, strlen("basedir")), 
                            "basedir"))
                {
                    arg += strlen("basedir");
                    result = set_basedir(arg, &basedir);
                    if (result)
                        return result;
                }
                else if (!strcmp(arg, "help"))
                    return usage();
                else
                {
                    error(EINVAL, (uint8_t*)"Invalid argument: --%s\n", arg);
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
                    error(EINVAL, (uint8_t*)"Invalid argument: -%c\n", c);
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
                input_filename = arg;
            cmd = CMD_NONE;
        }
    }

    if (cmd == CMD_BASEDIR)
        return error(1, (uint8_t*)"-d: Argument required\n");

    if (cmd == CMD_VERSION)
        return version();

    FILE* input = NULL;
    FILE* output = stdout;
    uint8_t* buffer = NULL;

    if (input_filename)
        read_file_into_buffer(&buffer, input_filename, &input_dirname, &input);
    else
    {
        uint8_t* bufline = NULL;
        size_t buffer_len = 0;
        size_t bufline_len = 0;

        input = stdin;

        CALLOC(bufline, uint8_t, BUFSIZE)
        CALLOC(buffer, uint8_t, BUFSIZE)

        while (!feof(input))
        {
            uint8_t* eol = NULL;
            if (!fgets((char*)bufline, BUFSIZE-1, input))
                break;
            eol = u8_strchr(bufline, (ucs4_t)'\n');
            if (eol)
                *(eol+1) = '\0';
            bufline_len = u8_strlen(bufline);
            if (buffer_len + bufline_len + 1 > sizeof(buffer))
                REALLOC(buffer, uint8_t, sizeof(buffer) + BUFSIZE)
            u8_strncat(buffer, bufline, bufline_len);
            buffer_len += bufline_len;
        }
        free(bufline);
    }

    KeyValue* vars = NULL;
    size_t vars_count = 0;
    KeyValue* macros = NULL;
    size_t macros_count = 0;
    KeyValue* links = NULL;
    size_t links_count = 0;

    CALLOC(vars, KeyValue, 1)
    vars->key = NULL;
    vars->value = NULL;

    CALLOC(macros, KeyValue, 1)
    macros->key = NULL;
    macros->value = NULL;

    CALLOC(links, KeyValue, 1)
    links->key = NULL;
    links->value = NULL;

    /* First pass: read YAML, macros and links */
    slweb_parse(buffer, output, &vars, &vars_count, &macros, &macros_count,
            &links, &links_count,
            body_only, TRUE);

    /* Second pass: parse and output */
    return slweb_parse(buffer, output, &vars, &vars_count, 
            &macros, &macros_count, &links, &links_count,
            body_only, FALSE);
}
 
