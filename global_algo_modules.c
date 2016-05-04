#include <stdlib.h>
#include "beast_module.h"

extern struct beast_ops des_handler_ops;

struct beast_ops *ops_handler_list[] = {
	&des_handler_ops,
	NULL,
};
