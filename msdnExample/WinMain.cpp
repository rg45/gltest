#include "stdafx.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
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
   gluPerspective(45.0, GLfloat(width) / height, 1.0, 100.0);
   glMatrixMode(GL_MODELVIEW);
}

class IGLObject
{
public:
   GLuint GetID() const { return m_id; }

   virtual size_t GetVersion() const = 0;
   virtual void Draw() = 0;

   virtual ~IGLObject() {}

private:
   static GLuint m_idCounter;
   GLuint m_id = ++m_idCounter;
};
GLuint IGLObject::m_idCounter;

class GLDisplayList : public IGLObject
{
public:

   explicit GLDisplayList(const std::function<void()>& draw) : m_draw(draw) {}

   size_t GetVersion() const override { return m_version; }
   void Draw() override { m_draw(); }

private:
   size_t m_version = 0;
   std::function<void()> m_draw;
};

template <typename T, typename U>
constexpr bool IsCollectionOf = std::is_convertible_v<std::decay_t<decltype(*std::begin(std::declval<T>()))>, U>;

class GLTestWindow
{
public:

   GLTestWindow(const GLTestWindow&) = delete;

   GLTestWindow(std::initializer_list<std::shared_ptr<IGLObject>> glObjects = {}) : GLTestWindow(glObjects, {}) { }

   template <typename GLObjectRange>
   GLTestWindow(const GLObjectRange& glObjects,
      std::enable_if_t<IsCollectionOf<GLObjectRange, std::shared_ptr<IGLObject>>, int> = {})
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
         glEnable(GL_BLEND);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         glEnable(GL_DEPTH_TEST);
         glClearColor(0.1f, 0.1f, 0.3f, 1);

         for (auto&& glObject : glObjects)
            AddGLObject(glObject);

         ShowWindow(m_hwnd, SW_SHOW);
         UpdateWindow(m_hwnd);
      }
   }

   explicit operator bool() const { return m_hwnd; };

   void AddGLObject(const std::shared_ptr<IGLObject>& glObject)
   {
      wglMakeCurrent(m_hdc, m_hrc);
      GLList list(glObject->GetID(), GL_COMPILE);
      glObject->Draw();
      m_glObjects[glObject->GetID()] = {glObject->GetVersion(), glObject};
   }

   GLvoid Draw(double dt)
   {
      //m_latitude = fmod(m_latitude + m_latinc * dt, 360);
      //m_latitude = 30;
      m_longitude = fmod(m_longitude + m_longinc * dt, 360);
      m_longinc *= std::exp(-dt);

      wglMakeCurrent(m_hdc, m_hrc);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glTranslated(0.0, 0.0, -m_radius);
      glRotated(m_latitude, 1.0, 0.0, 0.0);
      glRotated(m_longitude, 0.0, 1.0, 0.0);

      for (auto&& [id, pair] : m_glObjects)
      {
         if (const auto& glObject = pair.second.lock())
         {
            if (pair.first != glObject->GetVersion())
            {
               GLList list(glObject->GetID(), GL_COMPILE);
               glObject->Draw();
               pair = {glObject->GetVersion(), glObject};
            }
            glCallList(id);
         }
         else
         {
            m_glObjects.erase(id);
         }
      }
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
      if (uMsg != 0x84)
      {
         std::cout << std::hex << ", " << uMsg;
      }
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

      case WM_LBUTTONDOWN:
      case WM_MOUSEMOVE:
         {
            const int dragX = short(LOWORD(lParam));
            const int dragY = short(HIWORD(lParam));
            if (wParam & MK_LBUTTON)
            {
               m_longinc = (std::min)(500.0, (std::max)(-500.0, double(dragX - m_dragX) * 10));
               m_latitude = (std::min)(80.0, (std::max)(-80.0, m_latitude + double(dragY - m_dragY) / 5));
            }
            m_dragX = dragX;
            m_dragY = dragY;
         }
         break;

      case WM_MOUSEWHEEL:
         m_radius = (std::min)(30.0, (std::max)(3.0, m_radius - GET_WHEEL_DELTA_WPARAM(wParam) / double(WHEEL_DELTA)));
         break;

      case WM_KEYDOWN:
         switch (wParam)
         {
         case VK_ESCAPE:
            DestroyWindow(m_hwnd);
            break;
         case VK_LEFT:
            m_longinc += 5.0;
            break;
         case VK_RIGHT:
            m_longinc -= 5.0;
            break;
         case VK_UP:
            m_latitude = (std::min)(80.0, (std::max)(-80.0, m_latitude - 1));
            break;
         case VK_DOWN:
            m_latitude = (std::min)(80.0, (std::max)(-80.0, m_latitude + 1));
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
   std::map<GLuint, std::pair<size_t, std::weak_ptr<IGLObject>>> m_glObjects;
   int m_dragX = 0;
   int m_dragY = 0;
};

GLTestWindow::WndClass GLTestWindow::m_wndClass;


} // namespace

int main()
{
   std::shared_ptr<IGLObject> glObjects[]{
      std::make_shared<GLDisplayList>([] {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(0, 1, 0, 1);
         gluQuadricDrawStyle(quadric.get(), GLU_FILL);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glRotated(90.0, 1, 0, 0.7);
         glTranslated(1, 0, 0);
         gluCylinder(quadric.get(), 0.3, 0.3, 0.6, 24, 1);
         glPopMatrix();
      }),
      std::make_shared<GLDisplayList>([] {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(1, 0, 0, 1);
         gluQuadricDrawStyle(quadric.get(), GLU_FILL);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         gluCylinder(quadric.get(), 0.3, 0.1, 0.6, 24, 1);
      }),
      std::make_shared<GLDisplayList>([] {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(0, 0, 1, 1);
         gluQuadricDrawStyle(quadric.get(), GLU_LINE);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glRotated(90.0, 1, 0, 0);
         gluSphere(quadric.get(), 1.5, 32, 32);
         glPopMatrix();
      })
   };

   GLTestWindow windows[] = {
      {glObjects[0]},
      {glObjects[1]},
      {glObjects[0], glObjects[1]},
      {glObjects[2]},
      {glObjects[0], glObjects[2]},
      {glObjects[1], glObjects[2]},
      {glObjects[0], glObjects[1], glObjects[2]},
      glObjects,
      glObjects,
      glObjects,
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



