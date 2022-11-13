//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_AspectRatio = m_Width / (float)m_Height;
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Render_W1_Part1();	// Rasterizer Stage Only
	//Render_W1_Part2();	// Projection Stage (Camera)
	//Render_W1_Part3();	// Barycentric Coordinates
	Render_W1_Part4();	// Depth Buffer
	//Render_W1_Part5();	// BoundingBox Optimization

	// Define Triangle - Vertices in NDC space
	//std::vector<Vector3> vertices_ndc
	//{
	//	{ 0.f, .5f, 1.f },
	//	{ .5f, -.5f, 1.f },
	//	{ -.5f, -.5f, 1.f }
	//};

	////RENDER LOGIC
	//for (int px{}; px < m_Width; ++px)
	//{
	//	for (int py{}; py < m_Height; ++py)
	//	{
	//		float gradient = px / static_cast<float>(m_Width);
	//		gradient += py / static_cast<float>(m_Width);
	//		gradient /= 2.0f;

	//		ColorRGB finalColor{ gradient, gradient, gradient };



	//		//Update Color in Buffer
	//		finalColor.MaxToOne();

	//		m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
	//			static_cast<uint8_t>(finalColor.r * 255),
	//			static_cast<uint8_t>(finalColor.g * 255),
	//			static_cast<uint8_t>(finalColor.b * 255));
	//	}
	//}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	//Todo > W1 Projection Stage
	vertices_out.reserve(vertices_in.size());

	for (Vertex vertex : vertices_in)
	{
		// to view space
		vertex.position = m_Camera.viewMatrix.TransformPoint(vertex.position);

		// to projection space
		vertex.position.x = vertex.position.x / vertex.position.z;
		vertex.position.y = vertex.position.y / vertex.position.z;

		vertex.position.x = vertex.position.x / (m_Camera.fov * m_AspectRatio);
		vertex.position.y = vertex.position.y / m_Camera.fov;

		// to screen/raster space
		vertex.position.x = (vertex.position.x + 1) / 2.f * m_Width;
		vertex.position.y = (1 - vertex.position.y) / 2.f * m_Height;

		vertices_out.push_back(vertex);
	}
}

void dae::Renderer::Render_W1_Part1()
{
	std::vector<Vector3> vertices_ndc
	{
		{ 0.f, .5f, 1.f },
		{ .5f, -.5f, 1.f },
		{ -.5f, -.5f, 1.f }
	};

	std::vector<Vector2> vertices_raster{};

	for (const auto& v : vertices_ndc)
	{
		const auto ScreenSpaceVertexX = (v.x + 1) / 2.f * m_Width;
		const auto ScreenSpaceVertexY = (1 - v.y) / 2.f * m_Height;

		vertices_raster.push_back({ ScreenSpaceVertexX, ScreenSpaceVertexY });
	}
	
	for (size_t i{}; i < vertices_raster.size(); i += 3)
	{
		Vector2 edge01 = vertices_raster[i + 1] - vertices_raster[i];
		Vector2 edge12 = vertices_raster[i + 2] - vertices_raster[i + 1];
		Vector2 edge20 = vertices_raster[i] - vertices_raster[i + 2];

		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				ColorRGB finalColor{ 1, 1, 1 };

				Vector2 pixel = { (float)px,(float)py };

				const Vector2 directionV0 = pixel - vertices_raster[i];
				const Vector2 directionV1 = pixel - vertices_raster[i + 1];
				const Vector2 directionV2 = pixel - vertices_raster[i + 2];

				const float a = Vector2::Cross(directionV0, edge01);
				if (a > 0)
					continue;

				const float b = Vector2::Cross(directionV1, edge12);
				if (b > 0)
					continue;

				const float c = Vector2::Cross(directionV2, edge20);
				if (c > 0)
					continue;

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}
}

void dae::Renderer::Render_W1_Part2()
{
	std::vector<Vertex> vertices_world
	{
		{{0.f,2.f,0.f}},
		{{1.f,0.f,0.f}},
		{{-1.f,0.f,0.f}}
	};

	std::vector<Vertex> vertices_raster{};

	VertexTransformationFunction(vertices_world, vertices_raster);

	std::vector<Vector2> vertices_raster2D{};

	for (const auto& vertex : vertices_raster)
	{
		vertices_raster2D.push_back({ vertex.position.x,vertex.position.y });
	}

	ColorRGB finalColor{ 1, 1, 1 };

	for (size_t i{}; i < vertices_raster2D.size(); i += 3)
	{
		const Vector2 v0 = vertices_raster2D[i];
		const Vector2 v1 = vertices_raster2D[i + 1];
		const Vector2 v2 = vertices_raster2D[i + 2];

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{

				Vector2 pixel = { (float)px,(float)py };

				const Vector2 directionV0 = pixel - v0;
				const Vector2 directionV1 = pixel - v1;
				const Vector2 directionV2 = pixel - v2;

				const float a = Vector2::Cross(directionV0, edge01);
				if (a > 0)
					continue;

				const float b = Vector2::Cross(directionV1, edge12);
				if (b > 0)
					continue;

				const float c = Vector2::Cross(directionV2, edge20);
				if (c > 0)
					continue;

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}
}

void dae::Renderer::Render_W1_Part3()
{
	std::vector<Vertex> vertices_world
	{
		{{0.f,4.f,2.f},{1,0,0}},
		{{3.f,-2.f,2.f},{0,1,0}},
		{{-3.f,-2.f,2.f},{0,0,1}}
	};

	std::vector<Vertex> vertices_raster{};

	VertexTransformationFunction(vertices_world, vertices_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < vertices_raster.size(); i += 3)
	{
		const Vector2 v0 = { vertices_raster[i].position.x,	vertices_raster[i].position.y };
		const Vector2 v1 = { vertices_raster[i + 1].position.x,	vertices_raster[i + 1].position.y };
		const Vector2 v2 = { vertices_raster[i + 2].position.x,	vertices_raster[i + 2].position.y };

		ColorRGB colorV0 = vertices_raster[i].color;
		ColorRGB colorV1 = vertices_raster[i + 1].color;
		ColorRGB colorV2 = vertices_raster[i + 2].color;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				finalColor = colors::Black;

				Vector2 pixel = { (float)px,(float)py };

				const Vector2 directionV0 = pixel - v0;
				const Vector2 directionV1 = pixel - v1;
				const Vector2 directionV2 = pixel - v2;

				float weightV2 = Vector2::Cross(edge01, directionV0);
				if (weightV2 < 0)
					continue;

				float weightV0 = Vector2::Cross(edge12, directionV1);
				if (weightV0 < 0)
					continue;

				float weightV1 = Vector2::Cross(edge20, directionV2);
				if (weightV1 < 0)
					continue;

				weightV0 /= areaTriangle;
				weightV1 /= areaTriangle;
				weightV2 /= areaTriangle;

				finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}
}

void dae::Renderer::Render_W1_Part4()
{
	// Fill the array with max float value
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);
	
	ClearBackground();

	std::vector<Vertex> vertices_world
	{
		// Triangle 0
		{{0.f,2.f,0.f},{1,0,0}},
		{{1.5f,-1.f,0.f},{1,0,0}},
		{{-1.5f,-1.f,0.f},{1,0,0}},

		// Triangle 1
		{{0.f,4.f,2.f},{1,0,0}},
		{{3.f,-2.f,2.f},{0,1,0}},
		{{-3.f,-2.f,2.f},{0,0,1}}
	};

	std::vector<Vertex> vertices_raster{};

	VertexTransformationFunction(vertices_world, vertices_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < vertices_raster.size(); i += 3)
	{
		const Vector2 v0 = { vertices_raster[i].position.x,	vertices_raster[i].position.y };
		const Vector2 v1 = { vertices_raster[i + 1].position.x,	vertices_raster[i + 1].position.y };
		const Vector2 v2 = { vertices_raster[i + 2].position.x,	vertices_raster[i + 2].position.y };

		ColorRGB colorV0 = vertices_raster[i].color;
		ColorRGB colorV1 = vertices_raster[i + 1].color;
		ColorRGB colorV2 = vertices_raster[i + 2].color;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				finalColor = colors::Black;

				Vector2 pixel = { (float)px,(float)py };

				const Vector2 directionV0 = pixel - v0;
				const Vector2 directionV1 = pixel - v1;
				const Vector2 directionV2 = pixel - v2;

				float weightV2 = Vector2::Cross(edge01, directionV0);
				if (weightV2 < 0)
					continue;

				float weightV0 = Vector2::Cross(edge12, directionV1);
				if (weightV0 < 0)
					continue;

				float weightV1 = Vector2::Cross(edge20, directionV2);
				if (weightV1 < 0)
					continue;

				weightV0 /= areaTriangle;
				weightV1 /= areaTriangle;
				weightV2 /= areaTriangle;

				const float depthWeight =
				{
					weightV0 * vertices_raster[i].position.z +
					weightV1 * vertices_raster[i + 1].position.z +
					weightV2 * vertices_raster[i + 2].position.z
				};

				if (depthWeight > m_pDepthBufferPixels[px * m_Height + py])
					continue;

				m_pDepthBufferPixels[px * m_Height + py] = depthWeight;

				finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}

}


void dae::Renderer::Render_W1_Part5()
{
}

void dae::Renderer::Render_W5_Part1()
{
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
