#ifndef __CONFIG_H__
#define __CONFIG_H__

struct config_var
{
    const char *name;
    enum { INT, FLOAT, STRING, CALLBACK, BOOL, NONE } type;
    void *data;
};

void parse_config(const char *filename, const struct config_var *vars);

#endif
