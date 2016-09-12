#include <stdlib.h>
#include "beast_module.h"

extern struct beast_ops des_handler_ops;
extern struct beast_ops aes_handler_ops;
extern struct beast_ops base64_handler_ops;

struct beast_ops *ops_handler_list[] = {
    &des_handler_ops,
    &aes_handler_ops,
    &base64_handler_ops,
    NULL,
};
