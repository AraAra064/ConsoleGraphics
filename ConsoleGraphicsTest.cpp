#include "ConsoleGraphics.hpp"

using cg::InterpolationMethod;

int main()
{
	cg::ConsoleGraphics window(1920 / 2, 1080 / 2, true, 2, true);
	window.setTitle("ConsoleGraphics Example");
	window.enableAlpha();
	uint32 colourArr[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
	cg::Image image1("example.bmp"), image2(colourArr, 2, 2, true);
	image1.resize(128, 128);
	image2.resize(128, 128);

	float theta = 0.f;
	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		//Whole window isn't been drawn to so it must be cleared.
		window.clear();

		image1.setPos(25 + (25 * sinf(theta)), 0);
		image2.setPos(image1.getPosX(), image1.getHeight());
		window.draw(image1);
		window.draw(image2);

		window.display();
		theta += 0.001f;
		if (theta > 360.f) {
			theta -= 360.f;
		}
	}
}