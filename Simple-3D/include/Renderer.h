#pragma once
#include "Device.h"

//#define SDL_WINDOW

// Changes depending on if 
#ifdef SDL_WINDOW
#include "SDL.h"
#else
#include "GLFW/glfw3.h"
#endif




namespace Simple3D {
	class Renderer {
	public:
#ifdef SDL_WINDOW
		Renderer(SDL_Window* window);
#else
		Renderer(GLFWwindow* window);
#endif



		~Renderer();


		int Render();


	private:

	};
}