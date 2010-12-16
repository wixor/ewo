#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <stdexcept>

#include "config.h"

linereader::linereader(const char *filename)
{
    buffer = NULL;
    buffer_len = 0;
    f = fopen(filename, "rb");
    if(!f) throw std::runtime_error("failed to open file");
}

linereader::~linereader()
{
    free(buffer);
    if(f) fclose(f);
}

bool linereader::getline()
{
    line_len = ::getline(&buffer, &buffer_len, f);
    if(line_len != -1)
        return true;
    if(feof(f))
        return false;
    throw std::runtime_error("failed to read file");
}

void parse_config(const char *filename, const struct config_var *vars)
{
    linereader lrd(filename);

    while(lrd.getline())
    {
        char *begin, *ptr, *value;

        /* remove comments, trailing newline and whitespace */
        for(ptr = lrd.buffer+lrd.line_len-1; ptr >= lrd.buffer; ptr--)
            if(*ptr == '#' || (isspace(*ptr) && ptr[1] == '\0'))
                *ptr = 0;

        /* remove leading whitespace */
        for(begin = lrd.buffer; isspace(*begin); begin++);

        /* don't bother with empty lines */
        if(*begin == 0)
            continue;

        /* separate key and value ('=') */
        if(!(ptr = strchr(begin, '=')))
            throw std::runtime_error("expected '='"); 

        /* remove leading whitespace from value and trailing whitespace from key */
        value = ptr;
        do value++; while(isspace(*value));
        do ptr--; while(ptr >= begin && isspace(*ptr));
        *++ptr=0;

        /* check if there is Any Key now */
        if(ptr == begin)
            throw std::runtime_error("key not found");
        if(*value == 0)
            throw std::runtime_error("value not found");

        /* find match in config table */
        const struct config_var *c;
        for(c = vars; c->type != config_var::NONE; c++)
            if(strcasecmp(c->name, begin) == 0)
                break;

        /* store configuration value */
        switch(c->type)
        {
            case config_var::NONE:
                throw std::runtime_error("unknown key");

            case config_var::INT:
                *(int *)c->data = strtol(value, NULL, 0);
                break;
            case config_var::FLOAT:
                *(float *)c->data = strtof(value, NULL);
                break;
            case config_var::STRING:
                *(char **)c->data = strdup(value);
                break;
            case config_var::BOOL:
                *(bool *)c->data = strcasecmp(value, "1") == 0 ||
                                   strcasecmp(value, "yes") == 0 ||
                                   strcasecmp(value, "true") == 0;
                break;
            case config_var::CALLBACK:
                ((void (*)(char *)) c->data)(value);
                break;
        }
    }

    return;
}

