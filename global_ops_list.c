#include <stdlib.h>
#include "ops_struct.h"

extern struct beast_ops des_handler_ops;

struct beast_ops *ops_handler_list[] = {
	&des_handler_ops,
	NULL,
};
