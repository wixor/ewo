#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdio>

struct config_var
{
    const char *name;
    enum { INT, FLOAT, STRING, CALLBACK, BOOL, NONE } type;
    void *data;
};

void parse_config(const char *filename, const struct config_var *vars);

struct linereader
{
    FILE *f;
    char *buffer;
    size_t buffer_len;
    ssize_t line_len;

    linereader(const char *filename);
    ~linereader();
    bool getline();
};

#endif
