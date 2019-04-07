#include "stdafx.h"
#include <functional>

#include "GL/glut.h"

class AtExit
{
public:

   using Handler = std::function<void()>;

   explicit AtExit(Handler&& handler) : m_handler(std::move(handler)) {}

   ~AtExit() { m_handler(); }

private:
   Handler m_handler;
};

void display()
{
   glClear(GL_COLOR_BUFFER_BIT);
   glRotated(3, 1, 1, 0);
   glutWireTeapot(0.75);
   glutSwapBuffers();
}

void onKeyPressed(unsigned char key, int, int)
{
   switch (key)
   {
   case 'f':
      glutFullScreen();
      break;

   case 's':
      glutReshapeWindow(800, 800);
      break;

   case VK_ESCAPE:
      glutDestroyWindow(glutGetWindow());
      break;
   }
}

int main(int argc, char** args)
{
   std::cout << "Hello, World!" << std::endl;

   glutInit(&argc, args);
   glutInitWindowSize(800, 800);
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
   const int window = glutCreateWindow("Hello, OpenGL!");

   //const AtExit atExit([window]{glutDestroyWindow(window);});

   glClearColor(0.1f, 0.1f, 0.3f, 0.0f);
   glutDisplayFunc(display);
   glutKeyboardFunc(onKeyPressed);
   display();

   //glutMainLoop();

   while (glutGetWindow())
   {
      MSG msg {};
      while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
      Sleep(1);
   }
   std::cout << "Bye-Bye!" << std::endl;
}

