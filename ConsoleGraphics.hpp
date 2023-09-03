//ConsoleGraphics Version 1.0-Beta-3
//ISO C++ 11 or higher must be used to compile ConsoleGraphics.

#include <vector>
#include <limits>
#include <cmath>
#include <string>
#include <fstream>
#include <algorithm>
#include <utility>
#include <deque>
#include <mutex>
//#include <memory>

#define NOMINMAX
#include <windows.h>

#ifdef CG_DEBUG
	#include <iostream>
#endif

#ifndef int8
	#define int8 int8_t
	#define uint8 uint8_t
	#define int16 int16_t
	#define uint16 uint16_t
	#define int32 int32_t
	#define uint32 uint32_t
	#define int64 int64_t
	#define uint64 uint64_t
#endif

#ifndef CG_INCLUDE
#define CG_INCLUDE

class ConsoleGraphics;

#undef RGB

namespace cg
{
	uint32 RGB(uint8 r, uint8 g, uint8 b){return r | (g << 8) | (b << 16);
	}
	uint32 BGR(uint8 r, uint8 g, uint8 b){return RGB(b, g, r);
	}
	uint32 RGBA(uint8 r, uint8 g, uint8 b, uint8 a){return RGB(r, g, b) | (a << 24);
	}
	uint32 RGBA(uint32 rgb, uint8 a){return (rgb & 0x00FFFFFF) | (a << 24);
	}
	uint32 BGRA(uint8 r, uint8 g, uint8 b, uint8 a){return RGBA(b, g, r, a);
	}
	uint32 BGRA(uint32 bgr, uint8 a){return (bgr & 0x00FFFFFF) | (a << 24);
	}
	
	//If isRGB == false, then input is assumed to be in the BGRA format
	uint8 GetR(uint32 rgba, bool isRGB = false){return isRGB ? rgba : rgba >> 16;
	}
	uint8 GetG(uint32 rgba){return rgba >> 8;
	}
	uint8 GetB(uint32 rgba, bool isRGB = false){return isRGB ? rgba >> 16 : rgba;
	}
	uint8 GetA(uint32 rgba){return rgba >> 24;
	}
};

using namespace cg;

uint32 blendPixel(uint32 dstRGB, uint32 srcRGB, uint8 srcA)
{
	static uint16 r, g, b;
	
	//Get*Value calls are flipped because the rgb values are.
	r = (GetR(dstRGB, false)*(255-srcA))+(GetR(srcRGB, false)*srcA);
	g = (GetG(dstRGB)*(255-srcA))+(GetG(srcRGB)*srcA);
	b = (GetB(dstRGB, false)*(255-srcA))+(GetB(srcRGB, false)*srcA);
	r /= 255, g /= 255, b /= 255;
	
	return cg::BGR((uint8)r, (uint8)g, (uint8)b);
}

enum class InterpolationMethod{None, NearestNeighbor, Bilinear, AreaAveraging};
enum class ExtrapolationMethod{None, Repeat, Extend};

enum class FilterType{Grayscale, WeightedGrayscale, Invert, Custom = 255};

class Image
{
	//First element in pair is for rgb data, the second is for alpha
	std::vector<std::pair<uint32, uint8>> pixels;
	uint32 width, height, x = 0, y = 0;
	float aspectRatio;
	
	protected:
		std::vector<std::pair<uint32, uint8>> resizeData(std::vector<std::pair<uint32, uint8>> &pixels, uint32 newWidth, uint32 newHeight, InterpolationMethod m)
		{
			std::vector<std::pair<uint32, uint8>> data;
			bool success = true;
			float xScale = (float)width/(float)newWidth, yScale = (float)height/(float)newHeight;
			data.resize(newWidth * newHeight);
			
			switch (m)
			{
				default:
				case InterpolationMethod::NearestNeighbor:
				{
					uint32 srcX, srcY;
					for (uint32 y = 0; y < newHeight; ++y)
					{
						for (uint32 x = 0; x < newWidth; ++x)
						{
							srcX = x*xScale;
							srcY = y*yScale;
							data[(y*newWidth)+x] = pixels[(srcY*width)+srcX];
						}
					}
				}
				break;
				
				case InterpolationMethod::Bilinear:
				{
					float srcX, srcY;
					for (uint32 y = 0; y < newHeight; ++y)
					{
						for (uint32 x = 0; x < newWidth; ++x)
						{
							srcX = x*xScale;
							srcX /= width;
							srcY = y*yScale;
							srcY /= height;
							data[(y*newWidth)+x] = getPixel(srcX, srcY, InterpolationMethod::Bilinear, ExtrapolationMethod::Extend);
						}
					}
				}
				break;
				
				case InterpolationMethod::AreaAveraging:
				{
					if ((success = (xScale > 1.f && yScale > 1.f)))
					{
						uint16 pixW = xScale, pixH = yScale;
						uint16 _w = pixW/2, _h = pixH/2;
						pixW = std::max<uint16>(pixW, 2);
						pixH = std::max<uint16>(pixH, 2);
						uint32 avR, avG, avB, avA;
						float alphaRatio;
						uint32 srcX, srcY;
						uint16 div = pixW*pixH;
						
						for (uint32 y = 0; y < newHeight; ++y)
						{
							for (uint32 x = 0; x < newWidth; ++x)
							{
								srcX = x*xScale;
								srcY = y*yScale;
								avR = 0, avG = 0, avB = 0, avA = 0;
								
								for (uint32 w = 0; w < pixW; ++w)
								{
									for (uint32 h = 0; h < pixH; ++h)
									{
										if (getPixel(srcX+w-_w, srcY+h-_h) != nullptr)
										{
											alphaRatio = pixels[((srcY+h-_h)*width)+(srcX+w-_w)].second/255.f;
											avR += GetBValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avG += GetGValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avB += GetRValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avA += pixels[((srcY+h-_h)*width)+(srcX+w-_w)].second;
										} else {
											alphaRatio = pixels[(srcY*width)+srcX].second/255.f;
											avR += GetBValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avG += GetGValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avB += GetRValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avA += pixels[(srcY*width)+srcX].second;
										}
									}
								}
								
								avR /= div, avG /= div, avB /= div, avA /= div;
								data[(y*newWidth)+x] = std::make_pair(RGB(avB, avG, avR), avA);
							}
						}
						
					} else data = this->pixels;
				}
				break;
			}
			if (success)
			{
				width = newWidth;
				height = newHeight;
			} else {
				#ifdef CG_DEBUG
					std::cerr<<"CGIMG ERROR {this->resizeData()}: New image size greater than original ["<<width<<char(158)<<height<<" -> "<<newWidth<<char(158)<<newHeight<<", InterpolationMethod::AreaAveraging]"<<std::endl;
				#endif
			}
			return data;
		}
		
		void getHWNDSize(const HWND &hwnd, uint32 &width, uint32 &height)
		{
			RECT desktop;
			GetWindowRect(hwnd, &desktop);
			width = desktop.right - desktop.left, height = desktop.bottom - desktop.top;
			return;
		}
		
		//This doesn't work properly (header I think??)
		/*void saveBMPFile(std::string fileName, uint32 ver)
		{
			std::ofstream writeFile(fileName, std::ios::binary);
			BMPHeader header;
			uint8 p = 0;
			header.width = width;
			header.height = -height;
			header.fileSize += 3*(width+header.calcPaddingSize())*height;
			header.dataSize = header.fileSize-54;

			uint8 *headerInfo = header.getHeaderData();
			writeFile.write((char*)headerInfo, 54);
			delete[] headerInfo;
			uint32 paddingSize = header.calcPaddingSize();
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					writeFile.write((char*)&accessPixel(x, y)->first, sizeof(uint8)*3);
				}
				
				for (uint32 i = 0; i < paddingSize; ++i)
				{
					writeFile.write((char*)&p, sizeof(uint8));
				}
			}
			writeFile.close();
			return;
		}*/
		
		uint32 calcBMPPaddingSize(uint32 width){return (4 - (width * 3) % 4) % 4;
		}

	public:
		Image()
		{
			width = 256;
			height = 256;
			aspectRatio = 1.f;
			pixels.resize(width * height);
			memset(pixels.data(), 0, pixels.size()*sizeof(std::pair<uint32, uint8>));
		}
		Image(uint32 width, uint32 height, uint32 rgb = 0, uint8 a = 255)
		{
			this->width = width;
			this->height = height;
			aspectRatio = (float)width/(float)height;
			pixels.resize(width * height);
			for (uint32 y = 0; y<height; ++y)
			{
				for (uint32 x = 0; x<width; ++x)
				{
					pixels[(y * width)+x] = std::make_pair(rgb, a);
				}
			}
		}
		
		//Load image from memory
		Image(const uint32 *data, uint32 width, uint32 height, bool alpha = false)
		{
			loadImageFromArray(data, width, height, alpha);
		}
		//Load image from memory
		Image(const std::pair<uint32, uint8> *data, uint32 width, uint32 height)
		{
			loadImageFromArray(data, width, height);
		}
		
		//Load image from file
		Image(const std::string fileName)
		{
			loadImage(fileName);
		}
		
		//Copy constructor
		Image(const Image &image)
		{
			width = image.getWidth();
			height = image.getHeight();
			aspectRatio = (float)width/(float)height;
			x = image.getPosX();
			y = image.getPosY();
			
			pixels.resize(width * height);
			memcpy(&pixels[0], image.getPixelData(), width*height*sizeof(std::pair<uint32, uint8>));
		}
		Image& operator=(const Image &image)
		{
			if (&image != this)
			{
				width = image.getWidth();
				height = image.getHeight();
				aspectRatio = (float)width/(float)height;
				x = image.getPosX();
				y = image.getPosY();
				
				pixels.resize(width * height);
				memcpy(&pixels[0], image.getPixelData(), width * height * sizeof(std::pair<uint32, uint8>));
			}
			return *this;
		}
		
		std::pair<uint32, uint8> *operator[](uint32 i)
		{
			if (i < width * height){return &pixels[i];
			} else return nullptr;
		}
		
		//Loads an image from the disk (currently only supports 24 and 32 bit BMP files)
		bool loadImage(const std::string fileName)
		{
			std::string _fileName = fileName;
			std::transform(_fileName.begin(), _fileName.end(), _fileName.begin(), [](char c)->char{return toupper(c);});
			if (!pixels.empty()){pixels.clear();
			}
			
			if (_fileName.find(".BMP") != std::string::npos)
			{
				std::ifstream readfile(fileName, std::ios::binary);
				if (!readfile.is_open())
				{
					#ifdef CG_DEBUG
						std::cerr << "CGIMG ERROR {this->loadImage()}: Failed to read file ["<< fileName << ", errno=" << errno << "]" << std::endl;
					#endif
					return false;
				}

				//Read header
				std::vector<uint8> header(14);
				readfile.read((char*)&header[0], 14 * sizeof(uint8));
				uint32 fileSize = *(uint32*)&header[0x02];
				uint32 imgDataOffset = *(uint32*)&header[0x0A];
				uint32 dibHeaderSize = imgDataOffset - header.size();
				header.resize(imgDataOffset);
				readfile.read((char*)&header[14], dibHeaderSize * sizeof(uint8));

				int32 width = *(int32*)&header[0x12];
				int32 height = *(int32*)&header[0x16];

				uint16 bitesPerPixel = *(uint16*)&header[0x1C];
				uint16 bytesPerPixel = bitesPerPixel / 8;
				if (bytesPerPixel < 1){bytesPerPixel = 1;
				}

				//Check if valid before loading
				if (header[0] != 'B' || header[1] != 'M')
				{
					#ifdef CG_DEBUG
						std::cerr << "CGIMG ERROR {this->loadImage()}: Invalid BMP magic number [" << fileName << "]" << std::endl;
					#endif
					return false;
				}

				if (bytesPerPixel < 3)
				{
					#ifdef CG_DEBUG
						std::cerr << "CGIMG ERROR {this->loadImage()}: Currently only supports 24 or 32 bit BMP files [" << fileName << "]" std::endl;
					#endif
					return false;
				}

				this->width = width;
				this->height = abs(height);
				aspectRatio = (float)this->width / (float)this->height;
				pixels.resize(this->width * this->height);

				const uint32 rowSize = width * bytesPerPixel, paddingSize = (4 - (width % 4)) % 4;
				std::vector<uint8> imgData(fileSize - imgDataOffset);
				readfile.read(reinterpret_cast<char*>(&imgData[0]), imgData.size() * sizeof(uint8));

				for (uint32 y = 0; y < this->height; ++y)
				{
					for (uint32 x = 0; x < width; ++x)
					{
						uint32 index = ((rowSize + paddingSize) * y) + (x * bytesPerPixel);
						uint8 red = imgData[index + 2];
						uint8 green = imgData[index + 1];
						uint8 blue = imgData[index];
						uint8 alpha = (bytesPerPixel == 3 ? 255 : imgData[index + 3]);
						pixels[(y * this->width) + x] = std::make_pair(cg::BGR(red, green, blue), alpha);
					}
				}

				readfile.close();
				if (height > 0){flipVertically();
				}
			} else return false;
			return true;
		}

		//Loads image from memory, format = 0xAARRGGBB
		void loadImageFromArray(const uint32 *arr, uint32 width, uint32 height, bool alpha = false)
		{
			pixels.resize(width * height);
			this->width = width; 
			this->height = height;
			this->aspectRatio = (float)width/(float)height;
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					const uint32 &pixel = arr[(y*width)+x];
					uint32 rgb = cg::BGR(cg::GetR(pixel), cg::GetG(pixel), cg::GetB(pixel));
					uint8 a = alpha ? cg::GetA(pixel) : 255;
					pixels[(y*width)+x] = std::make_pair(rgb, a);
				}
			}
			return;
		}
		//Loads image from memory, format = {0x00RRGGBB, 0xAA}
		void loadImageFromArray(const std::pair<uint32, uint8> *arr, uint32 width, uint32 height)
		{
			pixels.resize(width * height);
			this->width = width; 
			this->height = height;
			this->aspectRatio = (float)width/(float)height;
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					const uint32 &rgb = arr[(y*width)+x].first;
					const uint8 &a = arr[(y*width)+x].second;
					pixels[(y*width)+x] = std::make_pair(cg::BGR(cg::GetR(rgb), cg::GetG(rgb), cg::GetB(rgb)), a);
				}
			}
			return;
		}
		
		//Will reimplement later
		/*void saveImage(const std::string fileName, uint32 ver)
		{
			std::string _fileName = fileName;
			std::transform(_fileName.begin(), _fileName.end(), _fileName.begin(), [](char c)->char{return toupper(c);});
			
			if (_fileName.find(".BMP") != std::string::npos)
			{
				saveBMPFile(fileName, ver);
			}
			return;
		}*/
		
		//FilterType::Invert = Invert all colours
		//FilterType::Custom = Custom (pass in a function pointer, or lamda)
		//Applies a function to all pixels in the image, funcData is not required
		void filter(FilterType filterType, void (*funcPtr)(std::pair<uint32, uint8>*, void*) = nullptr, void *funcData = nullptr)
		{
			switch (filterType)
			{
				default:
				case FilterType::Grayscale:
				{
					uint8 r, g, b;
					uint16 c;
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						r = GetBValue(pixels[i].first);
						g = GetGValue(pixels[i].first);
						b = GetRValue(pixels[i].first);
						c = (r+g+b)/3;
						pixels[i].first = cg::BGR(c, c, c);
					}
				}
				break;
				
				case FilterType::WeightedGrayscale:
				{
					uint8 r, g, b, c;
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						r = GetBValue(pixels[i].first);
						g = GetGValue(pixels[i].first);
						b = GetRValue(pixels[i].first);
						c = (r*0.3f)+(g*0.59f)+(b*0.11f);
						pixels[i].first = cg::BGR(c, c, c);
					}
				}
				break;
				
				case FilterType::Invert:
				{
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						pixels[i].first = pixels[i].first ^ 0xFFFFFF;
					}
				}
				break;
				
				case FilterType::Custom:
				{
					for (uint32 i = 0; i<pixels.size(); ++i)
					{
						funcPtr(&pixels[i], funcData);
					}
				}
				break;
			}
			return;
		}
		
		//Returns the width of the image
		uint32 getWidth(void) const {return width;
		}
		//Returns the height of the image
		uint32 getHeight(void) const {return height;
		}
		
		//Sets the position of where the image will be drawn to (co-ordinates can't be negative)
		void setPos(uint32 x, uint32 y)
		{
			this->x = x;
			this->y = y;
			return;
		}
		//Moves the position of the image (co-ordinates can't be negative)
		void move(uint32 x, uint32 y)
		{
			this->x += x;
			this->y += y;
			return;
		}
		
		//Returns X co-ordinate of the image
		uint32 getPosX(void) const {return x;
		}
		//Returns Y co-ordinate of the image
		uint32 getPosY(void) const {return y;
		}
		
		//Flips an image on the X-axis
		void flipVertically(void)
		{
			uint32 halfHeight = height/2;
			for (uint32 y = 0; y < halfHeight; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					std::swap(pixels[(y * width)+x], pixels[((height-y-1)*width)+x]);
				}
			}
			return;
		}
		//Flips an image on the Y-axis
		void flipHorizontally(void)
		{
			uint32 halfWidth = width/2;
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < halfWidth; ++x)
				{
					std::swap(pixels[(y * width)+x], pixels[(y * width)+(width-x-1)]);
				}
			}
			return;
		}
		
		//Return a read only pointer to pixel array
		const std::pair<uint32, uint8> *getPixelData(void) const {return this->pixels.data();
		}
		
		//If either newWidth or newHeight == 0, the image's aspect ratio is maintained
		//Resizes image to specified dimensions using chosen interpolation method
		void resize(uint32 newWidth, uint32 newHeight, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (newWidth == 0)
			{
				pixels = resizeData(pixels, newHeight * aspectRatio, newHeight, m);
				width = newHeight * aspectRatio;
				height = newHeight;
			} else if (newHeight == 0)
			{
				pixels = resizeData(pixels, newWidth, newWidth / aspectRatio, m);
				width = newWidth;
				height = newWidth / aspectRatio;
			} else {
				pixels = resizeData(pixels, newWidth, newHeight, m);
				width = newWidth;
				height = newHeight;
				aspectRatio = (float)width/(float)height;
			}
			return;
		}

		//Resizes image by a scale factor using a chosen interpolation method, aspect ratio is maintained
		void scale(float s, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (s > 0.f)
			{
				resize(width * s, height * s, m);
			}
			return;
		}

		//Resizes image by a scale factor in each axis using a chosen interpolation method, aspect can be changed
		void scale(float sx, float sy, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (sx > 0.f && sy > 0.f)
			{
				resize(width * sx, height * sy, m);
			}
			return;
		}
		
//		//Returns the first element of the array because references don't have a "nullptr" equivilant
//		std::pair<unsigned long int, unsigned char> &getPixel(unsigned long int x, unsigned long int y)
//		{
//			if (x < width && y < height){return pixels[(y * width)+x];
//			} else return pixels[0];
//		}

		//Returns a pointer to a pixel without checking if it exists 
		std::pair<uint32, uint8>* accessPixel(uint32 x, uint32 y){return &pixels[(y * width) + x];
		}

		//Returns a pointer to a pixel, if pixel doesn't exist "nullptr" will be returned
		std::pair<uint32, uint8> *getPixel(uint32 x, uint32 y)
		{
			if (x < width && y < height){return &pixels[(y * width)+x];
			} else return nullptr;
		}

		//Returns a copy of pixel at a point
		std::pair<uint32, uint8> getPixel(float x, float y, InterpolationMethod im = InterpolationMethod::NearestNeighbor, ExtrapolationMethod em = ExtrapolationMethod::Repeat) const
		{
			bool n = false, xNeg = (x < 0.f), yNeg = (y < 0.f);
			uint32 ix = width-1, iy = height-1;
			if (x > 1.f || xNeg)
			{
				switch (em)
				{
					default:
					case ExtrapolationMethod::None:
						n = true;
						break;
					
					case ExtrapolationMethod::Repeat:
						x = fabs(x) - floor(fabs(x));
						x = xNeg ? 1.f-x :x;
						ix = floor(x * (width-1));
						break;
					
					case ExtrapolationMethod::Extend:
						x = std::max(std::min(1.f, x), 0.f);
						ix = floor(x * (width-1));
						break;
				}
			} else ix = floor(x * (width-1));
			if (y > 1.f || yNeg)
			{
				switch (em)
				{
					default:
					case ExtrapolationMethod::None:
						n = true;
						break;
					
					case ExtrapolationMethod::Repeat:
						y = fabs(y) - floor(fabs(y));
						y = yNeg ? 1.f-y : y;
						iy = floor(y * (height-1));
						break;
					
					case ExtrapolationMethod::Extend:
						y = std::max(std::min(1.f, y), 0.f);
						iy = floor(y * (height-1));
						break;
				}
			} else iy = floor(y * (height-1));
			
			std::pair<uint32, uint8> pixel;
			
			if (!n)
			{
				float invX = 1.f-x;
				float invY = 1.f-y;
				
				switch (im)
				{
					default:
					case InterpolationMethod::NearestNeighbor:
					case InterpolationMethod::None:
						pixel = pixels[(iy * width) + ix];
						break;
					
					case InterpolationMethod::Bilinear:
					{
						//Convert to intergers and round down
						
						uint8 r[] = {0x00, 0x00, 0x00, 0x00};
						uint8 g[] = {0x00, 0x00, 0x00, 0x00};
						uint8 b[] = {0x00, 0x00, 0x00, 0x00};
						uint8 a[] = {0xFF, 0xFF, 0xFF, 0xFF};
						
						r[0] = (cg::GetR(pixels[(iy * width)+ix].first) * invX);
						g[0] = (cg::GetG(pixels[(iy * width)+ix].first) * invX);
						b[0] = (cg::GetB(pixels[(iy * width)+ix].first) * invX);
						a[0] = pixels[(iy * width)+ix].second * invX;
						
						if (ix + 1 > width-1)
						{
							switch (em)
							{
								default:
								case ExtrapolationMethod::None:
									r[1] = 0;
									g[1] = 0;
									b[1] = 0;
									a[1] = 0;
									break;
								
								case ExtrapolationMethod::Repeat:
									r[1] = cg::GetR(pixels[iy * width].first);
									g[1] = cg::GetG(pixels[iy * width].first);
									b[1] = cg::GetB(pixels[iy * width].first);
									a[1] = pixels[iy * width].second;
									break;
								
								case ExtrapolationMethod::Extend:
									r[1] = cg::GetR(pixels[(iy * width)+width-1].first);
									g[1] = cg::GetG(pixels[(iy * width)+width-1].first);
									b[1] = cg::GetB(pixels[(iy * width)+width-1].first);
									a[1] = pixels[(iy * width)+width-1].second;
									break;
							}
						} else {
							r[1] = cg::GetR(pixels[(iy * width)+(ix+1)].first);
							g[1] = cg::GetG(pixels[(iy * width)+(ix+1)].first);
							b[1] = cg::GetB(pixels[(iy * width)+(ix+1)].first);
							a[1] = pixels[(iy * width)+(ix+1)].second;
						}
						r[1] *= x;
						g[1] *= x;
						b[1] *= x;
						a[1] *= x;
						
						if (iy + 1 > height-1)
						{
							switch (em)
							{
								default:
								case ExtrapolationMethod::None:
									r[2] = 0;
									g[2] = 0;
									b[2] = 0;
									a[2] = 0;
									break;
								
								case ExtrapolationMethod::Repeat:
									r[2] = cg::GetR(pixels[ix].first);
									g[2] = cg::GetG(pixels[ix].first);
									b[2] = cg::GetB(pixels[ix].first);
									a[2] = pixels[ix].second;
									break;
								
								case ExtrapolationMethod::Extend:
									r[2] = cg::GetR(pixels[((height-1) * width)+ix].first);
									g[2] = cg::GetG(pixels[((height-1) * width)+ix].first);
									b[2] = cg::GetB(pixels[((height-1) * width)+ix].first);
									a[2] = pixels[((height-1) * width)+ix].second;
									break;
							}
						} else {
							r[2] = cg::GetR(pixels[(iy * width)+(ix+1)].first);
							g[2] = cg::GetG(pixels[(iy * width)+(ix+1)].first);
							b[2] = cg::GetB(pixels[(iy * width)+(ix+1)].first);
							a[2] = pixels[(iy * width)+(ix+1)].second;
						}
						r[2] *= invX;
						g[2] *= invX;
						b[2] *= invX;
						a[2] *= invX;
						
						uint8 k = (iy + 1 > height-1 ? 0 : 1)+(ix + 1 > width-1 ? 2 : 4);
						
						switch (k)
						{
							case 5: //Both in bounds
								r[3] = cg::GetR(pixels[((iy+1) * width)+(ix+1)].first);
								g[3] = cg::GetG(pixels[((iy+1) * width)+(ix+1)].first);
								b[3] = cg::GetB(pixels[((iy+1) * width)+(ix+1)].first);
								a[3] = pixels[((iy+1) * width)+(ix+1)].second;
								break;
								
							case 3: //x IN, y OUT
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[ix].first);
										g[3] = cg::GetG(pixels[ix].first);
										b[3] = cg::GetB(pixels[ix].first);
										a[3] = pixels[ix].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[((height-1) * width)+ix].first);
										g[3] = cg::GetG(pixels[((height-1) * width)+ix].first);
										b[3] = cg::GetB(pixels[((height-1) * width)+ix].first);
										a[3] = pixels[((height-1) * width)+ix].second;
										break;
								}
								break;
								
							case 4: //x OUT, y IN
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[iy * width].first);
										g[3] = cg::GetG(pixels[iy * width].first);
										b[3] = cg::GetB(pixels[iy * width].first);
										a[3] = pixels[iy * width].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[(iy * width)+width-1].first);
										g[3] = cg::GetG(pixels[(iy * width)+width-1].first);
										b[3] = cg::GetB(pixels[(iy * width)+width-1].first);
										a[3] = pixels[(iy * width)+width-1].second;
										break;
								}
								break;
								
							case 2: //Both out of bounds
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[0].first);
										g[3] = cg::GetG(pixels[0].first);
										b[3] = cg::GetB(pixels[0].first);
										a[3] = pixels[0].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[((height-1) * width)+(width-1)].first);
										g[3] = cg::GetG(pixels[((height-1) * width)+(width-1)].first);
										b[3] = cg::GetB(pixels[((height-1) * width)+(width-1)].first);
										a[3] = pixels[((height-1) * width)+(width-1)].second;
										break;
								}
								break;
						}
						r[3] *= x;
						g[3] *= x;
						b[3] *= x;
						a[3] *= x;
						
						uint8 finalR = ((r[0]+r[1]) * invY) + ((r[2]+r[3]) * y);
						uint8 finalG = ((g[0]+g[1]) * invY) + ((g[2]+g[3]) * y);
						uint8 finalB = ((b[0]+b[1]) * invY) + ((b[2]+b[3]) * y);
						uint8 finalA = ((a[0]+a[1]) * invY) + ((a[2]+a[3]) * y);
						
						pixel = std::make_pair(cg::BGR(finalR, finalG, finalB), finalA);
					}
					break;
				}
			} else pixel = std::make_pair(0, 0);
			
			return pixel;
		}
		
		void setPixel(uint32 x, uint32 y, uint32 rgb, uint8 a = 255)
		{
			if (x < width && y < height)
			{
				pixels[(y * width)+x] = std::make_pair(rgb, a);
			}
			return;
		}
		
		//Sets all alpha values in the image
		void setAlpha(uint8 a)
		{
			for (uint32 i = 0; i < pixels.size(); ++i)
			{
				pixels[i].second = a;
			}
			return;
		}

		//Replaces the alpha value of all pixels with certain colour
		void setColourToAlpha(uint32 rgb, uint8 a = 0)
		{
			for (uint32 i = 0; i < pixels.size(); ++i)
			{
				if (pixels[i].first == rgb){pixels[i].second = a;
				}
			}
			return;
		}
		
		//Change the image's dimensions (does not resize colour data)
		void setSize(uint32 newWidth, uint32 newHeight, bool clearData = false)
		{
			auto temp = pixels;
			pixels.clear();
			pixels.resize(newWidth * newHeight);
			if (!clearData)
			{
				for (uint32 y = 0; y < height; y++)
				{
					for (uint32 x = 0; x < width; x++)
					{
						pixels[(y * newWidth)+x] = temp[(y * width)+x];
					}
				}
			}
			width = newWidth;
			height = newHeight;
			aspectRatio = (float)width/(float)height;
			return;
		}

		//Copy a section from a section of an image, to another
		void copy(Image &image, uint32 dstX, uint32 dstY, uint32 srcX, uint32 srcY, uint32 width, uint32 height, bool alpha = false)
		{
			for (uint32 iy = 0; iy < height; iy++)
			{
				for (uint32 ix = 0; ix < width; ix++)
				{
					if (getPixel(ix+dstX, iy+dstY) != nullptr && image.getPixel(ix+srcX, iy+srcY) != nullptr)
					{
//						*getPixel(ix+dstX, iy+dstY) = std::make_pair(image.getPixel(ix+srcX, iy+srcY)->first, alpha ? image.getPixel(ix+srcX, iy+srcY)->second : 255);
						pixels[((iy+dstY)*this->width)+(ix+dstX)] = std::make_pair(image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)].first, alpha ? image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)].second : 255);
					}
				}
			}
			return;
		}

		//Draws a section from an section, to another
		void blendImage(Image &image, uint32 dstX, uint32 dstY, uint32 srcX, uint32 srcY, uint32 width, uint32 height, bool keepAlpha = true, bool mask = true)
		{
			uint8 tempA;
			for (uint32 iy = 0; iy < height; iy++)
			{
				for (uint32 ix = 0; ix < width; ix++)
				{
					if (getPixel(ix+dstX, iy+dstY) != nullptr && image.getPixel(ix+srcX, iy+srcY) != nullptr)
					{
						tempA = pixels[((iy+dstY)*this->width)+(ix+dstX)].second;
						std::pair<uint32, uint8> srcRGB = image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)], &dstRGB = pixels[((iy+dstY)*this->width)+(ix+dstX)];
						dstRGB.first = blendPixel(dstRGB.first, srcRGB.first, srcRGB.second);
						dstRGB.second = keepAlpha ? dstRGB.second : srcRGB.second;
						if (mask && srcRGB.second == 0){dstRGB.second = tempA;
						}
					}
				}
			}
			return;
		}
		
		//Needs to be rewritten
		void getHWNDBitmap(HWND handle)
		{
			HDC hdc = GetDC(handle);
			getHWNDSize(handle, width, height);
			
    		HDC tempDC  = CreateCompatibleDC(hdc);
    		HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    		HGDIOBJ lastObj = SelectObject(tempDC, bitmap); 
    		BitBlt(tempDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY); 
		
		    SelectObject(tempDC, lastObj); // always select the previously selected object once done
		    DeleteDC(tempDC);
		    
		    BITMAPINFO _bitmapInfo;
		    memset(&_bitmapInfo, 0, sizeof(BITMAPINFO));
    		_bitmapInfo.bmiHeader.biSize = sizeof(_bitmapInfo.bmiHeader); 
		
		    // Get the BITMAPINFO structure from the bitmap
		    if (GetDIBits(hdc, bitmap, 0, 0, NULL, &_bitmapInfo, DIB_RGB_COLORS) == 0) {
		        std::cout << "error" << std::endl;
		        return;
		    }
		    
		    _bitmapInfo.bmiHeader.biCompression = BI_RGB;
		    _bitmapInfo.bmiHeader.biHeight = -_bitmapInfo.bmiHeader.biHeight;
		    _bitmapInfo.bmiHeader.biWidth = width;
		    _bitmapInfo.bmiHeader.biBitCount = 8*4;
			_bitmapInfo.bmiHeader.biPlanes = 1;
			_bitmapInfo.bmiHeader.biCompression = BI_RGB;
//		    _bitmapInfo.bmiHeader.biHeight = BI_RGB;
//		    _bitmapInfo.bmiHeader.biBitCount = 24;
			pixels.resize((_bitmapInfo.bmiHeader.biWidth * -_bitmapInfo.bmiHeader.biHeight) + 1);
			width = _bitmapInfo.bmiHeader.biWidth;
			height = -_bitmapInfo.bmiHeader.biHeight;
		
    		// get the actual bitmap buffer
    		if (GetDIBits(hdc, bitmap, 0, -_bitmapInfo.bmiHeader.biHeight, &pixels[0], &_bitmapInfo, DIB_RGB_COLORS) == 0) {
    		    std::cout << "error2" << std::endl;
    		    return;
    		}
//    		setSize(width/2, height/2);
    		
    		DeleteObject(bitmap);
		}
};

struct Size
{
	uint32 width, height;
	
	Size()
	{
		width = 0, height = 0;
	}
	Size(uint32 width, uint32 height)
	{
		this->width = width;
		this->height = height;
	}
};

struct SubImageData
{
	Size size;
	uint32 srcX, srcY, dstX, dstY;
	bool isVertical, transparency;
	
	SubImageData()
	{
		srcX = 0, srcY = 0;
		dstX = 0, dstY = 0;
		isVertical = false;
		transparency = false;
	}
	SubImageData(uint32 srcX, uint32 srcY, uint32 dstX, uint32 dstY, uint32 width, uint32 height, bool isVertical, bool transparency = false)
	{
		this->srcX = srcX;
		this->srcY = srcY;
		this->dstX = dstX;
		this->dstY = dstY;
		size.width = width;
		size.height = height;
		this->isVertical = isVertical;
		this->transparency = transparency;
	}
};

//Class for animated images
class AniImage
{
	Image image, dstImage;
	bool cycleMethod, dir, finished = false, firstFrame = true, firstFrameDrawn = true;
	uint8 ver, currentCycle = 0, maxCycles = 0;
	uint32 currentFrame = 0, maxFrames;
	uint32 frameTime, currentFrameTime = 0, currentTime = 0;
	std::vector<SubImageData> subImageData;
	
	protected:
		void load(std::string fileName)
		{
			if (fileName.find(".gif") != std::string::npos || fileName.find(".GIF") != std::string::npos)
			{
				#ifdef CG_DEBUG
					std::cerr<<"Cannot open:"<<fileName<<std::endl;
					std::cerr<<"GIF file loading has not been implemented yet."<<std::endl;
				#endif
				return;
			}
			
			std::ifstream readFile(fileName.c_str(), std::ios::binary);
			if (readFile.is_open())
			{
				readFile.read(reinterpret_cast<char*>(&ver), sizeof(uint8)); //Get version
				Size tempSize;
				readFile.read(reinterpret_cast<char*>(&tempSize.width), sizeof(uint32)); //Get main image dimensions
				readFile.read(reinterpret_cast<char*>(&tempSize.height), sizeof(uint32));
				dstImage.setSize(tempSize.width, tempSize.height, true);
				readFile.read(reinterpret_cast<char*>(&tempSize.width), sizeof(uint32)); //Get total image dimensions
				readFile.read(reinterpret_cast<char*>(&tempSize.height), sizeof(uint32));
				image.setSize(tempSize.width, tempSize.height, true);
				
				//Get Image data
				std::pair<uint32, uint8> pixel;
				uint8 c;
				uint32 size = image.getWidth()*image.getHeight();
				switch (ver)
				{
					case 1:
					{
						for (uint32 i = 0; i < size; ++i)
						{
							readFile.read(reinterpret_cast<char*>(&pixel), sizeof(std::pair<uint32, uint8>));
							*image[i] = pixel;
						}
					}
					break;
					
					default:
					case 2:
					{
						uint32 i = 0;
						while (i < size)
						{
							readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
							readFile.read(reinterpret_cast<char*>(&pixel), sizeof(std::pair<uint32, uint8>));
							
							while (c != 0)
							{
								*image[i] = pixel;
								++i;
								--c;
							}
						}
					}
				}
				
				maxFrames = 0;
				if (ver == 1)
				{
					readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
					maxFrames = c;
				} else if (ver >= 2){readFile.read(reinterpret_cast<char*>(&maxFrames), sizeof(uint32));
				}
				readFile.read(reinterpret_cast<char*>(&frameTime), sizeof(uint32));
				readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
				
				switch (c)
				{
					default:
					case 0:
						cycleMethod = false;
						break;
					
					case 1:
						cycleMethod = true;
						break;
				}
				
				if (ver >= 2){readFile.read(reinterpret_cast<char*>(&maxCycles), sizeof(uint8));
				}
				
				uint8 s;
				readFile.read(reinterpret_cast<char*>(&s), sizeof(uint8));
				
				subImageData.clear();
				for (uint8 i = 0; i < s; ++i)
				{
					subImageData.push_back(SubImageData());
					readFile.read(reinterpret_cast<char*>(&subImageData.back().srcX), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().srcY), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().dstX), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().dstY), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().size.width), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().size.height), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
					subImageData.back().isVertical = (c == 1 ? true : false);
					if (ver >= 2)
					{
						readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
						subImageData.back().transparency = (c == 1 ? true : false);
					} else subImageData.back().transparency = false;
				}
				
				readFile.close();
				
				update(0.f);
				reset();
				
				#ifdef CG_DEBUG
					std::clog<<"Filename:\t"<<fileName<<std::endl;
					std::clog<<"Version:\t"<<(int)ver<<std::endl;
					std::clog<<"Main Image dimensions:\t"<<dstImage.getWidth()<<(char)158<<dstImage.getHeight()<<std::endl;
					std::clog<<"Image dimensions:\t"<<image.getWidth()<<(char)158<<image.getHeight()<<std::endl;
					std::clog<<"Expected data size:\t"<<image.getWidth()*image.getHeight()<<std::endl;
//					std::clog<<"Data size:\t"<<d.size()<<std::endl;
					std::clog<<"Number of frames:\t"<<maxFrames<<std::endl;
					std::clog<<"Frame time:\t"<<frameTime<<" ("<<(float)1.f/(frameTime/1000.f)<<" FPS)"<<std::endl;
					if (cycleMethod){std::clog<<"Cycle method:\tTRUE ("<<(int)c<<')'<<std::endl;
					} else std::clog<<"Cycle method:\tFALSE ("<<(int)c<<')'<<std::endl;
					std::clog<<"Cycles:\t"<<(int)maxCycles<<std::endl;
					std::cout<<"Number of sub images:\t"<<(int)s<<std::endl;
				#endif
			}
			return;
		}
		
		void play(void)
		{
			if (firstFrame){firstFrame = false;
			}
			if (cycleMethod)
			{
				if (dir)
				{
					if (currentFrame == maxFrames-1){dir = false;
					} else ++currentFrame;
				} else {
					if (currentFrame == 0){dir = true;
					} else --currentFrame;
				}
				if (!firstFrame && currentFrame == 0 && !dir){++currentCycle;
				}
			} else {
				if ((++currentFrame) == maxFrames-1){++currentCycle;
				}
				currentFrame %= maxFrames;
			}
			finished = (currentTime >= getAniTime() ? true : false);
		}
	public:
		AniImage()
		{
			ver = 0;
			maxFrames = 0;
			frameTime = 0;
			cycleMethod = false;
			dir = false;
		}
		AniImage(std::string fileName)
		{
			ver = 0;
			maxFrames = 0;
			frameTime = 0;
			cycleMethod = false;
			dir = false;
			load(fileName);
		}
		~AniImage()
		{
			
		}
		
		//Filename must end with .cgai
		void loadAniImage(std::string fileName)
		{
			load(fileName);
			return;
		}
		void unload(void)
		{
			
			return;
		}
		void update(float deltaTime) //Seconds
		{
			currentFrameTime += deltaTime*1000.f;
			currentTime += deltaTime*1000.f;
			if (currentFrameTime >= frameTime)
			{
				for (uint32 i = 0; i < (currentFrameTime / frameTime); ++i){play();
				}
				currentFrameTime = 0;
			}
			
			dstImage.copy(image, 0, 0, 0, 0, dstImage.getWidth(), dstImage.getHeight(), true);
			for (auto &data : subImageData)
			{
				if (!data.transparency || ver <= 1)
				{
					dstImage.copy(image, data.dstX, data.dstY, data.srcX + (data.isVertical ? 0 : (currentFrame*data.size.width)), data.srcY + (data.isVertical ? (currentFrame*data.size.height) : 0), data.size.width, data.size.height, true);
				} else dstImage.blendImage(image, data.dstX, data.dstY, data.srcX + (data.isVertical ? 0 : (currentFrame*data.size.width)), data.srcY + (data.isVertical ? (currentFrame*data.size.height) : 0), data.size.width, data.size.height, true);
			}
			return;
		}
		
		void setFrame(uint32 frame)
		{
			currentFrame = frame;
			currentFrame %= maxFrames;
			return;
		}
		uint32 getCurrentFrame(void){return currentFrame;
		}
		uint32 getMaxFrames(void){return maxFrames;
		}
		void advanceFrame(void)
		{
			++currentFrame;
			currentFrame %= maxFrames;
			return;
		}
		void reverseFrame(void)
		{
			--currentFrame;
			currentFrame %= maxFrames;
			return;
		}
		
		void setMaxCycles(uint8 cycles)
		{
			maxCycles = cycles;
			return;
		}
		uint8 getMaxCycles(void){return maxCycles;
		}
		uint8 getCurrentCycle(void){return currentCycle;
		}
		uint32 getCycleTime(void){return frameTime * maxFrames * (cycleMethod ? 2 : 1);
		}
		uint32 getAniTime(void){return frameTime * maxFrames * maxCycles * (cycleMethod ? 2 : 1);
		}
		uint32 getCurrentTime(void){return currentTime;
		}
		
		bool isFinished(void){return finished;
		}
		bool isFirstFrame(void){return firstFrame;
		}
		bool isFirstFrameDrawn(void){return firstFrameDrawn;
		}
		
		void reset(void)
		{
			firstFrame = true;
			firstFrameDrawn = true;
			currentFrame = 0;
			currentCycle = 0;
			finished = false;
			currentFrameTime = 0;
			currentTime = 0;
			dir = false;
			return;
		}
		
		void scale(float n)
		{
			image.scale(n);
			dstImage.scale(n);
			
			for (SubImageData &data : subImageData)
			{
				data.srcX *= n;
				data.srcY *= n;
				data.dstX *= n;
				data.dstY *= n;
				data.size.width *= n;
				data.size.height *= n;
			}
			return;
		}
		
		uint32 getWidth(void){return dstImage.getWidth();
		}
		uint32 getHeight(void){return dstImage.getHeight();
		}
		uint32 getPosX(void){return dstImage.getPosX();
		}
		uint32 getPosY(void){return dstImage.getPosY();
		}
		
		Image *getFinalImage(void){return &dstImage;
		}
		Image *getSourceImage(void){return &image;
		}
		
		void setPos(uint32 x, uint32 y)
		{
			dstImage.setPos(x, y);
			return;
		}
		void draw(ConsoleGraphics *graphics);
};

class Font
{
	uint32 charWidth, charHeight;
};

class Text
{
	Image *font = nullptr;
	std::string text;
	uint32 charWidth, charHeight;
	Image textImage;
	
	protected:
		void getTextSize(const std::string text, uint32 &width, uint32 &height)
		{
			std::vector<uint32> sizes;
			
			for (uint32 i = 0; i < text.size(); ++i)
			{
				if (text[i] == '\n')
				{
					sizes.push_back(width);
					width = 0;
					++height;
				} else ++width;
			}
			
			if (height != 1)
			{
				++sizes.front();
				for (uint32 i = 0; i < sizes.size(); ++i)
				{
					if (width < sizes[i]){width = sizes[i]-1;
					}
				}
			}
			width = std::max<uint32>(width, 1);
		}
	public:
		Text()
		{
			this->charWidth = 0;
			this->charHeight = 0;
		}
		Text(Image *fontImage, uint32 charWidth, uint32 charHeight, const std::string text)
		{
			this->font = fontImage;
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			this->text = text;
			setText(text);
		}
		~Text()
		{
			this->font = nullptr;
		}
		
		//Pass in image pointer with font loaded to save memory
		void setFont(Image *fontImage, uint32 charWidth, uint32 charHeight)
		{
			font = fontImage;
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			return;
		}
		void setCharSize(uint32 charWidth, uint32 charHeight)
		{
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			return;
		}
		void setPos(uint32 x, uint32 y)
		{
			textImage.setPos(x, y);
			return;
		}
		uint32 getPosX(void){return textImage.getPosX();
		}
		uint32 getPosY(void){return textImage.getPosY();
		}
		
		void setText(const std::string text, uint32 compX = 0, uint32 compY = 0)
		{
			this->text = text;
			uint32 textWidth = 0, textHeight = 1;
			getTextSize(text, textWidth, textHeight);
			textImage.setSize((textWidth*charWidth)-(compX*textWidth), (textHeight*charHeight)-(compY*textHeight), true);
			uint32 srcX = 0, srcY = 0, dstX = 0, dstY = 0;
			
			for (uint32 i = 0; i < text.size(); ++i)
			{
				uint8 c = text[i];
				srcX = c*charWidth;
				
				switch (c)
				{
					case '\n':
						dstX = 0;
						dstY++;
						break;
					
					default:
						textImage.copy(*font, (dstX*charWidth)-(compX*dstX), (dstY*charHeight)-(compY*dstY), srcX, 0, charWidth, charHeight, true);
						dstX++;
						break;
				}
			}
			return;
		}
		
		std::string getText(void){return text;
		}
		Image &getTextImage(void){return textImage;
		}
		Image *getFontImage(void){return font;
		}
		
		uint32 getCharWidth(void){return charWidth;
		}
		uint32 getCharHeight(void){return charHeight;
		}
		uint32 getWidth(void){return textImage.getWidth();
		}
		uint32 getHeight(void){return textImage.getHeight();
		}
};

enum class RenderMode{BitBlt, BitBltInv, SetPixel, SetPixelVer, SetPixelInv, SetPixelVerInv};
enum class DrawType{Repeat, Resize};

class ConsoleGraphics
{
	std::vector<uint32> pixels;
	uint32 width, height, startX, startY, consoleWidth, consoleHeight;
	RenderMode renderMode;
	HBITMAP pixelBitmap;
	HDC targetDC, tempDC;
	bool alphaMode;
	uint8 pixelSize;
	bool enableShaders;
	std::vector<void(*)(uint32*, uint32, uint32, uint32, uint32, void*)> shaderList;
	std::vector<void*> shaderDataList;
	std::string title;
	float outputScale = 1.f;
	//"shaderList" function struct
	//Arg 1 = Pointer to current pixel
	//Arg 2 = Current X
	//Arg 3 = Current Y
	//Arg 4 = Number of operations
	//Arg 5 = Extra data
	protected:
		void initialise(void)
		{
			targetDC = GetDC(GetConsoleWindow());
			tempDC = CreateCompatibleDC(targetDC);
			
			renderMode = RenderMode::BitBlt;
			startX = 0;
			startY = 0;
			pixelSize = 1;
			outputScale = 1.f;
			
			alphaMode = false;
			enableShaders = false;
			
			return;
		}
		
		//Improve this
		void removeScrollbars(void)
		{
			HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
  			CONSOLE_SCREEN_BUFFER_INFO csbi;
    		GetConsoleScreenBufferInfo(hstdout, &csbi);
			
    		csbi.dwSize.X -= 2;//= csbi.dwMaximumWindowSize.X;
    		csbi.dwSize.Y = csbi.dwMaximumWindowSize.Y;
    		SetConsoleScreenBufferSize(hstdout, csbi.dwSize);
			
    		HWND x = GetConsoleWindow();
    		ShowScrollBar(x, SB_BOTH, FALSE);
			return;
		}
		
		void getHWNDRes(const HWND &hwnd, uint32 &width, uint32 &height)
		{
			static RECT desktop;
			GetWindowRect(hwnd, &desktop);
			width = desktop.right - desktop.left, height = desktop.bottom - desktop.top;
			return;
		}
		
		void getConsoleRes(void)
		{
			getHWNDRes(GetConsoleWindow(), consoleWidth, consoleHeight);
			return;
		}
		
		void setConsoleRes(const uint32 width, const uint32 height)
		{
			HWND console = GetConsoleWindow();
			RECT window;
			GetWindowRect(console, &window);
			MoveWindow(console, window.left, window.top, width, height, true);
			removeScrollbars();
			return;
		}
		
		std::vector<uint32> resizeDataNearestNeighbor(std::vector<uint32> &pixels, uint32 newWidth, uint32 newHeight)
		{
			std::vector<uint32> data;
			float xScale = (float)width/(float)newWidth, yScale = (float)height/(float)newHeight;
			data.resize(newWidth * newHeight);
			
			uint32 srcX, srcY;
			for (uint32 y = 0; y < newHeight; ++y)
			{
				for (uint32 x = 0; x < newWidth; ++x)
				{
					srcX = x*xScale;
					srcY = y*yScale;
					data[(y*newWidth)+x] = pixels[(srcY*width)+srcX];
				}
			}
			
			width = newWidth;
			height = newHeight;
			return data;
		}
		
		uint32 &accessBuffer(uint32 x, uint32 y){return pixels[(y * width)+x];
		}
		uint32 &accessBuffer(uint32 index){return pixels[index];
		}
		
	public:
		ConsoleGraphics()
		{
			initialise();
			getConsoleRes();
			removeScrollbars();
			width = consoleWidth;
			height = consoleHeight;
			pixels.resize(width * height);
		}
		
		ConsoleGraphics(uint32 width, uint32 height, bool setSize = false, uint8 pixelSize = 1, bool pixelMode = false)
		{
			initialise();
			this->pixelSize = std::max<uint8>(pixelSize, 1);
			getConsoleRes();
			this->width = width / (!pixelMode ? 1 : pixelSize);
			this->height = height / (!pixelMode ? 1 : pixelSize);
			
			if (setSize || pixelMode)
			{
				/*OSVERSIONINFOA windowsVersion;
    			memset(&windowsVersion, 0, sizeof(OSVERSIONINFOA));
    			windowsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
			    GetVersionExA(&windowsVersion);*/
			    
			    unsigned char windowWidthOffset = 0, windowHeightOffset = 0;
			    /*float _windowsVersion = windowsVersion.dwMajorVersion + ((float)windowsVersion.dwMinorVersion / 10);
			    
				consoleWidth = (!pixelMode ? width : this->width) * pixelSize;
				consoleHeight = (!pixelMode ? height : this-> height) * pixelSize;
			    if (_windowsVersion >= 6.2f)
			    {
			    	windowWidthOffset = 15; //15
			    	windowHeightOffset = 39; //39
				} else if (_windowsVersion <= 6.1f)
				{
					float aspectRatio = (float)consoleWidth / (float)consoleHeight;
					if (consoleWidth > 800)
					{
						consoleWidth = 800;
						this->width = consoleWidth / pixelSize;
						
						consoleHeight = consoleWidth / aspectRatio;
						this->height = consoleHeight / pixelSize;
					}
				}*/
				
				setConsoleRes(consoleWidth+windowWidthOffset, consoleHeight+windowHeightOffset);
			}
			
			if (!pixelMode){pixels.resize(width * height);
			} else pixels.resize(this->width * this->height);
		}
		
		bool display(void)
		{
			bool returnValue;
			
			if (enableShaders)
			{
				for (uint8 i = 0; i < shaderList.size(); ++i)
				{
					for (uint32 y = 0; y < height; ++y)
					{
						for (uint32 x = 0; x < width; ++x)
						{
							shaderList[i](&pixels[(y * width)+x], width, height, x, y, shaderDataList[i]);
						}
					}
				}
			}
			
			switch (renderMode)
			{
				default:
				case RenderMode::BitBltInv:
				case RenderMode::BitBlt:
					pixelBitmap = CreateBitmap(width, height, 1, 8*4, &pixels[0]);
					if (returnValue = (pixelBitmap == NULL))
					{
						#ifdef CG_DEBUG
							std::cerr << "CGOUT ERROR {this->display()}: CreateBitmap() returned 'NULL' [GetLastError()=" << GetLastError() << "]" << std::endl;
						#endif

						return returnValue;
					}
					SelectObject(tempDC, pixelBitmap);
					returnValue = StretchBlt(targetDC, startX, startY, consoleWidth*outputScale, consoleHeight*outputScale, tempDC, 0, 0, width, height, renderMode == RenderMode::BitBlt ? SRCCOPY : NOTSRCCOPY);
					if (!returnValue)
					{
						#ifdef CG_DEBUG
							std::cerr << "CGOUT ERROR {this->display()}: StretchBlt() returned 'false' [GetLastError()=" << GetLastError() << "]" << std::endl;
						#endif

						return returnValue;
					}
					DeleteObject(pixelBitmap);
					break;
					
				case RenderMode::SetPixelInv:
				case RenderMode::SetPixel:
					for (uint32 y = 0; y < consoleHeight; ++y)
					{
						for (uint32 x = 0; x < consoleWidth; ++x)
						{
							uint32 &p = pixels[((y/pixelSize) * width)+(x/pixelSize)];
							SetPixelV(targetDC, x, y, renderMode == RenderMode::SetPixel ? RGB(GetBValue(p), GetGValue(p), GetRValue(p)) : RGB(GetBValue(p), GetGValue(p), GetRValue(p)) ^ 0x00ffffff); //^ 0x00ffffff << inverts colours
						}
					}
					break;
					
				case RenderMode::SetPixelVer:
				case RenderMode::SetPixelVerInv:
					for (uint32 x = 0; x < consoleWidth; ++x)
					{
						for (uint32 y = 0; y < consoleHeight; ++y)
						{
							uint32 &p = pixels[((y/pixelSize) * width)+(x/pixelSize)];
							SetPixelV(targetDC, x, y, renderMode == RenderMode::SetPixelVer ? RGB(GetBValue(p), GetGValue(p), GetRValue(p)) : RGB(GetBValue(p), GetGValue(p), GetRValue(p)) ^ 0x00ffffff);
						}
					}
					break;
			}
			return returnValue;
		}
		
		void setRenderMode(RenderMode mode)
		{
			renderMode = mode;
			return;
		}
		void enableAlpha(void)
		{
			this->alphaMode = true;
			return;
		}
		void disableAlpha(void)
		{
			this->alphaMode = false;
			return;
		}
		
		//Clear window with grayscale value
		void clear(uint8 c = 0x00)
		{
			memset(pixels.data(), c, pixels.size() * sizeof(uint32));
			return;
		}
		
		void setPixel(uint32 x, uint32 y, uint32 rgb)
		{
			if (x < width && y < height){pixels[(y * width)+x] = rgb;
			}
			return;
		}
		void drawPixel(uint32 x, uint32 y, uint32 rgb, uint8 a)
		{
			if (x < width && y < height && alphaMode)
			{
				pixels[(y * width)+x] = blendPixel(pixels[(y * width)+x], rgb, a);
			}
			return;
		}
		uint32 *getPixel(uint32 x, uint32 y)
		{
			if (x < width && y < height){return &pixels[(y * width)+x];
			} else return nullptr;
		}
		uint32 *accessPixel(uint32 x, uint32 y){return &pixels[(y * width)+x];
		}
		const uint32 *getPixelData(void) const {return &pixels[0];
		}
		
		uint32 getWidth(void){return width;
		}
		uint32 getHeight(void){return height;
		}
		
		uint32 getConsoleWidth(void){return consoleWidth;
		}
		uint32 getConsoleHeight(void){return consoleHeight;
		}
		
		uint8 getPixelSize(void){return pixelSize;
		}
		
		//Pixelizes the output already drawn to the screen (this is a very costly function, only use when nessesary)
		void pixelize(const float ratio)
		{
			if (!(ratio <= 1.f) && ratio < width && ratio < height)
			{
				pixels = resizeDataNearestNeighbor(pixels, width / ratio, height / ratio);
				pixels = resizeDataNearestNeighbor(pixels, consoleWidth / pixelSize, consoleHeight / pixelSize);
			}
			return;
		}
		
		void lineHorizontal(uint32 x, uint32 y, uint32 iterations, uint32 rgb, bool forward = true)
		{
			if (x < width && y < height && ((forward && x+iterations < width) || (!forward && x-iterations < width)))
			{
				for (uint32 i = 0; i < iterations; ++i)
				{
					accessBuffer(forward ? x+i : x-i, y) = rgb;
				}
			} else {
				for (uint32 i = 0; i < iterations; ++i)
				{
					if (x+i < width){accessBuffer(forward ? x+i : x-i, y) = rgb;
					} else break;
				}
			}
			return;
		}
		
		void lineVertical(uint32 x, uint32 y, uint32 iterations, uint32 rgb, bool forward = true)
		{
			if (x < width && y < height && ((forward && y+iterations < height) || (!forward && y-iterations < height)))
			{
				for (uint32 i = 0; i < iterations; ++i)
				{
					accessBuffer(x, forward ? y+i : y-i) = rgb;
				}
			} else {
				for (uint32 i = 0; i < iterations; ++i)
				{
					if (y+i < height){accessBuffer(x, forward ? y+i : y-i) = rgb;
					} else break;
				}
			}
			return;
		}
		
		void drawLine(uint32 x0, uint32 y0, uint32 x1, uint32 y1, uint32 rgb, uint8 alpha = 255)
		{
			int32 w, h, l;
			float dx, dy;
			uint32 x = x0, y = y0;
			
			w = x1-x0;
			h = y1-y0;
			l = ceil(sqrtf((w*w)+(h*h)));
			dx = (float)w/(float)l;
			dy = (float)h/(float)l;
			
			for (uint32 i = 0; i < l; ++i)
			{
				x = x0+(dx*i);
				y = y0+(dy*i);
				if (x < width && y < height)
				{
					switch (alpha)
					{
						case 255:
							accessBuffer(x, y) = rgb;
							break;
							
						default:
							accessBuffer(x, y) = blendPixel(accessBuffer(x, y), rgb, alpha);
							break;
					}
				} else break;
				
//				x += dx;
//				y += dy;
			}
			return;
		}
		
		void drawRect(uint32 x, uint32 y, uint32 width, uint32 height, uint32 rgb, bool fill = true)
		{
			if (fill)
			{
				for (uint32 dy = y; dy < y+height; ++dy)
				{
					for (uint32 dx = x; dx < x+width; ++dx)
					{
						if (dx < this->width && dy < this->height)
						{
							pixels[(dy * this->width)+dx] = rgb;
						}
					}
				}
			} else {
				lineHorizontal(x, y, width, rgb);
				lineVertical(x+width, y, height, rgb);
				lineHorizontal(x+width, y+height, width, rgb, false);
				lineVertical(x, y+height, height, rgb, false);
			}
			return;
		}
		void drawRectA(uint32 x, uint32 y, uint32 width, uint32 height, uint32 rgb, uint8 a)
		{
			for (uint32 dy = y; dy < y+height; ++dy)
			{
				for (uint32 dx = x; dx < x+width; ++dx)
				{
					if (dx < this->width && dy < this->height)
					{
						pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], rgb, a);
					}
				}
			}
			return;
		}
		
		//Enable Post-Processing shaders
		void enablePPShaders(void)
		{
			enableShaders = true;
			return;
		}
		//Disable Post-Processing shaders
		void disablePPShaders(void)
		{
			enableShaders = false;
			return;
		}
		//Load Post-Processing shaders for ConsoleGraphics to use
		void loadPPShader(void(*funcPtr)(uint32*, uint32, uint32, uint32, uint32, void*), void* shaderData = nullptr)
		{
			shaderList.push_back(funcPtr);
			shaderDataList.push_back(shaderData);
			return;
		}
		//Clear all Post-Processing shaders
		void clearPPShaders(void)
		{
			shaderList.clear();
			shaderDataList.clear();
			return;
		}
		
		void draw(Image &image)
		{
			for (uint32 y = 0; y < image.getHeight(); ++y)
			{
				for (uint32 x = 0; x < image.getWidth(); ++x)
				{
					uint32 dstX = x+image.getPosX(), dstY = y+image.getPosY();
					if (dstX < width && dstY < height) //Within bounds
					{
						if (image.getPixel(x, y)->second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
						{
							pixels[(dstY * width)+dstX] = image.accessPixel(x, y)->first;
						} else if (image.getPixel(x, y)->second != 0){ //Alpha is enabled
							pixels[(dstY * width)+dstX] = blendPixel(pixels[(dstY * width)+dstX], image.accessPixel(x, y)->first, image.accessPixel(x, y)->second);
						}
					} else if (dstY > height-1){return;
					} else if (dstX > width-1){break;
					}
				}
			}
			return;
		}
		void draw(Text &text)
		{
			this->draw(text.getTextImage());
			return;
		}
		//drawType = 0 - if width > image.width() or height > image.height(), the image will be tiled
		//drawType = 1 - if width > image.width() or height > image.height(), the image will be resized using nearest neighbor interpolation
		void drawEX(Image &image, uint32 srcX, uint32 srcY, uint32 dstX, uint32 dstY, uint32 width, uint32 height, DrawType drawType = DrawType::Repeat, void(*funcPtr)(std::pair<uint32, uint8>*, void*) = nullptr, void *funcData = nullptr)
		{
			uint32 x = srcX, y = srcY, dx = dstX, dy = dstY;
			
			if (drawType == DrawType::Repeat)
			{
				while (dy<dstY+height && dy<this->height)
				{
					while (dx<dstX+width && dx<this->width)
					{
						if (dx < this->width && dy < this->height) //Within bounds
						{
							auto pixel = image.getPixelData()[(y*image.getWidth())+x];
							if (funcPtr != nullptr){funcPtr(&pixel, funcData);
							}
							if (pixel.second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
							{
								pixels[(dy * this->width)+dx] = pixel.first;//image.getPixel(x, y)->first;
							} else if (pixel.second != 0){ //Alpha is enabled
								pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], pixel.first, pixel.second);
							}
						} else if (dy > this->height-1){return;
						} else if (dx > this->width-1){break;
						}
						if (x == image.getWidth()-1){x = 0;
						} else ++x;
						++dx;
					}
					x = srcX;
					dx = dstX;
					if (y == image.getHeight()-1){y = 0;
					} else ++y;
					++dy;
				}
			} else {
				if (srcX >= image.getWidth()){srcX %= image.getWidth();
				}
				if (srcY >= image.getHeight()){srcY %= image.getHeight();
				}
				float scaleX = (float)(image.getWidth()-srcX)/(float)width, scaleY = (float)(image.getHeight()-srcY)/(float)height;
				
				while (dy<dstY+height && dy<this->height)
				{
					while (dx<dstX+width && dx<this->width)
					{
						if (dx < this->width && dy < this->height) //Within bounds
						{
							x = srcX+((dx-dstX)*scaleX);
							x = std::max<int>(std::min<int>(x, image.getWidth()), 0);
							y = srcY+((dy-dstY)*scaleY);
							y = std::max<int>(std::min<int>(y, image.getHeight()), 0);
							auto pixel = image.getPixelData()[(y*image.getWidth())+x];
							if (funcPtr != nullptr){funcPtr(&pixel, funcData);
							}
							if (pixel.second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
							{
								pixels[(dy * this->width)+dx] = pixel.first;//image.getPixel(x, y)->first;
							} else if (pixel.second != 0){ //Alpha is enabled
								pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], pixel.first, pixel.second);
							}
						} else if (dy > this->height-1){return;
						} else if (dx > this->width-1){break;
						}
						++dx;
					}
					dx = dstX;
					++dy;
				}
			}
		}
		
		void setTitle(const std::string title)
		{
			this->title = title;
			SetConsoleTitleA(this->title.c_str());
			return;
		}
		std::string getTitle(void){return title;
		}
		
		void setOutputScale(float s)
		{
			outputScale = s;
			return;
		}
		float getOutputScale(void){return outputScale;
		}
		
		void setOutputPos(uint32 x, uint32 y)
		{
			startX = x;
			startY = y;
			return;
		}
		uint32 getOutputPosX(void){return startX;
		}
		uint32 getOutputPosY(void){return startY;
		}
		
		void setRenderTarget(HWND hwnd)
		{
			targetDC = GetDC(hwnd);
			tempDC = CreateCompatibleDC(targetDC);
			return;
		}
		void setRenderTarget(HDC hdc)
		{
			targetDC = hdc;
			tempDC = CreateCompatibleDC(targetDC);
			return;
		}
		HWND getRenderTarget(void){return WindowFromDC(targetDC);
		}
};

void AniImage::draw(ConsoleGraphics *graphics)
{
	if (firstFrameDrawn){firstFrameDrawn = false;
	}
	if (graphics != nullptr){graphics->draw(dstImage);
	}
	return;
}

#endif
