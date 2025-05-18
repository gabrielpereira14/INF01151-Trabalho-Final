#include "common.h"

#include <stdlib.h>
#include <stdio.h>

void perror_exit(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}