#include "ConsoleGraphics.hpp" //includes windows.h

int main()
{
	//Create a "window" that is 960x540 and create that window with a pixel size of 2x2
	cg::ConsoleGraphics window(1920 / 2, 1080 / 2, true, 2, true);
	window.setTitle("ConsoleGraphics Example");
	uint32 colourArr[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF}; //Pixels are 0xAARRGGBB
	cg::Image image1("example.bmp"), image2(colourArr, 2, 2, true);
	image1.resize(128, 128); //Resample image to 128x128 using default method (nearest neighbor)
	image2.resize(128, 128);

	float theta = 0.f;
	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		//Whole window isn't been drawn to so it must be cleared.
		window.clear();

		image1.setPos(25 + (25 * sinf(theta)), 0);
		image2.setPos(image1.getPosX(), image1.getHeight());
		window.draw(image1); //Draw to internal buffer
		window.draw(image2);

		window.display(); //Swap buffers and show user what is drawn
		theta += 0.001f;
		if (theta > 360.f) {
			theta -= 360.f;
		}
	}
}
