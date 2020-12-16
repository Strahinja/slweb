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
#include <stdint.h>
#include <unistdio.h>

static size_t lineno = 0;
static size_t colno = 1;
static uint8_t* filename = NULL;

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
    fprintf(stderr, "%s: %s", PROGRAMNAME, buf);
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
    if (!result)
        exit(error(ENOMEM, (uint8_t*)"Memory allocation failed (out of memory?)\n"));
    char* presult = result;

    for (int i = start; i < finish && *(src+i) != '\0'; i++)
        *presult++ = *(src+i);
    *presult = '\0';

    return result;
}

uint8_t*
get_value(KeyValue* list, size_t list_count, uint8_t* key)
{
    KeyValue* plist = list;
    while (plist < list + list_count)
    {
        if (!u8_strcmp(plist->key, key))
            return plist->value;
        plist++;
    }
    return NULL;
}

int
set_basedir(char* arg, char** basedir)
{
    if (*basedir)
        free(*basedir);

    if (strlen(arg) < 1)
        return error(1, (uint8_t*)"--basedir: Argument required\n");

    *basedir = (char*) calloc(strlen(arg)+1, sizeof(char));

    if (!*basedir) 
        return error(ENOMEM, (uint8_t*)"Memory allocation failed (out of memory?)\n");

    strcpy(*basedir, arg);

    return 0;
}

int
finish_and_print_token(uint8_t** token, uint8_t** ptoken, FILE* output)
{
    if (!token || !*token || !ptoken || !*ptoken)
        return 1;

    **ptoken = '\0';
    fprintf(output, "%s", *token);

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
process_tag(uint8_t* token, FILE* output, BOOL end_tag)
{
    if (!token || u8_strlen(token) < 1)
        return warning(1, (uint8_t*)"No tag\n");

    if (!strcmp((char*)token, "git-log"))
    {
        if (!filename)
            return warning(1, (uint8_t*)"Cannot use git-log in stdin\n");

        uint8_t* basename = (uint8_t*) calloc(u8_strlen(filename), 
                sizeof(uint8_t));
        uint8_t* slash = u8_strrchr(filename, '/');
        if (slash)
            u8_strncpy(basename, slash+1, u8_strlen(slash+1));
        else
            u8_strncpy(basename, filename, u8_strlen(filename));

        fprintf(output, "<div id=\"git-log\">\nPrevious commit:\n");
        uint8_t* command = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
        u8_snprintf(command, BUFSIZE,
                "git log -1 --pretty=format:\"%s %%h %%ci (%%cn) %%d\""
                " || echo \"(Not in a Git repository)\"",
            basename);
        FILE* cmd_output = popen((char*)command, "r");
        if (!cmd_output)
        {
            perror(PROGRAMNAME);
            return error(1, (uint8_t*)"Cannot popen\n");
        }
        uint8_t* cmd_output_line = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
        while (!feof(cmd_output))
        {
            if (!fgets((char*)cmd_output_line, BUFSIZE, cmd_output))
                continue;

            char* eol = strchr((char*)cmd_output_line, '\n');
            if (eol)
                *eol = '\0';

            fprintf(output, "%s\n", cmd_output_line);

        }
        pclose(cmd_output);
        fprintf(output, "</div><!--git-log-->\n");

        free(command);
        free(cmd_output_line);
        free(basename);

    }
    else if (!strcmp((char*)token, "made-by"))
    {
        fprintf(output, "<div id=\"made-by\">\n"
                "Generated by <a href=\"https://github.com/Strahinja/slweb\">"
                "slweb</a>\n"
                "© 2020 Strahinya Radich.\n"
                "</div><!--made-by-->\n");
    }
    else
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
process_blockquote(FILE* output, BOOL end_tag)
{
    fprintf(output, "<%sblockquote>", end_tag ? "/" : "");
    return 0;
}

int
process_link(uint8_t* link_text, uint8_t* link_id, KeyValue* links, 
        size_t links_count, FILE* output)
{
    uint8_t* url = get_value(links, links_count, link_id);
    fprintf(output, "<a href=\"%s\">%s</a>", 
            url ? (char*)url : "", link_text);
    return 0;
}

int
process_inline_link(uint8_t* link_text, uint8_t* link_url, FILE* output)
{
    fprintf(output, "<a href=\"%s\">%s</a>", 
            link_url, link_text);
    return 0;
}

int
process_image(uint8_t* image_text, uint8_t* image_id, KeyValue* links, 
        size_t links_count, FILE* output)
{
    uint8_t* url = get_value(links, links_count, image_id);
    fprintf(output, "<img src=\"%s\" alt=\"%s\" />", 
            url ? (char*)url : "", image_text);
    return 0;
}

int
process_inline_image(uint8_t* image_text, uint8_t* image_url, FILE* output)
{
    fprintf(output, "<img src=\"%s\" alt=\"%s\" />", 
            image_url, image_text);
    return 0;
}

int
process_line_start(USHORT* state, BOOL first_line_in_doc,
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
process_text_token(USHORT* state, 
        BOOL first_line_in_doc,
        BOOL previous_line_blank,
        BOOL processed_start_of_line,
        BOOL read_yaml_macros_and_links,
        FILE* output,
        uint8_t** token,
        uint8_t** ptoken,
        BOOL add_enclosing_paragraph)
{
    if (add_enclosing_paragraph)
        process_line_start(state, first_line_in_doc, previous_line_blank,
                processed_start_of_line, read_yaml_macros_and_links,
                output, token, ptoken);
    **ptoken = '\0';
    if (!read_yaml_macros_and_links && **token)
        fprintf(output, "%s", *token);
    **token = '\0';
    *ptoken = *token;
    return 0;
}

int
begin_html_and_head(FILE* output, KeyValue* vars, size_t vars_count)
{
    KeyValue* pvars = vars;
    uint8_t* site_name = get_value(vars, vars_count, (uint8_t*)"site-name");
    uint8_t* site_desc = get_value(vars, vars_count, (uint8_t*)"site-desc");

    fprintf(output, "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<title>%s</title>\n"
            "<meta charset=\"utf8\" />\n",
            site_name ? (char*)site_name : "");
    
    if (site_desc && *site_desc)
        fprintf(output, "<meta name=\"description\" content=\"%s\" />\n",
                (char*)site_desc);

    fprintf(output, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
            "<meta name=\"generator\" content=\"slweb\" />\n");
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
end_head_start_body(FILE* output)
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
    uint8_t* pbuffer = NULL;
    uint8_t* line = NULL;
    uint8_t* pline = NULL;
    size_t line_len = 0;
    uint8_t* token = NULL;
    uint8_t* ptoken = NULL;
    uint8_t* link_text = NULL;
    KeyValue* pvars = NULL;
    KeyValue* plinks = NULL;
    USHORT state = ST_NONE;
    UBYTE heading_level = 0;
    BOOL end_tag = FALSE;
    BOOL first_line_in_doc = TRUE;
    BOOL skip_change_first_line_in_doc = FALSE;
    BOOL skip_eol = FALSE;
    BOOL previous_line_blank = FALSE;
    BOOL processed_start_of_line = FALSE;

    if (!buffer)
        return error(1, (uint8_t*)"Empty buffer\n");

    if (!vars || !*vars)
        return error(EINVAL, (uint8_t*)"Invalid argument (vars)\n");

    line = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
    if (!line)
        return error(ENOMEM, 
                (uint8_t*)"Memory allocation failed (out of memory?)\n");

    token = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
    if (!token)
        return error(ENOMEM, 
                (uint8_t*)"Memory allocation failed (out of memory?)\n");

    pbuffer = buffer;
    pvars = *vars;
    plinks = *links;
    lineno = 0;

    if (!read_yaml_macros_and_links)
    {
        begin_html_and_head(output, *vars, *vars_count);
        add_css(output, *vars, *vars_count);
        end_head_start_body(output);
    }

    do
    {
        uint8_t* eol = u8_strchr(pbuffer, (ucs4_t)'\n');
        if (!eol)
            continue;

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
                        && !(state & ST_PRE)
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
                        && !(state & ST_YAML_VAL)
                        && read_yaml_macros_and_links)
                {
                    *ptoken = '\0';

                    (*vars_count)++;

                    if (*vars_count > 1)
                    {
                        *vars = (KeyValue*) realloc(*vars, 
                                *vars_count * sizeof(KeyValue));
                        pvars = *vars + *vars_count - 1;
                    }
                    pvars->key = (uint8_t*) calloc(u8_strlen(token)+1,
                            sizeof(uint8_t));
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
                else if (!(state & (ST_HEADING | ST_YAML | ST_PRE)))
                {
                    state ^= ST_CODE;

                    if (state & ST_CODE)
                    {
                        *ptoken = '\0';
                        u8_strncat(ptoken, (uint8_t*)"<code>", strlen("<code>"));
                        ptoken += strlen("<code>");
                    }
                    else
                    {
                        *ptoken = '\0';
                        u8_strncat(ptoken, (uint8_t*)"</code>", strlen("</code>"));
                        ptoken += strlen("</code>");
                    }

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
                        || state & (ST_TAG | ST_PRE | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '_')
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

                    state ^= ST_BOLD;
                    pline += 2;
                    colno += 2;
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

                    state ^= ST_ITALIC;
                    pline++;
                    colno++;
                }
                break;

            case '*':
                if (read_yaml_macros_and_links
                        || state & (ST_TAG | ST_PRE | ST_CODE))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (u8_strlen(pline) > 1 && *(pline+1) == '*')
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

                    state ^= ST_BOLD;
                    pline += 2;
                    colno += 2;
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

                    state ^= ST_ITALIC;
                    pline++;
                    colno++;
                }
                break;

            case ' ':
                if (u8_strlen(pline) == 2)
                {
                    if (*(pline+1) == ' ')
                    {
                        *ptoken = '\0';
                        u8_strncat(ptoken, (uint8_t*)"<br />", 
                                strlen("<br />"));
                        ptoken += strlen("<br />");
                        pline++;
                        colno++;
                    }
                    else
                        *ptoken++ = *pline;
                }
                else if (!(state & ST_HEADING
                        && *(pline-1) == '#'))
                    *ptoken++ = *pline;
                pline++;
                colno++;
                break;

            case '{':
                if (read_yaml_macros_and_links
                        || state & (ST_PRE | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

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

                state |= ST_TAG;
                *token = '\0';
                ptoken = token;

                pline++;
                colno++;
                break;

            case '/':
                if (read_yaml_macros_and_links
                        || state & (ST_PRE | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                if (state & ST_TAG)
                {
                    if (*(pline-1) != '{')
                        return error(1, (uint8_t*)"Character '/' not allowed here");

                    end_tag = TRUE;
                }
                else
                    *ptoken++ = *pline;

                pline++;
                colno++;
                break;

            case '}':
                if (read_yaml_macros_and_links
                        || state & (ST_PRE | ST_HEADING))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                state &= ~ST_TAG;
                *ptoken = '\0';

                if (!read_yaml_macros_and_links)
                    process_tag(token, output, end_tag);

                *token = '\0';
                ptoken = token;
                end_tag = FALSE;

                pline++;
                colno++;
                break;

            case '>':
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
                if (state & (ST_PRE | ST_HEADING | ST_CODE))
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

            case '[':
                if (state & (ST_PRE | ST_HEADING | ST_CODE))
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

                state |= ST_LINK;

                *token = '\0';
                ptoken = token;

                pline++;
                colno++;
                break;

            case ')':
                if (!(state & (ST_LINK_SECOND_ARG | ST_IMAGE_SECOND_ARG)))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                *ptoken = '\0';
                if (!read_yaml_macros_and_links)
                {
                    if (state & ST_LINK_SECOND_ARG)
                        process_inline_link(link_text, token, output);
                    else
                        process_inline_image(link_text, token, output);
                }
                *token = '\0';
                ptoken = token;
                state &= ~(ST_LINK | ST_LINK_SECOND_ARG 
                        | ST_IMAGE | ST_IMAGE_SECOND_ARG);
                pline++;
                colno++;
                break;

            case ']':
                if (!(state & (ST_LINK | ST_IMAGE)))
                {
                    *ptoken++ = *pline++;
                    colno++;
                    break;
                }

                *ptoken = '\0';
                if (state & ST_LINK_SECOND_ARG)
                {
                    if (!read_yaml_macros_and_links)
                        process_link(link_text, token, *links, *links_count, 
                                output);
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
                                *links = (KeyValue*) realloc(*links,
                                        *links_count * sizeof(KeyValue));
                                plinks = *links + *links_count - 1;
                            }
                            plinks->key = (uint8_t*) calloc(u8_strlen(token),
                                    sizeof(uint8_t));
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
                                link_text = (uint8_t*) realloc(link_text,
                                        u8_strlen(token) * sizeof(uint8_t));
                            else
                                link_text = (uint8_t*) calloc(u8_strlen(token),
                                        sizeof(uint8_t));
                            if (!link_text)
                                return error(ENOMEM, 
                                        (uint8_t*)"Memory allocation failed (out of memory?)\n");
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
                /*
                 *if (state & (ST_PRE | ST_CODE | ST_LINK_SECOND_ARG))
                 *{
                 *    *ptoken++ = *pline++;
                 *    colno++;
                 *    break;
                 *}
                 */

                /* TODO: Cases? */
                pline++;
                colno++;
                break;

            default:
                *ptoken++ = *pline++;
                colno++;
            }
        }

        if (!(state & ST_YAML))
        {
            if (*token)
            {
                if (state & ST_LINK_SECOND_ARG)
                {
                    *ptoken = '\0';
                    plinks->value = (uint8_t*) calloc(u8_strlen(token),
                            sizeof(uint8_t));
                    u8_strcpy(plinks->value, token);
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
                {
                    process_text_token(&state, first_line_in_doc,
                            previous_line_blank,
                            processed_start_of_line,
                            read_yaml_macros_and_links,
                            output,
                            &token,
                            &ptoken,
                            TRUE);
                }
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
        else if (*token && read_yaml_macros_and_links
                && state & ST_YAML_VAL)
        {
            *ptoken = '\0';
            pvars->value = (uint8_t*) calloc(u8_strlen(token)+1, 
                    sizeof(uint8_t));
            u8_strcpy(pvars->value, token);
        }

        if (!skip_eol)
        {
            if (!read_yaml_macros_and_links 
                    && !(state & (ST_YAML | ST_YAML_VAL 
                            | ST_LINK_SECOND_ARG)))
            {
                fprintf(output, "\n");
            }
        }

        *token = '\0';
        ptoken = token;

        if (!skip_change_first_line_in_doc
                && !(state & ST_YAML))
            first_line_in_doc = FALSE;

        skip_change_first_line_in_doc = FALSE; 
            ;
        if (line_len == 0)
            previous_line_blank = TRUE;

        /* Lasts until the end of line */
        state &= ~(ST_YAML_VAL | ST_LINK | ST_LINK_SECOND_ARG);
    }
    while (pbuffer && *pbuffer);

    if (!read_yaml_macros_and_links)
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
    char* basedir = NULL;

    basedir = (char*) calloc(2, sizeof(char));
    if (!basedir)
        return error(ENOMEM, (uint8_t*)"Memory allocation failed (out of memory?)\n");
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
                filename = arg;
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

    if (filename)
    {
        struct stat fs;

        input = fopen(filename, "r");
        if (!input)
            return error(ENOENT, (uint8_t*)"No such file: %s\n", filename);

        fstat(fileno(input), &fs);
        buffer = (uint8_t*) calloc(fs.st_size, sizeof(uint8_t));
        if (!buffer)
            return error(ENOMEM, 
                    (uint8_t*)"Memory allocation failed (out of memory?)\n");

        fread((void*)buffer, sizeof(char), fs.st_size, input);
    }
    else
    {
        uint8_t* bufline = NULL;
        size_t buffer_len = 0;
        size_t bufline_len = 0;

        input = stdin;

        bufline = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
        if (!bufline)
            return error(ENOMEM, 
                    (uint8_t*)"Memory allocation failed (out of memory?)\n");
        buffer = (uint8_t*) calloc(BUFSIZE, sizeof(uint8_t));
        if (!buffer)
            return error(ENOMEM, 
                    (uint8_t*)"Memory allocation failed (out of memory?)\n");

        while (!feof(input))
        {
            uint8_t* eol = NULL;
            if (!fgets((char*)bufline, BUFSIZE-1, input))
                break;
            eol = u8_strchr(bufline, (ucs4_t)'\n');
            if (eol)
                *(eol+1) = '\0';
            bufline_len = u8_strlen(bufline);
            if (buffer_len + bufline_len + 1 > BUFSIZE)
            {
                buffer = (uint8_t*) realloc(buffer, 
                        buffer_len + bufline_len + 1);
            }
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

    vars = (KeyValue*) calloc(1, sizeof(KeyValue));
    if (!vars)
        return error(ENOMEM, 
                (uint8_t*)"Memory allocation failed (out of memory?)\n");
    vars->key = NULL;
    vars->value = NULL;

    macros = (KeyValue*) calloc(1, sizeof(KeyValue));
    if (!macros)
        return error(ENOMEM, 
                (uint8_t*)"Memory allocation failed (out of memory?)\n");
    macros->key = NULL;
    macros->value = NULL;

    links = (KeyValue*) calloc(1, sizeof(KeyValue));
    if (!links)
        return error(ENOMEM, 
                (uint8_t*)"Memory allocation failed (out of memory?)\n");
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
 
