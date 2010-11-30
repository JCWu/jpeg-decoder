#include "bitstream.h"
#include "jpeg_decoder.h"

__inline void skip(io_oper* st, int count) {
	st->Seek(st->Tell() + count);
}

__inline unsigned char get8(io_oper* st) {
	unsigned char res = 0;
	st->Read(&res, 1);
	return res;
}

__inline unsigned short get16be(io_oper* st) {
	unsigned short res = 0;
	st->Read(&res, 2);
	res = (res >> 8) + ((res & 0xff) << 8);
	return res;
}

__inline unsigned short get16le(io_oper* st) {
	unsigned short res = 0;
	st->Read(&res, 2);
	return res;
}

__inline unsigned int get32be(io_oper* st) {
	unsigned int res;
	res = get16be(st) + (get16be(st) << 16);
	return res;
}

bool CheckJpeg(io_oper* st) {
	st->Seek(0);
	return get8(st) == 0xff && get8(st) == 0xd8;
}

const static int zig_order[8][8] = {
	{0, 1, 5, 6, 14, 15, 27, 28},
	{2, 4, 7, 13, 16, 26, 29, 42},
	{3, 8, 12, 17, 25, 30, 41, 43},
	{9, 11, 18, 24, 31, 40, 44, 53},
	{10, 19, 23, 32, 39, 45, 52, 54},
	{20, 22, 33, 38, 46, 51, 55, 60},
	{21, 34, 37, 47, 50, 56, 59, 61},
	{35, 36, 48, 49, 57, 58, 62, 63}
};

struct huffman_node{
	huffman_node *child[2];
	unsigned char key;
};

void init_node(huffman_node* &p_node)
{
	p_node=(huffman_node*)malloc(sizeof(huffman_node));
	memset(p_node, 0, sizeof(huffman_node));
}

struct huffman_table
{
	unsigned char size[16];
	int count;
	unsigned char code[256];
	huffman_node* root;
};

typedef unsigned short qtab[64];

void print_tree(std::string str, huffman_node* t) {
	if (t->child[0]) {
		print_tree(str + "0", t->child[0]);
		print_tree(str + "1", t->child[1]);
	} else printf("%02x %s\n", t->key, str.c_str());
}

void huffman_tree(huffman_table &table)
{
	init_node(table.root);
	huffman_node* previos[256];
	huffman_node* current[256];
	int i, j, k, count=0, prev_size=1;
	previos[0]=table.root;

	for (i=0; i<16; i++){
		for (j=0; j<prev_size; j++){
			for (k=0; k<2; k++){
				init_node(previos[j]->child[k]);
				current[j*2+k]=previos[j]->child[k];
			}
		}
		for (j=0; j<table.size[i]; j++)
			current[j]->key=table.code[count++];
		if (count >= table.count) break;
		for (j=table.size[i], k=0; j<2*prev_size; j++)
			previos[k++]=current[j];

		prev_size=k;
	}

	print_tree("", table.root);
}

struct comp_info
{
	int type;
	int qt_id;
	int ac_table;
	int dc_table;
	int dc;
};

struct jpeg
{
	int w, h;
	int type;

	int dri;

	comp_info comp[3];
	qtab qt[3];
	huffman_table ac_huff[4], dc_huff[4];
	double G[64], covG[64];
};

static unsigned char get_section(io_oper* st) {
	while (get8(st) != 0xff) ;
	//get8(st);
	return get8(st);
}

static void skip_section(io_oper* st) {
	skip(st, get16be(st) - 2);
}

static void read_header(jpeg* j, io_oper* st) {
	int end = st->Tell(); end += get16be(st);
	get8(st);
	j->h = get16be(st);
	j->w = get16be(st);
	int t = get8(st); // comp ÊýÁ¿
	for (int i = 0; i < 3; ++i) {
		int id = get8(st);
		j->comp[id-1].type = get8(st);
		j->comp[id-1].qt_id = get8(st);
	}
	st->Seek(end);
	printf("w=%d h=%d t=%d\n", j->w, j->h, t);
}

static void read_huffman(jpeg* j, io_oper* st) {
	printf("<<DHT>>\n");
	int end = st->Tell(); end += get16be(st);
	while (st->Tell() < end) {
		unsigned char state = get8(st);
		int id = state & 15;
		int ac = ((state >> 4) & 1);
		huffman_table * table_ptr;
		if (ac) table_ptr = &j->ac_huff[id]; else table_ptr = &j->dc_huff[id];
		huffman_table &ht = *table_ptr;
		printf("HT %d %d\n", ac, id);
		ht.count = 0;
		for (int i = 0; i < 16; ++i) {
			ht.size[i] = get8(st);
			ht.count += ht.size[i];
		}
		for (int i = 0; i < ht.count; ++i)
			ht.code[i] = get8(st);
		huffman_tree(ht);
	}
	st->Seek(end);
}

static void read_sos(jpeg* j, io_oper* st) {
	printf("<<SOS>>\n");
	int end = st->Tell(); end += get16be(st);
	get8(st);
	for (int i = 0; i < 3; ++i) {
		int t = get8(st);
		unsigned char tb = get8(st);
		j->comp[t-1].ac_table = tb & 15;
		j->comp[t-1].dc_table = tb >> 4;
	}
	st->Seek(end);
}

static void read_dqt(jpeg* j, io_oper* st) {
	printf("<<DQT>>\n");
	int end = st->Tell(); end += get16be(st);
	while (st->Tell() < end) {
		unsigned char state = get8(st);
		int id = state & 15;
		int is16 = state >> 4;
		qtab &qt = j->qt[id];
		if (is16) {
			for (int i = 0; i < 64; ++i)
				qt[i] = get16le(st);
		} else {
			for (int i = 0; i < 64; ++i)
				qt[i] = get8(st);
		}

		printf("table id: %d\n", id);
		for (int i = 0; i < 8; ++i) {
			for (int j = 0; j < 8; ++j)
				printf("%4d ", qt[zig_order[i][j]]);
			printf("\n");
		}
	}
	st->Seek(end);
}

static void read_dri(jpeg* j, io_oper* st) {
	printf("<<DRI>>\n");
	int end = st->Tell(); end += get16be(st);
	j->dri = get16be(st);
	st->Seek(end);
}

void prepare_idct(jpeg* jpg)
{
	double pi=acos(.0)*2, sqr2=sqrt(2.0);

	for (int i=0; i<8; i++)
	{
		for (int j=0; j<8; j++)
		{
			jpg->G[i*8+j] = cos ( i*pi*(2*j+1)/16 )/2;
			if ( i==0 ) jpg->G[i*8+j]/=sqr2;
			jpg->covG[j*8+i]=jpg->G[i*8+j];
		}
	}			
}

void idct_multiply(double* dst, const double* src1, const double* src2)
{
	for (int i=0; i<8; i++)
		for (int j=0; j<8; j++){
			dst[i*8+j]=0;
			for (int k=0; k<8; k++)
				dst[i*8+j] += ( src1[i*8+k]* src2[k*8+j] );	
		}	
}

void idct(jpeg* j, double* dst, const double* src)
{
	double* store=(double*)malloc(sizeof(double)*64);
	idct_multiply(store, j->covG, src);
	idct_multiply(dst, store, j->G);
	free(store);
}

unsigned char get_huffman(huffman_node* node, bitstream* bits)
{
	if (node->child[0]==0)
		return node->key;
	return get_huffman(node->child[getbit(bits)], bits);
}

int decode_huffman(int len, bitstream *bits)
{
	if (len == 0) return 0;
	int sgn=!getbit(bits), ans = 1;
	for (int j = 1; j < len; ++j) ans = ans * 2 + (getbit(bits) ^ sgn);
	if ( sgn ) ans=-ans;
	return ans;
}

void read_block(jpeg *j, bitstream *bits, int* block, int comp_id)
{
	int key=(int)get_huffman(j->dc_huff[j->comp[comp_id].dc_table].root, bits);
	int i;

	j->comp[comp_id].dc+=decode_huffman(key, bits);
	block[0]=j->comp[comp_id].dc;

	for (i=0; i<63;){
		int val=(int)get_huffman(j->ac_huff[j->comp[comp_id].ac_table].root, bits);
		int id= val & 15;
		int num=val >> 4;

		if (id == 0 && num == 0) {
			while (i < 63) block[++i] = 0;
			break;
		}

		while ( num-- && i<62 ) block[ ++i ]=0;
		block[++i] = decode_huffman(id, bits);
	}
	if (i != 63) {
		printf("error !\n");
	}
}

typedef unsigned char array16[3][16][16];

void iqt(jpeg* j, double* dst, int* block, int comp_id)
{
	qtab &qt = j->qt[j->comp[comp_id].qt_id];
	for (int i=0; i<64; i++){
		int ind=zig_order[i/8][i%8];
		dst[i] = block[ind] * qt[ind];
	}
}

unsigned char check_number(double number) {
	int t = (int)(number + 128.5);
	if (t > 255) t = 255;
	if (t < 0) t = 0;
	return t;
}

void read_mcu(jpeg *j, bitstream* bits, array16 buf)
{	
	int block[64];
	double buffer[2][64];
	double temp[3][16][16];
	for (int i=0; i<4; i++)
	{
		read_block(j, bits, block ,0);

		iqt(j, buffer[0], block, 0);
		idct(j, buffer[1], buffer[0]);
		int x_off=i%2*8, y_off=i/2*8;
		for (int j=0; j<64; j++){
			temp[0][y_off+j/8][x_off+j%8]=buffer[1][j];
		}
	}

	for (int i = 1; i <= 2; ++i) {
		read_block(j, bits, block, i);
		iqt(j, buffer[0], block, i);
		idct(j, buffer[1], buffer[0]);

		for (int j=0; j<64; j++){
			temp[i][j/8*2][j%8*2]=buffer[1][j];
			temp[i][j/8*2 + 1][j%8*2]=buffer[1][j];
			temp[i][j/8*2][j%8*2 + 1]=buffer[1][j];
			temp[i][j/8*2 + 1][j%8*2 + 1]=buffer[1][j];
		}
	}

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 16; ++j) {
			buf[0][i][j] = check_number(temp[0][i][j] + 1.402 * (temp[2][i][j]));
			buf[1][i][j] = check_number(temp[0][i][j] -0.34414*(temp[1][i][j]) - 0.71414 * (temp[2][i][j]));
			buf[2][i][j] = check_number(temp[0][i][j] + 1.772 * (temp[1][i][j]));
		}
}

void* decode_mcu(jpeg *jp, io_oper* st)
{
	bitstream* bits=create_bit_stream(st);	
	unsigned char* buffer= new unsigned char[jp->w*jp->h*3];
	int mcu_i, mcu_j;
	array16 buf;
	static int a;

	if (jp->comp[0].type ==0x22 ){
		mcu_i = (jp->h+15)/16;
		mcu_j = (jp->w+15)/16;
	}

	for (int i=0; i<mcu_i; i++)
		for (int j=0; j<mcu_j; j++){
			read_mcu(jp, bits, buf);
			printf("%d\n", a++);
			for (int x=0; x<16; x++)
			{
				if ( i*16+x<jp->h ){
					for (int y=0; y<16; y++)
					{
						if ( j*16+y < jp->w ){
							buffer[((i*16+x)*jp->w+j*16+y)*3]=buf[0][x][y];
							buffer[((i*16+x)*jp->w+j*16+y)*3+1]=buf[1][x][y];
							buffer[((i*16+x)*jp->w+j*16+y)*3+2]=buf[2][x][y];
						}
					}
				}
			}
		}

	//for (int i=0; i< sizeof(unsigned char)*jp->w*jp->h*3; i++){
	//	printf("%02X ", buffer[i]);
	//}
	return buffer;
}

void * DecodeJpeg(io_oper* st, int &w, int &h, int &dep, int req_comp) {
	dep = 3;
	if (!CheckJpeg(st)) return 0;
	st->Seek(2);
	int done = 0;
	jpeg * j = (jpeg*) malloc(sizeof(jpeg));
	memset(j, 0, sizeof(jpeg));
	prepare_idct(j);
	while (! done ) {
		unsigned char sc = get_section(st);
		switch (sc) {
		case 0xc0: read_header(j, st); break; // baseline
		case 0xc1: return 0;
		case 0xc4: read_huffman(j, st); break; // huffman table
		case 0xc8: return 0;
		case 0xda: // SOS
			read_sos(j, st);
			done = true;
			break; 
		case 0xdb: read_dqt(j, st); break; // DQT
		case 0xdd: read_dri(j, st); break; // DRI
		default: skip_section(st);
		}
	}

	for (int i = 0; i < 3; ++i) {
		printf("comp%d : qt = %d ac = %d dc = %d\n", i, j->comp[i].qt_id, j->comp[i].ac_table, j->comp[i].dc_table);
	}
	void *result = decode_mcu(j, st);
	w = j->w;
	h = j->h;
	free(j);
	return result;
}
