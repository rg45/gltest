#include "stdafx.h"

#include "GL/glut.h"

void display()
{
   glClear(GL_COLOR_BUFFER_BIT);
   glRotated(3, 1, 1, 0);
   glutWireTorus(0.25, 0.50, 32, 32);
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
   glutCreateWindow("Hello, OpenGL!");

   glClearColor(0.1f, 0.1f, 0.3f, 0.0f);
   glutDisplayFunc(display);
   glutKeyboardFunc(onKeyPressed);
   glutMainLoop();
}

