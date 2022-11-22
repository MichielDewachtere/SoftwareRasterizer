#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		//Load SDL_Surface using IMG_LOAD
		//Create & Return a new Texture Object (using SDL_Surface)

		return new Texture{ IMG_Load(path.c_str()) };
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		Uint8 r{ 0 }, g{ 0 }, b{ 0 };

		const int x = (int)(uv.x * m_pSurface->w);
		const int y = (int)(uv.y * m_pSurface->w);

		const int idx{ x + y * m_pSurface->w };

		SDL_GetRGB(m_pSurfacePixels[idx], m_pSurface->format, &r, &g, &b);

		return ColorRGB{ r / 255.f, g / 255.f, b / 255.f };
	}
}