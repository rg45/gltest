#include "stdafx.h"

#include <algorithm>
#include <fstream>
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

GLvoid setProjection(GLsizei width, GLsizei height)
{
   glViewport(0, 0, width, height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(45.0, GLfloat(width) / height, 1.0, 25.0);
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

template <typename T, typename U>
constexpr bool IsCollectionOf = std::is_convertible_v<std::decay_t<decltype(*std::begin(std::declval<T>()))>, U>;

constexpr double floorLevel = 0;
constexpr double topLevel = 2.5;

using byte = unsigned char;

template <typename T>
void extract(std::istreambuf_iterator<char>& it, T& t)
{
   for (size_t i = 0; i < sizeof(T); ++i)
      reinterpret_cast<char*>(&t)[i] = *it++;
}

class GLTexture
{
public:

      GLTexture(GLuint id, const std::string& fname)
         : m_id(id)
      {
         std::ifstream input(fname, std::ios::binary);
         std::istreambuf_iterator<char> it(input);

         extract(it, m_fileHeader);
         if (m_fileHeader.bfType != 0x4D42)
            return;

         extract(it, m_infoHeader);
         if (m_infoHeader.biSize != sizeof(m_infoHeader))
            return;

         m_data.reserve(m_fileHeader.bfSize - m_fileHeader.bfOffBits);

         std::copy(it, std::istreambuf_iterator<char>(), std::back_inserter(m_data));
      }

      GLuint GetID() const { return m_id; }
      LONG GetWidth() const { return m_infoHeader.biWidth; }
      LONG GetHeight() const { return (std::abs)(m_infoHeader.biHeight); }
      bool IsTopDown() const { return m_infoHeader.biHeight < 0; }
      WORD GetBitsPerPixel() const { return m_infoHeader.biBitCount; }
      DWORD GetFormat() const { return m_infoHeader.biCompression; }

      bool empty() const { return m_data.empty(); }
      size_t size() const { return m_data.size(); }
      const byte* begin() const { return reinterpret_cast<const byte*>(m_data.data()); }
      const byte* end() const { return begin() + size(); }
      explicit operator bool() const { return !empty(); }

private:
   GLuint m_id = 0;
   BITMAPFILEHEADER m_fileHeader{};
   BITMAPINFOHEADER m_infoHeader{};
   std::vector<char> m_data;
};

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

         RECT rect {};
         GetClientRect(m_hwnd, &rect);
         setProjection(rect.right, rect.bottom);

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
      m_longinc *= std::exp(-dt / 2);
      if (-5 < m_longinc && m_longinc < 5)
         m_longinc = 0;

      wglMakeCurrent(m_hdc, m_hrc);

      glEnable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glPolygonMode(GL_BACK, GL_LINE);

      glClearColor(0.1f, 0.1f, 0.3f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glTranslated(0.0, 0.0, -m_viewDistance);
      glRotated(m_latitude, 1.0, 0.0, 0.0);
      glRotated(m_longitude, 0.0, 1.0, 0.0);
      glTranslated(0.0, -m_viewLevel, 0.0);

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

   void AddTexture(const GLTexture& texture)
   {
      wglMakeCurrent(m_hdc, m_hrc);

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, texture.GetID());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.GetWidth(),
         texture.GetHeight(), 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, texture.begin());

      glBindTexture(GL_TEXTURE_2D, 0);
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
      const auto res = wnd->WindowProc(hWnd, uMsg, wParam, lParam);

      if (uMsg == WM_DESTROY)
      {
         wnd->m_hwnd = nullptr;
         SetWindowLongPtr(hWnd, GWLP_WNDPROC, LONG_PTR(DefWindowProc));
      }
      return res;
   }

   LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
   {
      switch (uMsg)
      {
      case WM_CLOSE:
         DestroyWindow(hwnd);
         break;

      case WM_DESTROY:
         if (m_hrc)
            wglDeleteContext(m_hrc);

         if (m_hdc)
            ReleaseDC(hwnd, m_hdc);

         break;

      case WM_SIZE:
         {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            setProjection(rect.right, rect.bottom);
         }
         break;

      case WM_LBUTTONDOWN:
      case WM_MOUSEMOVE:
         {
            const int dragX = short(LOWORD(lParam));
            const int dragY = short(HIWORD(lParam));
            if (wParam & MK_LBUTTON)
            {
               m_longinc = 0;
               m_longitude = std::fmod(m_longitude + double(dragX - m_dragX) / 5, 360);
               m_latitude = (std::min)(80.0, (std::max)(-80.0, m_latitude + double(dragY - m_dragY) / 5));
            }
            m_dragX = dragX;
            m_dragY = dragY;
         }
         break;

      case WM_MOUSEWHEEL:
         m_viewDistance = (std::min)(20.0, (std::max)(3.0, m_viewDistance - GET_WHEEL_DELTA_WPARAM(wParam) / double(WHEEL_DELTA)));
         break;

      case WM_KEYDOWN:
         switch (wParam)
         {
         case VK_ESCAPE:
            DestroyWindow(hwnd);
            break;
         case VK_LEFT:
            m_longinc -= 10.0;
            break;
         case VK_RIGHT:
            m_longinc += 10.0;
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
         return DefWindowProc(hwnd, uMsg, wParam, lParam);
      }
      return true;
   }

private:
   static WndClass m_wndClass;
   HWND m_hwnd = nullptr;
   HDC   m_hdc = nullptr;
   HGLRC m_hrc = nullptr;
   double m_viewDistance = 4;
   double m_viewLevel = (floorLevel * 75 + topLevel * 0.25);
   double m_latitude = 10.0;
   double m_longitude = -20.0;
   double m_latinc = 6.0;
   double m_longinc = 2.5;
   std::map<GLuint, std::pair<size_t, std::weak_ptr<IGLObject>>> m_glObjects;
   int m_dragX = 0;
   int m_dragY = 0;
};

GLTestWindow::WndClass GLTestWindow::m_wndClass;

double rand() { return std::rand() / double(RAND_MAX);  }

class GLDisplayList : public IGLObject
{
public:

   GLDisplayList() = default;
   explicit GLDisplayList(const std::function<void()>& draw) : m_draw(draw) {}

   size_t GetVersion() const override { return m_version; }
   void Draw() override { m_draw(); }

   void Reset(const std::function<void()>& draw)
   {
      m_draw = draw;
      ++m_version;
   }

private:
   size_t m_version = 0;
   std::function<void()> m_draw = []{};
};

class JumpingBall : public GLDisplayList
{
public:

   JumpingBall() = default;
   JumpingBall(double radius, const std::function<void()>& model) : m_radius(radius), m_model(model) {}

   void Calc(double dt)
   {
      m_t = std::fmod(m_t + dt, m_vy/5);

      m_y = floorLevel + m_radius + m_vy * m_t - 10 * m_t * m_t / 2;
      m_rotation = std::fmod(m_rotation + 123 * dt, 360);

      m_x += m_vx * dt;
      if (m_x < -3 + m_radius && m_vx < 0 || m_x > 3 - m_radius && m_vx > 0)
         m_vx = -m_vx;

      m_z += m_vz * dt;
      if (m_z < -3 + m_radius && m_vz < 0 || m_z > 3 - m_radius && m_vz > 0)
         m_vz = -m_vz;

      Reset(createDisplayList());
   }

private:

   std::function<void()> createDisplayList() const
   {
      return [this] {
         glPushMatrix();
         glTranslated(m_x, m_y, m_z);
         glRotated(m_rotation, 1, 1, 1);

         m_model();

         glPopMatrix();
      };
   }

private:
   size_t m_version = 0;
   double m_radius = 0.105;
   double m_x = rand() * 4 - 2;
   double m_y = 0;
   double m_z = rand() * 4 - 2;
   double m_rotation = 0;
   double m_vx = rand() * 2 - 1;
   double m_vy = rand() * 3 + 3;
   double m_vz = rand() * 2 - 1;
   double m_t = rand() * m_vy/5;

   std::function<void()> m_model = [this, red = rand(), green = rand(), blue = rand()]{
      QuadricPtr quadric(gluNewQuadric());
      glColor4d(red, green, blue, 1);
      gluQuadricDrawStyle(quadric.get(), GLU_LINE);
      gluQuadricNormals(quadric.get(), GLU_SMOOTH);
      gluSphere(quadric.get(), m_radius, 16, 16);
   };
};

} // namespace

int main()
{
   std::shared_ptr<IGLObject> glObjects[]{
      std::make_shared<GLDisplayList>([] {
         glBegin(GL_QUAD_STRIP);
         
         glColor3d(0.2, 0.2, 0.2);
         glVertex3d(-3, floorLevel, -3);
         glColor3d(0.2, 0.7, 0.2);
         glVertex3d(-3, topLevel, -3);

         glColor3d(0.2, 0.2, 0.7);
         glVertex3d(-3, floorLevel, 3);
         glColor3d(0.2, 0.7, 0.7);
         glVertex3d(-3, topLevel, 3);

         glColor3d(0.7, 0.2, 0.7);
         glVertex3d(3, floorLevel, 3);
         glColor3d(0.7, 0.7, 0.7);
         glVertex3d(3, topLevel, 3);

         glColor3d(0.2, 0.7, 0.7);
         glVertex3d(3, floorLevel, -3);
         glColor3d(0.2, 0.2, 0.7);
         glVertex3d(3, topLevel, -3);

         glColor3d(0.2, 0.2, 0.2);
         glVertex3d(-3, floorLevel, -3);
         glColor3d(0.2, 0.7, 0.2);
         glVertex3d(-3, topLevel, -3);

         glEnd();

         {
            const int m = 8;
            const int n = 8;
            const double tileBegin = 0;
            const double tileEnd = 1;

            for (int i = 0; i < m; ++i)
            {
               double z0 = -3 + 6.0 / m * i;
               double z1 = z0 + 6.0 / m;

               for (int j = 0; j < n; ++j)
               {

                  double x0 = -3 + 6.0 / n * j;
                  double x1 = x0 + 6.0 / n;

                  glColor3d(1, 1, 1);
                  if (i == 0 || i == m - 1 || j == 0 || j == n - 1)
                  {
                     glBindTexture(GL_TEXTURE_2D, 2);
                  }
                  else
                  {
                     glBindTexture(GL_TEXTURE_2D, 1);
                  }

                  glBegin(GL_QUADS);
                  glVertex3d(x0, floorLevel, z0);
                  glTexCoord2d(tileBegin, tileBegin);
                  glVertex3d(x0, floorLevel, z1);
                  glTexCoord2d(tileBegin, tileEnd);
                  glVertex3d(x1, floorLevel, z1);
                  glTexCoord2d(tileEnd, tileEnd);
                  glVertex3d(x1, floorLevel, z0);
                  glTexCoord2d(tileEnd, tileBegin);
                  glEnd();
                  glBindTexture(GL_TEXTURE_2D, 0);
               }
            }
         }

      }),
   };

   GLTestWindow windows[] = {
      glObjects,
      glObjects,
   };

   std::vector<std::shared_ptr<JumpingBall>> balls;

   const auto jumpingGlobe = [] {
      {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(0, 1, 0, 1);
         gluQuadricDrawStyle(quadric.get(), GLU_FILL);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glRotated(90.0, 1, 0, 0.7);
         glTranslated(0.3, 0, 0);
         gluCylinder(quadric.get(), 0.1, 0.1, 0.2, 24, 1);
         glPopMatrix();
      }
      {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(1, 0, 0, 1);
         gluQuadricDrawStyle(quadric.get(), GLU_FILL);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glTranslated(0, 0, 0.20);
         gluCylinder(quadric.get(), 0.1, 0.04, 0.2, 24, 1);
         glPopMatrix();
      }
      {
         QuadricPtr quadric(gluNewQuadric());
         glColor3d(1, 1, 0);
         gluQuadricDrawStyle(quadric.get(), GLU_LINE);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glTranslated(0, 0, -0.20);
         gluSphere(quadric.get(), 0.1, 16, 16);
         glPopMatrix();
      }
      {
         QuadricPtr quadric(gluNewQuadric());
         gluQuadricDrawStyle(quadric.get(), GLU_LINE);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glTranslated(-0.3, 0, 0);
         glColor3d(0.5, 0, 0.5);
         gluSphere(quadric.get(), 0.07, 16, 16);
         glColor3d(1, 0, 0);
         gluDisk(quadric.get(), 0.07, 0.15, 32, 1);
         glRotated(90, 1, 0, 0);
         gluDisk(quadric.get(), 0.07, 0.15, 32, 1);
         glPopMatrix();
      }
      {
         QuadricPtr quadric(gluNewQuadric());
         glColor4d(0, 0, 1, 0.4);
         gluQuadricDrawStyle(quadric.get(), GLU_LINE);
         gluQuadricNormals(quadric.get(), GLU_SMOOTH);
         glPushMatrix();
         glRotated(90.0, 1, 0, 0);
         gluSphere(quadric.get(), 0.5, 32, 32);
         glPopMatrix();
      }
   };

   for (size_t i = 0; i < 3; ++i)
   {
      balls.push_back(std::make_shared<JumpingBall>(0.5, jumpingGlobe));
   }

   for (size_t i = 0; i < 20; ++i)
   {
      balls.push_back(std::make_shared<JumpingBall>());
   }

   {
      GLTexture textures[]{
         {1, "Resources/tiles2.bmp"},
         {2, "Resources/tiles3.bmp"}
      };

      for (auto&& wnd : windows)
      {
         for (auto&& texture : textures)
         {
            wnd.AddTexture(texture);
         }
         for (auto&& ball : balls)
         {
            wnd.AddGLObject(ball);
         }
      }
   }

   auto t0 = GetTickCount64();
   for (;;)
   {
      auto t1 = GetTickCount64();
      const auto dt = double(t1 - t0) / 1000;

      for (auto&& ball : balls)
         ball->Calc(dt);

      size_t wndCount = 0;
      for (auto&& wnd : windows)
      {
         if (wnd)
         {
            wnd.Draw(dt);
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



