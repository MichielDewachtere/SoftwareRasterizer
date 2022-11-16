//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

#define INT int

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

	m_pDepthBufferPixels = new float[(int)(m_Width * m_Height)];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_AspectRatio = (float)m_Width / (float)m_Height;

	m_pUVGridTexture = Texture::LoadFromFile("Resources/uv_grid_2.png");
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pUVGridTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render() const
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Render_W1_Part1();	// Rasterizer Stage Only
	//Render_W1_Part2();	// Projection Stage (Camera)
	//Render_W1_Part3();	// Barycentric Coordinates
	//Render_W1_Part4();	// Depth Buffer
	//Render_W1_Part5();	// BoundingBox Optimization

	//Render_W2_Part1();	// QUAD (TriangleList)
	//Render_W2_Part2();	//QUAD (TriangleStrip)
	//Render_W2_Part3();	// Texture
	Render_W2_Part4();	// Correct Interpolation

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction_W1(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
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
		vertex.position.x = (vertex.position.x + 1) / 2.f * (float)m_Width;
		vertex.position.y = (1 - vertex.position.y) / 2.f * (float)m_Height;

		vertices_out.push_back(vertex);
	}
}
void dae::Renderer::VertexTransformationFunction_W2(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const
{
	meshes_out.reserve(meshes_in.size());

	for (const Mesh& mesh : meshes_in)
	{
		meshes_out.push_back(mesh);
		meshes_out[0].vertices.clear();
		meshes_out[0].vertices.reserve(mesh.vertices.size());

		for (Vertex vertex : mesh.vertices)
		{
			// to view space
			vertex.position = m_Camera.viewMatrix.TransformPoint(vertex.position);

			// to projection space
			vertex.position.x = vertex.position.x / vertex.position.z;
			vertex.position.y = vertex.position.y / vertex.position.z;

			vertex.position.x = vertex.position.x / (m_Camera.fov * m_AspectRatio);
			vertex.position.y = vertex.position.y / m_Camera.fov;

			// to screen/raster space
			vertex.position.x = (vertex.position.x + 1) / 2.f * (float)m_Width;
			vertex.position.y = (1 - vertex.position.y) / 2.f * (float)m_Height;

			meshes_out[0].vertices.push_back(vertex);
		}
	}
}
#pragma region Week1
void dae::Renderer::Render_W1_Part1() const
{
	const std::vector<Vector3> vertices_ndc
	{
		{ 0.f, .5f, 1.f },
		{ .5f, -.5f, 1.f },
		{ -.5f, -.5f, 1.f }
	};

	std::vector<Vector2> vertices_raster{};

	for (const auto& v : vertices_ndc)
	{
		const auto screenSpaceVertexX = (v.x + 1) / 2.f * (float)m_Width;
		const auto screenSpaceVertexY = (1 - v.y) / 2.f * (float)m_Height;

		vertices_raster.emplace_back(screenSpaceVertexX, screenSpaceVertexY);
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

void dae::Renderer::Render_W1_Part2() const
{
	const std::vector<Vertex> vertices_world
	{
		{{0.f,2.f,0.f}},
		{{1.f,0.f,0.f}},
		{{-1.f,0.f,0.f}}
	};

	std::vector<Vertex> vertices_raster{};

	VertexTransformationFunction_W1(vertices_world, vertices_raster);

	std::vector<Vector2> vertices_raster2D{};

	for (const auto& vertex : vertices_raster)
	{
		vertices_raster2D.emplace_back(vertex.position.x,vertex.position.y);
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

void dae::Renderer::Render_W1_Part3() const
{
	const std::vector<Vertex> vertices_world
	{
		{{0.f,4.f,2.f},{1,0,0}},
		{{3.f,-2.f,2.f},{0,1,0}},
		{{-3.f,-2.f,2.f},{0,0,1}}
	};

	std::vector<Vertex> vertices_raster{};

	VertexTransformationFunction_W1(vertices_world, vertices_raster);

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

void dae::Renderer::Render_W1_Part4() const
{
	// Fill the array with max float value
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);
	
	ClearBackground();

	const std::vector<Vertex> vertices_world
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

	VertexTransformationFunction_W1(vertices_world, vertices_raster);

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

void dae::Renderer::Render_W1_Part5() const
{
	// Fill the array with max float value
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	const std::vector<Vertex> vertices_world
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

	VertexTransformationFunction_W1(vertices_world, vertices_raster);

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

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 0 || right >= (m_Width - 1))
			continue;

		if (bottom <= 0 || top >= (m_Height - 1))
			continue;

		for (INT px = left; px < right; ++px)
		{
			for (INT py = bottom; py < top; ++py)
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

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				const float depthWeight =
				{
					weightV0 * vertices_raster[i].position.z +
					weightV1 * vertices_raster[i + 1].position.z +
					weightV2 * vertices_raster[i + 2].position.z
				};

				if (depthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = depthWeight;

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
#pragma endregion
#pragma region Week2
void dae::Renderer::Render_W2_Part1() const
{
	// Fill the array with max float value
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	const std::vector<Mesh> meshes_world
	{
		Mesh{
			{
			Vertex{{-3,3,-2}},
			Vertex{{0,3,-2}},
			Vertex{{3,3,-2}},
			Vertex{{-3,0,-2}},
			Vertex{{0,0,-2}},
			Vertex{{3,0,-2}},
			Vertex{{-3,-3,-2}},
			Vertex{{0,-3,-2}},
			Vertex{{3,-3,-2}}
			},

			{
				3,0,1,	1,4,3,	4,1,2,
				2,5,4,	6,3,4,	4,7,6,
				7,4,5,	5,8,7
			},

		PrimitiveTopology::TriangleList
		}
	};

	std::vector<Mesh> meshes_raster{};

	VertexTransformationFunction_W2(meshes_world, meshes_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < meshes_raster[0].indices.size(); i += 3)
	{
		const Mesh mesh = meshes_raster[0];

		const Vector2 v0 = { mesh.vertices[mesh.indices[i]].position.x, mesh.vertices[mesh.indices[i]].position.y };
		const Vector2 v1 = { mesh.vertices[mesh.indices[i + 1]].position.x, mesh.vertices[mesh.indices[i + 1]].position.y };
		const Vector2 v2 = { mesh.vertices[mesh.indices[i + 2]].position.x, mesh.vertices[mesh.indices[i + 2]].position.y };
		
		ColorRGB colorV0 = mesh.vertices[mesh.indices[i]].color;
		ColorRGB colorV1 = mesh.vertices[mesh.indices[i + 1]].color;
		ColorRGB colorV2 = mesh.vertices[mesh.indices[i + 2]].color;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 0 || right >= (m_Width - 1))
			continue;

		if (bottom <= 0 || top >= (m_Height - 1))
			continue;

		for (INT px = left; px < right; ++px)
		{
			for (INT py = bottom; py < top; ++py)
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

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				const float depthWeight =
				{
					weightV0 * mesh.vertices[mesh.indices[i]].position.z +
					weightV1 * mesh.vertices[mesh.indices[i + 1]].position.z +
					weightV2 * mesh.vertices[mesh.indices[i + 2]].position.z
				};

				if (depthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = depthWeight;

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

void dae::Renderer::Render_W2_Part2() const
{
	// Fill the array with max float value
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	const std::vector<Mesh> meshes_world
	{
		Mesh{
			{
			Vertex{{-3,3,-2}},
			Vertex{{0,3,-2}},
			Vertex{{3,3,-2}},
			Vertex{{-3,0,-2}},
			Vertex{{0,0,-2}},
			Vertex{{3,0,-2}},
			Vertex{{-3,-3,-2}},
			Vertex{{0,-3,-2}},
			Vertex{{3,-3,-2}}
			},

			{
				3,0,4,1,5,2,
				2,6,
				6,3,7,4,8,5
			},

		PrimitiveTopology::TriangleStrip
		}
	};

	std::vector<Mesh> meshes_raster{};

	VertexTransformationFunction_W2(meshes_world, meshes_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < meshes_raster[0].indices.size() - 2; ++i)
	{
		const Mesh mesh = meshes_raster[0];

		const Vector2 v0 = { mesh.vertices[mesh.indices[i]].position.x, mesh.vertices[mesh.indices[i]].position.y };
		Vector2 v1 = { mesh.vertices[mesh.indices[i + 1]].position.x, mesh.vertices[mesh.indices[i + 1]].position.y };
		Vector2 v2 = { mesh.vertices[mesh.indices[i + 2]].position.x, mesh.vertices[mesh.indices[i + 2]].position.y };

		ColorRGB colorV0 = mesh.vertices[mesh.indices[i]].color;
		ColorRGB colorV1 = mesh.vertices[mesh.indices[i + 1]].color;
		ColorRGB colorV2 = mesh.vertices[mesh.indices[i + 2]].color;

		if (i % 2 == 1)
		{
			v1 = { mesh.vertices[mesh.indices[i + 2]].position.x, mesh.vertices[mesh.indices[i + 2]].position.y };
			v2 = { mesh.vertices[mesh.indices[i + 1]].position.x, mesh.vertices[mesh.indices[i + 1]].position.y };

			colorV1 = mesh.vertices[mesh.indices[i + 2]].color;
			colorV2 = mesh.vertices[mesh.indices[i + 1]].color;
		}

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 0 || right >= (m_Width - 1))
			continue;

		if (bottom <= 0 || top >= (m_Height - 1))
			continue;

		for (INT px = left; px < right; ++px)
		{
			for (INT py = bottom; py < top; ++py)
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

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				float depthWeight =
				{
					weightV0 * mesh.vertices[mesh.indices[i]].position.z +
					weightV1 * mesh.vertices[mesh.indices[i + 1]].position.z +
					weightV2 * mesh.vertices[mesh.indices[i + 2]].position.z
				};

				if (i % 2 == 1)
				{
					depthWeight =
					{
						weightV0 * mesh.vertices[mesh.indices[i]].position.z +
						weightV1 * mesh.vertices[mesh.indices[i + 2]].position.z +
						weightV2 * mesh.vertices[mesh.indices[i + 1]].position.z
					};
				}
				if (depthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = depthWeight;

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

void dae::Renderer::Render_W2_Part3() const
{
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	const std::vector<Mesh> meshes_world
	{
		Mesh{
			{
			Vertex{{-3,3,-2},colors::White,{0,0}},
			Vertex{{0,3,-2},	colors::White,{.5f,0}},
			Vertex{{3,3,-2},	colors::White,{1,0}},
			Vertex{{-3,0,-2},colors::White,{0,.5f}},
			Vertex{{0,0,-2},	colors::White,{.5f,.5f}},
			Vertex{{3,0,-2},	colors::White,{1,.5f}},
			Vertex{{-3,-3,-2},colors::White,{0,1}},
			Vertex{{0,-3,-2},colors::White,{.5f,1}},
			Vertex{{3,-3,-2},colors::White,{1,1}}
			},

			{
				3,0,1,	1,4,3,	4,1,2,
				2,5,4,	6,3,4,	4,7,6,
				7,4,5,	5,8,7
			},

		PrimitiveTopology::TriangleList
		}
	};

	std::vector<Mesh> meshes_raster{};

	VertexTransformationFunction_W2(meshes_world, meshes_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < meshes_raster[0].indices.size(); i += 3)
	{
		const Mesh mesh = meshes_raster[0];

		const Vector2 v0 = { mesh.vertices[mesh.indices[i]].position.x, mesh.vertices[mesh.indices[i]].position.y };
		const Vector2 v1 = { mesh.vertices[mesh.indices[i + 1]].position.x, mesh.vertices[mesh.indices[i + 1]].position.y };
		const Vector2 v2 = { mesh.vertices[mesh.indices[i + 2]].position.x, mesh.vertices[mesh.indices[i + 2]].position.y };

		const Vector2 uvV0 = mesh.vertices[mesh.indices[i]].uv;
		const Vector2 uvV1 = mesh.vertices[mesh.indices[i + 1]].uv;
		const Vector2 uvV2 = mesh.vertices[mesh.indices[i + 2]].uv;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 0 || right >= (m_Width - 1))
			continue;

		if (bottom <= 0 || top >= (m_Height - 1))
			continue;

		for (INT px = left; px < right; ++px)
		{
			for (INT py = bottom; py < top; ++py)
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

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				const float depthWeight =
				{
					weightV0 * mesh.vertices[mesh.indices[i]].position.z +
					weightV1 * mesh.vertices[mesh.indices[i + 1]].position.z +
					weightV2 * mesh.vertices[mesh.indices[i + 2]].position.z
				};

				if (depthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = depthWeight;

				const Vector2 weightedUV = uvV0 * weightV0 + uvV1 * weightV1 + uvV2 * weightV2;

				finalColor = m_pUVGridTexture->Sample(weightedUV);

				//finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;

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

void dae::Renderer::Render_W2_Part4() const
{
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	const std::vector<Mesh> meshes_world
	{
		Mesh{
			{
			Vertex{{-3,3,-2},colors::White,{0,0}},
			Vertex{{0,3,-2},	colors::White,{.5f,0}},
			Vertex{{3,3,-2},	colors::White,{1,0}},
			Vertex{{-3,0,-2},colors::White,{0,.5f}},
			Vertex{{0,0,-2},	colors::White,{.5f,.5f}},
			Vertex{{3,0,-2},	colors::White,{1,.5f}},
			Vertex{{-3,-3,-2},colors::White,{0,1}},
			Vertex{{0,-3,-2},colors::White,{.5f,1}},
			Vertex{{3,-3,-2},colors::White,{1,1}}
			},

			{
				3,0,1,	1,4,3,	4,1,2,
				2,5,4,	6,3,4,	4,7,6,
				7,4,5,	5,8,7
			},

		PrimitiveTopology::TriangleList
		}
	};

	std::vector<Mesh> meshes_raster{};

	VertexTransformationFunction_W2(meshes_world, meshes_raster);

	ColorRGB finalColor{ };

	for (size_t i{}; i < meshes_raster[0].indices.size(); i += 3)
	{
		const Mesh mesh = meshes_raster[0];

		const Vector2 v0 = { mesh.vertices[mesh.indices[i]].position.x, mesh.vertices[mesh.indices[i]].position.y };
		const Vector2 v1 = { mesh.vertices[mesh.indices[i + 1]].position.x, mesh.vertices[mesh.indices[i + 1]].position.y };
		const Vector2 v2 = { mesh.vertices[mesh.indices[i + 2]].position.x, mesh.vertices[mesh.indices[i + 2]].position.y };

		const float depthV0 = mesh.vertices[mesh.indices[i]].position.z;
		const float depthV1 = mesh.vertices[mesh.indices[i + 1]].position.z;
		const float depthV2 = mesh.vertices[mesh.indices[i + 2]].position.z;

		const Vector2 uvV0 = mesh.vertices[mesh.indices[i]].uv;
		const Vector2 uvV1 = mesh.vertices[mesh.indices[i + 1]].uv;
		const Vector2 uvV2 = mesh.vertices[mesh.indices[i + 2]].uv;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 0 || right >= (m_Width - 1))
			continue;

		if (bottom <= 0 || top >= (m_Height - 1))
			continue;

		for (INT px = left; px < right; ++px)
		{
			for (INT py = bottom; py < top; ++py)
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

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				const float interpolatedDepthWeight = {
					1.f /
					((1 / depthV0) * weightV0 +
					(1 / depthV1) * weightV1 +
					(1 / depthV2) * weightV2)
				};

				if (interpolatedDepthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedDepthWeight;

				const Vector2 interpolatedUV = {
					((uvV0 / depthV0) * weightV0 +
					(uvV1 / depthV1) * weightV1 +
					(uvV2 / depthV2) * weightV2) * interpolatedDepthWeight
				};
				
				finalColor = m_pUVGridTexture->Sample(interpolatedUV);
				
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
#pragma endregion
void dae::Renderer::ClearBackground() const
{
	SDL_FillRect(m_pBackBuffer, nullptr, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
