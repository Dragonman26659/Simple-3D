#include "Renderer.h"



namespace Simple3D {
// Change depending on selected Windowing API
#ifdef SDL_WINDOW
	Renderer::Renderer(SDL_Window* window) {

	}

#else
	Renderer::Renderer(GLFWwindow* window) {

	}
#endif



}