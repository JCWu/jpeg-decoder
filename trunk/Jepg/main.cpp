/** 
 * @author 
 */

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include "dialog.h"
#include "io_oper.h"
#include "BMPproc.h"
#include "jpeg_decoder.h"

LRESULT CALLBACK 
	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
VOID EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC);
VOID DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);


int width, height, window_width, window_height;
void* buffer;
GLuint tex;

void InitOpenGL();
void Render();
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
		WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE | WS_MAXIMIZE | WS_OVERLAPPEDWINDOW,
		0, 0, 640, 480,
		NULL, NULL, wc.hInstance, NULL);

	// enable OpenGL for the window
	EnableOpenGL( hWnd, &hDC, &hRC );

	InitOpenGL();
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
			Render();
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static short t;
	static RECT rect;

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

	case WM_MOUSEWHEEL:
		t=HIWORD(wParam);
		window_height=window_height+(int)(window_height*t/120*0.1);
		window_width=window_width+(int)(window_width*t/120*0.1);
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
	window_width=width; window_height=height;

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

void Render() {
	glClear( GL_COLOR_BUFFER_BIT );
	
	int left = 0;
	int top = 0;
	int right = 1;
	int bottom = 1;

	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2i( left, top );
		glVertex2d(-window_width/2, window_height/2);
		glTexCoord2i( right, top );
		glVertex2d(window_width/2, window_height/2);
		glTexCoord2i( right, bottom );
		glVertex2d(window_width/2, -window_height/2);
		glTexCoord2i( left, bottom );
		glVertex2d(-window_width/2, -window_height/2);
	glEnd();
}
