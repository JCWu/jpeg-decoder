#ifndef _JPEG_DECODER__
#define _JPEG_DECODER__

#include "io_oper.h"

bool CheckJpeg(io_oper* st);
void * DecodeJpeg(io_oper* st, int &w, int &h, int &dep, int req_comp);

#endif
