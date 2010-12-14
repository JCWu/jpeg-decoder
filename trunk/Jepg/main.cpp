/** 
 * @author 
 */

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <algorithm>
#include <cmath>
#include "dialog.h"
#include "io_oper.h"
#include "BMPproc.h"
#include "jpeg_decoder.h"

LRESULT CALLBACK 
	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
VOID EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC);
VOID DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);


int width, height, window_width, window_height;
int center_x=0, center_y=0;
double scale=1;
void* buffer;
GLuint tex;

void InitOpenGL();
void Render(double);
void init() {
	std::wstring fname;
	bool ok = GetFileName(fname);
	if (! ok ) {
		exit(0);
	}

	buffer = 0;
	io_oper* ist = IStreamFromFile(fname);
	if (CheckBMP(ist)) {
		int dep;
		void * tmp;
		tmp = DecodeBMP(ist, width, height, dep, 3);
		if (!tmp) return;
		buffer = tmp;
	} else if (CheckJpeg(ist)) {
		int dep;
		void * tmp;
		tmp = DecodeJpeg(ist, width, height, dep, 3);
		if (!tmp) return;
		buffer = tmp;
	}
}

int convert(int tmp)
{
	int res = 1;
	while (res < tmp) res *= 2;
	return res;
}

int main (int argc, char* args[])
{
	WNDCLASS wc;
	HWND hWnd;
	HDC hDC;
	HGLRC hRC;    
	MSG msg;
	BOOL bQuit = FALSE;
	float theta = 0.0f;
	double scale_disp;

	init();	
	if (buffer == 0) return 1;
	// register window class
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle( NULL );
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = TEXT("GLSample");
	RegisterClass( &wc );

	// create main window
	hWnd = CreateWindow( 
		TEXT("GLSample"), TEXT("OpenGL Sample"), 
		WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE | WS_MAXIMIZE ,
		0, 0, 640, 480,
		NULL, NULL, wc.hInstance, NULL);

	WndProc(hWnd, WM_SIZE, 0, 0);

	// enable OpenGL for the window
	EnableOpenGL( hWnd, &hDC, &hRC );

	InitOpenGL();

	while (width * scale > window_width || height * scale > window_height) scale *= 0.5;
	scale_disp=0.05;
	double last_tick=GetTickCount();

	// program main loop
	while (!bQuit) 
	{
		// check for messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			// handle or dispatch messages
			if (msg.message == WM_QUIT) 
			{
				bQuit = TRUE;
			} 
			else 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

		} 
		else 
		{
			int now_tick = GetTickCount();
			double dis=fabs(scale-scale_disp);
			using namespace std;
			double dt=min(dis, 2e-3*(now_tick-last_tick)*(dis+0.25));
			scale_disp+= dt*(scale>scale_disp?1:-1);
			last_tick = now_tick;
			Render(scale_disp);
			SwapBuffers(hDC);
		}
	}
	// shutdown OpenGL
	DisableOpenGL( hWnd, hDC, hRC );
	// destroy the window explicitly
	DestroyWindow( hWnd );
	return msg.wParam;
}

// Window Procedure

static void save_point(LPARAM lParam, POINT& pt)
{		
	pt.x=LOWORD(lParam);
	pt.y=HIWORD(lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static short t = 0;
	static RECT rect;
	static POINT pos;
	static bool first = true;

	switch (message) 
	{
	case WM_CREATE:
		return 0;

	case WM_SIZE:
		GetClientRect(hWnd, &rect);
		window_width=rect.right-rect.left;
		window_height=rect.bottom-rect.top;
		return 0;

	case WM_CLOSE:
		PostQuitMessage( 0 );
		return 0;

	case WM_DESTROY:
		return 0;

	case WM_LBUTTONDOWN:
		save_point(lParam, pos);
		return 0;

	case WM_MOUSEMOVE:
		if ( wParam==MK_LBUTTON )
		{	if (first) first = false;
			else {
				center_x+=(LOWORD(lParam)-pos.x);
				center_y-=(HIWORD(lParam)-pos.y);
			}
			save_point(lParam, pos);
		}
		return 0;

	case WM_MOUSEWHEEL:
		t+=HIWORD(wParam);
		if (t > 100) scale *= 1.2, t = 0;
		else if (t < 100) scale *= 0.8, t = 0;
		if ( scale<0.05 ) scale=0.05;
		return 0;

	case WM_KEYDOWN:
		switch (wParam) 
		{
		case VK_ESCAPE:
			PostQuitMessage( 0 );
			return 0;
		}
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

VOID EnableOpenGL( HWND hWnd, HDC * hDC, HGLRC * hRC )
{
	PIXELFORMATDESCRIPTOR pfd;
	int iFormat;

	*hDC = GetDC( hWnd );

	// set the pixel format for the DC
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | 
		PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	iFormat = ChoosePixelFormat( *hDC, &pfd );
	SetPixelFormat( *hDC, iFormat, &pfd );

	// create and enable the render context (RC)
	*hRC = wglCreateContext( *hDC );
	wglMakeCurrent( *hDC, *hRC );
}

// Disable OpenGL

VOID DisableOpenGL( HWND hWnd, HDC hDC, HGLRC hRC )
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hRC );
	ReleaseDC( hWnd, hDC );
} 

int restyScaleImage(int iw, int ih, unsigned char *src, int ow, int oh, unsigned char *dst) {
	for (int i = 0; i < oh; ++i) {
		for (int j = 0; j < ow; ++j) {
			int c = i * ih / oh, r = j * iw / ow;
			dst[0] = src[(c * iw + r) * 3];
			dst[1] = src[(c * iw + r) * 3 + 1];
			dst[2] = src[(c * iw + r) * 3 + 2];
			dst += 3;
		}
	}
	return 0;
}

void InitOpenGL() {
	glDisable( GL_DEPTH_TEST );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho(-window_width/2, window_width/2, -window_height/2, window_height/2,  -1, 1);

	int t_width=convert(width), t_height=convert(height);
	GLubyte* outbuffer=new GLubyte[t_width*t_height*3];

	int ans=restyScaleImage(width, height, (unsigned char*) buffer, t_width, t_height, outbuffer);
	if (ans) exit(-1);
	delete[] buffer;
	buffer=outbuffer;

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGB,
		t_width, t_height,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		buffer);
}

void Render(double scale) {
	glClear( GL_COLOR_BUFFER_BIT );
	
	int left = 0;
	int top = 0;
	int right = 1;
	int bottom = 1;

	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2i( left, top );
		glVertex2d(center_x-scale*width/2, center_y+scale*height/2);
		glTexCoord2i( right, top );
		glVertex2d(center_x+scale*width/2, center_y+scale*height/2);
		glTexCoord2i( right, bottom );
		glVertex2d(center_x+scale*width/2, center_y-scale*height/2);
		glTexCoord2i( left, bottom );
		glVertex2d(center_x-scale*width/2, center_y-scale*height/2);
	glEnd();
}
