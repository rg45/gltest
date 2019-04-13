#include "stdafx.h"

#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <typeinfo>
#include <type_traits>

#include <windows.h> 
#include <winuser.h> 
#include <GL/gl.h> 
#include <GL/glu.h> 



namespace
{

BOOL bSetupPixelFormat(HDC hdc)
{
   PIXELFORMATDESCRIPTOR pfd{}, *ppfd {};
   int pixelformat{};

   ppfd = &pfd;

   ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
   ppfd->nVersion = 1;
   ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   ppfd->dwLayerMask = PFD_MAIN_PLANE;
   ppfd->iPixelType = PFD_TYPE_RGBA;
   ppfd->cColorBits = 24;
   ppfd->cAlphaBits = 8;
   ppfd->cDepthBits = 16;
   ppfd->cAccumBits = 0;
   ppfd->cStencilBits = 0;

   if ((pixelformat = ChoosePixelFormat(hdc, ppfd)) == 0)
   {
      MessageBox(nullptr, "ChoosePixelFormat failed", "Error", MB_OK);
      return FALSE;
   }

   if (SetPixelFormat(hdc, pixelformat, ppfd) == FALSE)
   {
      MessageBox(nullptr, "SetPixelFormat failed", "Error", MB_OK);
      return FALSE;
   }
   return TRUE;
}

struct QuadricDeleter { void operator()(GLUquadricObj* obj) { /*gluDeleteQuadric(obj)*/; } };
using QuadricPtr = std::unique_ptr<GLUquadricObj, QuadricDeleter>;
struct GLList
{
   explicit GLList(GLuint id, GLenum mode) { glNewList(id, mode); }
   ~GLList() { glEndList(); }
};

GLvoid resize(GLsizei width, GLsizei height)
{
   glViewport(0, 0, width, height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(45.0, GLfloat(width) / height, 3.0, 7.0);
   glMatrixMode(GL_MODELVIEW);
}

void polarView(GLdouble radius, GLdouble twist, GLdouble latitude, GLdouble longitude)
{
   glTranslated(0.0, 0.0, -radius);
   glRotated(-twist, 0.0, 0.0, 1.0);
   glRotated(-latitude, 1.0, 0.0, 0.0);
   glRotated(longitude, 0.0, 0.0, 1.0);
}

class GLTestWindow
{
public:

   GLTestWindow(const GLTestWindow&) = delete;

   GLTestWindow(std::initializer_list<std::function<void()>> initializerLists = {})
      : GLTestWindow(initializerLists, {}) { }

   template <typename DisplayLists>
   GLTestWindow(const DisplayLists& displayLists,
      std::enable_if_t<std::is_same_v<std::function<void()>, typename DisplayLists::value_type>, int> = {})
   {
      CreateWindow(
         m_wndClass.lpszClassName,
         "Generic OpenGL Sample",
         WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
         CW_USEDEFAULT,
         CW_USEDEFAULT,
         CW_USEDEFAULT, CW_USEDEFAULT,
         nullptr,
         nullptr,
         m_wndClass.hInstance,
         this);

      if (m_hwnd)
      {
         m_hdc = GetDC(m_hwnd);
         bSetupPixelFormat(m_hdc);
         m_hrc = wglCreateContext(m_hdc);
         wglMakeCurrent(m_hdc, m_hrc);

         GLuint displayListID = 0;
         for (auto&& displayList : displayLists)
         {
            SetDisplayList(++displayListID, displayList);
         }
         ShowWindow(m_hwnd, SW_SHOW);
         UpdateWindow(m_hwnd);
      }
   }

   explicit operator bool() const { return m_hwnd; };

   void SetDisplayList(GLuint id, const std::function<void()>& createList)
   {
      wglMakeCurrent(m_hdc, m_hrc);
      GLList list(id, GL_COMPILE);
      createList();
      m_displayLists.insert(id);
   }

   GLvoid Draw(GLdouble dt)
   {
      m_latitude = fmod(m_latitude + m_latinc * dt, 360);
      m_longitude = fmod(m_longitude + m_longinc * dt, 360);

      wglMakeCurrent(m_hdc, m_hrc);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      polarView(m_radius, 0, m_latitude, m_longitude);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glClearColor(0.1f, 0.1f, 0.3f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      for (const GLuint displayList : m_displayLists)
         glCallList(displayList);

      glPopMatrix();
      SwapBuffers(m_hdc);
   }

   ~GLTestWindow()
   {
      if (m_hwnd)
      {
         DestroyWindow(m_hwnd);
      }
   }

private:

   struct WndClass : public WNDCLASS
   {
      WndClass() : WNDCLASS()
      {
         lpszClassName = typeid(*this).name();
         hInstance = GetModuleHandle(nullptr);

         style = 0;
         lpfnWndProc = WndProcInit_;
         cbClsExtra = 0;
         cbWndExtra = 0;
         hIcon = LoadIcon (hInstance, lpszClassName);
         hCursor = LoadCursor(nullptr, IDC_ARROW);
         hbrBackground = HBRUSH(COLOR_WINDOW + 1);
         lpszMenuName = lpszClassName;
         RegisterClass(this);
      }
      ~WndClass()
      {
         UnregisterClass(lpszClassName, hInstance);
      }
   };

private:

   static LRESULT WINAPI WndProcInit_(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      if (uMsg == WM_CREATE)
      {
         const CREATESTRUCT* createStruct = LPCREATESTRUCT(lParam);
         SetWindowLongPtrA(hWnd, GWLP_USERDATA, LONG_PTR(createStruct->lpCreateParams));
         SetWindowLongPtrA(hWnd, GWLP_WNDPROC, LONG_PTR(WndProc_));
         GLTestWindow* wnd = reinterpret_cast<GLTestWindow*>(createStruct->lpCreateParams);
         wnd->m_hwnd = hWnd;
         return WndProc_(hWnd, uMsg, wParam, lParam);
      }
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
   }

   static LRESULT WINAPI WndProc_(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      std::cout << std::hex << ", " << uMsg;
      GLTestWindow* const wnd = reinterpret_cast<GLTestWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
      const auto res = wnd->WndProc(uMsg, wParam, lParam);

      if (uMsg == WM_DESTROY)
      {
         wnd->m_hwnd = nullptr;
         SetWindowLongPtr(hWnd, GWLP_WNDPROC, LONG_PTR(DefWindowProc));
      }
      return res;
   }

   LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      switch (uMsg)
      {
      case WM_CLOSE:
         DestroyWindow(m_hwnd);
         break;

      case WM_DESTROY:
         if (m_hrc)
            wglDeleteContext(m_hrc);

         if (m_hdc)
            ReleaseDC(m_hwnd, m_hdc);

         break;

      case WM_SIZE:
         {
            RECT rect{};
            GetClientRect(m_hwnd, &rect);
            resize(rect.right, rect.bottom);
         }
         break;

      case WM_KEYDOWN:
         switch (wParam)
         {
         case VK_ESCAPE:
            DestroyWindow(m_hwnd);
            break;
         case VK_LEFT:
            m_longinc += 0.5;
            break;
         case VK_RIGHT:
            m_longinc -= 0.5;
            break;
         case VK_UP:
            m_latinc += 0.5;
            break;
         case VK_DOWN:
            m_latinc -= 0.5;
            break;
         }
         break;

      default:
         return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
      }
      return true;
   }

private:
   static WndClass m_wndClass;
   HWND m_hwnd = nullptr;
   HDC   m_hdc = nullptr;
   HGLRC m_hrc = nullptr;
   GLdouble m_radius = 4.5;
   GLdouble m_latitude = 0.0;
   GLdouble m_longitude = 0.0;
   GLdouble m_latinc = 6.0;
   GLdouble m_longinc = 2.5;
   std::set<GLuint> m_displayLists;
};

GLTestWindow::WndClass GLTestWindow::m_wndClass;


std::vector<std::function<void()>> displayLists{
   [] {
      QuadricPtr quadric(gluNewQuadric());
      glColor4d(1, 0, 0, 1);
      gluQuadricDrawStyle(quadric.get(), GLU_FILL);
      gluQuadricNormals(quadric.get(), GLU_SMOOTH);
      gluCylinder(quadric.get(), 0.3, 0.1, 0.6, 24, 1);
   },
   [] {
      QuadricPtr quadric(gluNewQuadric());
      glColor4d(0, 1, 0, 0.7);
      gluQuadricDrawStyle(quadric.get(), GLU_FILL);
      gluQuadricNormals(quadric.get(), GLU_SMOOTH);
      glPushMatrix();
      glRotated(90.0, 1, 0, 0.7);
      glTranslated(1, 0, 0);
      gluCylinder(quadric.get(), 0.3, 0.3, 0.6, 24, 1);
      glPopMatrix();
   },
   [] {
      QuadricPtr quadric(gluNewQuadric());
      glColor4d(0, 0, 1, 0.5);
      gluQuadricDrawStyle(quadric.get(), GLU_LINE);
      gluQuadricNormals(quadric.get(), GLU_SMOOTH);
      gluSphere(quadric.get(), 1.5, 32, 32);
   }
};

} // namespace

int main()
{
   GLTestWindow windows[] = {
      {displayLists[0]},
      {displayLists[1]},
      {displayLists[0], displayLists[1]},
      {displayLists[2]},
      {displayLists[0], displayLists[2]},
      {displayLists[1], displayLists[2]},
      {displayLists[0], displayLists[1], displayLists[2]},
      displayLists,
      displayLists,
      displayLists,
   };

   auto t0 = GetTickCount64();
   for (;;)
   {
      auto t1 = GetTickCount64();

      size_t wndCount = 0;
      for (auto&& wnd : windows)
      {
         if (wnd)
         {
            wnd.Draw(GLdouble(t1 - t0) / 1000);
            ++wndCount;
         }
      }
      if (!wndCount)
      {
         break;
      }
      t0 = t1;
      Sleep(1);

      for (MSG msg{}; PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE); )
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }
}



