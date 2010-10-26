#include "BMPproc.h"
#include <cassert>
/*
	¼ì²âBMP¸ñÊ½
*/

bool CheckBMP(io_oper* st) {
   int sz;
   st->Seek(0);
   unsigned char buf[18];
   int t = st->Read(buf, 18);
   if (t != 18) return false;
   if (buf[0] != 'B' || buf[1] != 'M') return false;
   sz = *(int*)(buf + 14);
   if (sz == 12 || sz == 40 || sz == 56 || sz == 108) return true;
   return false;
}

typedef unsigned int uint;
/*
   ÔØÈëBMP
*/
static unsigned char compute_y(int r, int g, int b) {
   return (unsigned char) (((r*77) + (g*150) +  (29*b)) >> 8);
}

static unsigned char *convert_format(unsigned char *data, int img_n, int req_comp, uint x, uint y)
{
   int i,j;
   unsigned char *good;

   if (req_comp == img_n) return data;
   assert(req_comp >= 1 && req_comp <= 4);

   good = (unsigned char *) malloc(req_comp * x * y);
   if (good == NULL) {
      free(data);
      return NULL;
   }

   for (j=0; j < (int) y; ++j) {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      #define COMBO(a,b)  ((a)*8+(b))
      #define CASE(a,b)   case COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and massive macros
      switch(COMBO(img_n, req_comp)) {
         CASE(1,2) dest[0]=src[0], dest[1]=255; break;
         CASE(1,3) dest[0]=dest[1]=dest[2]=src[0]; break;
         CASE(1,4) dest[0]=dest[1]=dest[2]=src[0], dest[3]=255; break;
         CASE(2,1) dest[0]=src[0]; break;
         CASE(2,3) dest[0]=dest[1]=dest[2]=src[0]; break;
         CASE(2,4) dest[0]=dest[1]=dest[2]=src[0], dest[3]=src[1]; break;
         CASE(3,4) dest[0]=src[0],dest[1]=src[1],dest[2]=src[2],dest[3]=255; break;
         CASE(3,1) dest[0]=compute_y(src[0],src[1],src[2]); break;
         CASE(3,2) dest[0]=compute_y(src[0],src[1],src[2]), dest[1] = 255; break;
         CASE(4,1) dest[0]=compute_y(src[0],src[1],src[2]); break;
         CASE(4,2) dest[0]=compute_y(src[0],src[1],src[2]), dest[1] = src[3]; break;
         CASE(4,3) dest[0]=src[0],dest[1]=src[1],dest[2]=src[2]; break;
         default: assert(0);
      }
      #undef CASE
   }

   free(data);
   return good;
}

static int high_bit(unsigned int z)
{
   int n=0;
   if (z == 0) return -1;
   if (z >= 0x10000) n += 16, z >>= 16;
   if (z >= 0x00100) n +=  8, z >>=  8;
   if (z >= 0x00010) n +=  4, z >>=  4;
   if (z >= 0x00004) n +=  2, z >>=  2;
   if (z >= 0x00002) n +=  1, z >>=  1;
   return n;
}

static int bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); // max 2
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); // max 4
   a = (a + (a >> 4)) & 0x0f0f0f0f; // max 8 per 4, now 8 bits
   a = (a + (a >> 8)); // max 16 per 8 bits
   a = (a + (a >> 16)); // max 32 per 8 bits
   return a & 0xff;
}

static int shiftsigned(int v, int shift, int bits)
{
   int result;
   int z=0;

   if (shift < 0) v <<= -shift;
   else v >>= shift;
   result = v;

   z = bits;
   while (z < 8) {
      result += v >> z;
      z += bits;
   }
   return result;
}

__inline void skip(io_oper* st, int count) {
	st->Seek(st->Tell() + count);
}

__inline unsigned char get8(io_oper* st) {
	unsigned char res = 0;
	st->Read(&res, 1);
	return res;
}

__inline unsigned short get16le(io_oper* st) {
	unsigned short res = 0;
	st->Read(&res, 2);
	return res;
}
__inline unsigned int get32le(io_oper* st) {
	unsigned int res = 0;
	st->Read(&res, 4);
	return res;
}

typedef unsigned char uc;

void * DecodeBMP(io_oper* st, int &w, int &h, int &dep, int req_comp) {
   st->Seek(0);
   io_oper*s = st;
   
   uc *out;
   unsigned int mr=0,mg=0,mb=0,ma=0;
   uc pal[256][4];
   int psize=0,i,j,compress=0,width;
   int bpp, flip_vertically, pad, target, offset, sz;
   
   if (get8(s) != 'B' || get8(s) != 'M') return 0;
   get32le(s); // discard filesize
   get16le(s); // discard reserved
   get16le(s); // discard reserved
   offset = get32le(s);
   sz = get32le(s);
   if (sz != 12 && sz != 40 && sz != 56 && sz != 108) return 0;
   
    if (sz == 12) {
      w = get16le(s);
      h = get16le(s);
   } else {
      w = get32le(s);
      h = get32le(s);
   }
   
   if (get16le(s) != 1) return 0;
   bpp = get16le(s);
   
   if (bpp == 1) return 0;
   flip_vertically = (h) > 0;
   h = abs(h);
   
    if (sz == 12) {
      if (bpp < 24)
         psize = (offset - 14 - 24) / 3;
   } else {
      compress = get32le(s);
      if (compress == 1 || compress == 2) return 0;
      get32le(s); // discard sizeof
      get32le(s); // discard hres
      get32le(s); // discard vres
      get32le(s); // discard colorsused
      get32le(s); // discard max important
      if (sz == 40 || sz == 56) {
         if (sz == 56) {
            get32le(s);
            get32le(s);
            get32le(s);
            get32le(s);
         }
         if (bpp == 16 || bpp == 32) {
            mr = mg = mb = 0;
            if (compress == 0) {
               if (bpp == 32) {
                  mr = 0xff << 16;
                  mg = 0xff <<  8;
                  mb = 0xff <<  0;
               } else {
                  mr = 31 << 10;
                  mg = 31 <<  5;
                  mb = 31 <<  0;
               }
            } else if (compress == 3) {
               mr = get32le(s);
               mg = get32le(s);
               mb = get32le(s);
               // not documented, but generated by photoshop and handled by mspaint
               if (mr == mg && mg == mb) {
                  // ?!?!?
                  return NULL;
               }
            } else
               return NULL;
         }
      } else {
         assert(sz == 108);
         mr = get32le(s);
         mg = get32le(s);
         mb = get32le(s);
         ma = get32le(s);
         get32le(s); // discard color space
         for (i=0; i < 12; ++i)
            get32le(s); // discard color space parameters
      }
      if (bpp < 16)
         psize = (offset - 14 - sz) >> 2;
   }
   dep = ma ? 4 : 3;
   if (req_comp && req_comp >= 3) // we can directly decode 3 or 4
      target = req_comp;
   else
      target = dep; // if they want monochrome, we'll post-convert
	  
   out = new uc[target * w * h];//(uc *) malloc(target * w * h);
   if (!out) return 0;
   if (bpp < 16) {
      int z=0;
      if (psize == 0 || psize > 256) { free(out); return 0; }
      for (i=0; i < psize; ++i) {
         pal[i][2] = get8(s);
         pal[i][1] = get8(s);
         pal[i][0] = get8(s);
         if (sz != 12) get8(s);
         pal[i][3] = 255;
      }
      skip(s, offset - 14 - sz - psize * (sz == 12 ? 3 : 4));
      if (bpp == 4) width = (w + 1) >> 1;
      else if (bpp == 8) width = w;
      else { free(out); return 0; }
      pad = (-width)&3;
      for (j=0; j < (int) h; ++j) {
         for (i=0; i < (int) h; i += 2) {
            int v=get8(s),v2=0;
            if (bpp == 4) {
               v2 = v & 15;
               v >>= 4;
            }
            out[z++] = pal[v][0];
            out[z++] = pal[v][1];
            out[z++] = pal[v][2];
            if (target == 4) out[z++] = 255;
            if (i+1 == (int) w) break;
            v = (bpp == 8) ? get8(s) : v2;
            out[z++] = pal[v][0];
            out[z++] = pal[v][1];
            out[z++] = pal[v][2];
            if (target == 4) out[z++] = 255;
         }
         skip(s, pad);
      }
   } else {
      int rshift=0,gshift=0,bshift=0,ashift=0,rcount=0,gcount=0,bcount=0,acount=0;
      int z = 0;
      int easy=0;
      skip(s, offset - 14 - sz);
      if (bpp == 24) width = 3 * w;
      else if (bpp == 16) width = 2 * w;
      else /* bpp = 32 and pad = 0 */ width=0;
      pad = ( - width) & 3;
      if (bpp == 24) {
         easy = 1;
      } else if (bpp == 32) {
         if (mb == 0xff && mg == 0xff00 && mr == 0xff000000 && ma == 0xff000000)
            easy = 2;
      }
      if (!easy) {
         if (!mr || !mg || !mb) return 0;
         // right shift amt to put high bit in position #7
         rshift = high_bit(mr)-7; rcount = bitcount(mr);
         gshift = high_bit(mg)-7; gcount = bitcount(mr);
         bshift = high_bit(mb)-7; bcount = bitcount(mr);
         ashift = high_bit(ma)-7; acount = bitcount(mr);
      }
      for (j=0; j < (int) h; ++j) {
         if (easy) {
            for (i=0; i < (int) w; ++i) {
               int a;
               out[z+2] = get8(s);
               out[z+1] = get8(s);
               out[z+0] = get8(s);
               z += 3;
               a = (easy == 2 ? get8(s) : 255);
               if (target == 4) out[z++] = a;
            }
         } else {
            for (i=0; i < (int) w; ++i) {
               unsigned int v = (bpp == 16 ? get16le(s) : get32le(s));
               int a;
               out[z++] = shiftsigned(v & mr, rshift, rcount);
               out[z++] = shiftsigned(v & mg, gshift, gcount);
               out[z++] = shiftsigned(v & mb, bshift, bcount);
               a = (ma ? shiftsigned(v & ma, ashift, acount) : 255);
               if (target == 4) out[z++] = a;
            }
         }
         skip(s, pad);
      }
   }
   if (flip_vertically) {
      uc t;
      for (j=0; j < (int) h >> 1; ++j) {
         uc *p1 = out +      j     *w*target;
         uc *p2 = out + (h-1-j)*w*target;
         for (i=0; i < w*target; ++i) {
            t = p1[i], p1[i] = p2[i], p2[i] = t;
         }
      }
   }

   if (req_comp && req_comp != target) {
      out = convert_format(out, target, req_comp, w, h);
      if (out == NULL) return out; // convert_format frees input on failure
   }

   return out;
}
