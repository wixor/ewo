#include <cstdio>
#include "config.h"

static int integer_field = 42;
static char *string_field = "hello";
static float float_field = 3.14;
static bool bool_field = true;

static struct config_var vars[] = {
    { "int",   config_var::INT,    &integer_field },
    { "str",   config_var::STRING, &string_field },
    { "float", config_var::FLOAT,  &float_field },
    { "bool",  config_var::BOOL,   &bool_field },
    { NULL,    config_var::NONE,   NULL }
};

int main(void)
{
    parse_config("test.cfg", vars);
    printf("int: %d, str: '%s', float: %f, bool: %s\n",
            integer_field, string_field, float_field,
            bool_field ? "true" : "false");
    return 0;
}
