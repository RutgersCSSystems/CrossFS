#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "unvme_nvme.h"
#include "crfslib.h"

int main(int argc, char **argv) {
	crfsinit(USE_DEFAULT_PARAM, USE_DEFAULT_PARAM, USE_DEFAULT_PARAM);
	crfsexit();
	return 0;
}
