
#ifndef BMPPROC_H_
#define BMPPROC_H_

#include "io_oper.h"

bool CheckBMP(io_oper* st);
void * DecodeBMP(io_oper* st, int &w, int &h, int &dep, int req_comp);

#endif
