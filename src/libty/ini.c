/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "ini.h"

#define MAX_LINE_SIZE 1024
#define MAX_SECTION_SIZE 256

static int ini_parse_error(const char *path, unsigned int line_number, const char *expected)
{
    return ty_error(TY_ERROR_PARSE, "Parse error (INI) on line %u in '%s', expected %s",
                    line_number, path, expected);
}

int ty_ini_walk_fp(FILE *fp, const char *filename, ty_ini_callback_func *f, void *udata)
{
    assert(fp);
    assert(f);

    if (!filename)
        filename = "?";

    unsigned int line_number = 1;
    char line_buf[MAX_LINE_SIZE];
    char section_buf[MAX_SECTION_SIZE] = "";

    for (;;) {
        char *line;
        size_t line_len;

        line = fgets(line_buf, sizeof(line_buf), fp);
        if (!line) {
            if (ferror(fp)) {
                if (errno == EIO)
                    return ty_error(TY_ERROR_IO, "I/O error while reading from '%s'", filename);
                return ty_error(TY_ERROR_SYSTEM, "fgets('%s') failed: %s", filename,
                                strerror(errno));
            } else {
                return 0;
            }
        }
        line += strspn(line, " \t");
        line_len = strlen(line);

        // Skip remaining line characters if longer than fgets() buffer
        if (!feof(fp) && line_len && line[line_len - 1] != '\n') {
            ty_log(TY_LOG_WARNING, "Line %u in '%s' truncated to %zu characters",
                   line_number, filename, sizeof(line_buf) - 1);
            int c;
            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n');
        }

        if (strchr("#;\r\n", line[0])) {
            // Ignore this line (empty or comment)
        } else if (line[0] == '[') {
            char *section, *section_end;

            section = line + 1;
            section += strspn(section, " \t");
            section_end = strchr(section, ']');
            if (!section_end || section_end == section ||
                    (section_end + 1)[strspn(section_end + 1, " \t\r\n")])
                return ini_parse_error(filename, line_number, "[section]");
            while (section_end > section && strchr(" \t", section_end[-1]))
                section_end--;
            section_end[0] = 0;

            strncpy(section_buf, section, sizeof(section_buf));
            section_buf[sizeof(section_buf) - 1] = 0;
        } else {
            char *key, *key_end;
            char *value, *value_end;
            int r;

            key = line;
            key_end = strchr(key, '=');
            if (!key_end || key == key_end)
                return ini_parse_error(filename, line_number, "key = value");
            value = key_end + 1;
            value += strspn(value, " \t");
            value_end = line + line_len;
            while (key_end > key && strchr(" \t", key_end[-1]))
                key_end--;
            while (value_end > value && strchr(" \t\r\n", value_end[-1]))
                value_end--;
            key_end[0] = 0;
            value_end[0] = 0;

            r = (*f)(section_buf[0] ? section_buf : NULL, key, value, udata);
            if (r)
                return r;
        }

        line_number++;
    }

    return 0;
}

int ty_ini_walk(const char *filename, ty_ini_callback_func *f, void *udata)
{
    assert(filename);
    assert(f);

    FILE *fp;
    int r;

#ifdef _WIN32
    fp = fopen(filename, "rb");
#else
    fp = fopen(filename, "rbe");
#endif
    if (!fp) {
        switch (errno) {
            case EACCES: {
                return ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", filename);
            } break;
            case EIO: {
                return ty_error(TY_ERROR_IO, "I/O error while opening '%s' for reading", filename);
            } break;
            case ENOENT:
            case ENOTDIR: {
                return ty_error(TY_ERROR_NOT_FOUND, "File '%s' does not exist", filename);
            } break;

            default: {
                return ty_error(TY_ERROR_SYSTEM, "fopen('%s') failed: %s", filename,
                                strerror(errno));
            } break;
        }
    }
    r = ty_ini_walk_fp(fp, filename, f, udata);
    fclose(fp);

    return r;
}
