#include "stdafx.h"

#include "GL/glut.h"

void display()
{
   glClear(GL_COLOR_BUFFER_BIT);
   glutWireTorus(0.25, 0.50, 32, 32);
   glutSwapBuffers();
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
   glutMainLoop();
}

