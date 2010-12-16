#ifndef __CONFIG_H__
#define __CONFIG_H__

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

    inline linereader(const char *filename);
    inline ~linereader();
    inline bool getline();
};

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

#endif
