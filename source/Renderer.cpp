//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"
#include "BRDF.h"
#include <iostream>

#define INT int

//#define MESH_TUKTUK
#define MESH_VEHICLE

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow),
	m_IsRotating(false),
	m_EnableNormalMap(true)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[(int)(m_Width * m_Height)];

	// This way the Camera::CalculateProjectionMatrix is only called when the FOV or AspectRatio is changed
	// see definition 
	SetAspectRatio((float)m_Width / (float)m_Height);


	m_pUVGridTexture = Texture::LoadFromFile("Resources/uv_grid_2.png");
#ifdef MESH_TUKTUK
	SetFovAngle(60.f);

	m_pTukTukTexture = Texture::LoadFromFile("resources/tuktuk.png");

	TukTukMeshInit();
#elif defined(MESH_VEHICLE)
	SetFovAngle(45.f);

	m_pVehicleDiffuse = Texture::LoadFromFile("resources/vehicle_diffuse.png");
	m_pVehicleNormalMap = Texture::LoadFromFile("resources/vehicle_normal.png");
	m_pGlossinessMap = Texture::LoadFromFile("resources/vehicle_gloss.png");
	m_pSpecularMap = Texture::LoadFromFile("resources/vehicle_specular.png");

	VehicleMeshInit();
#endif

	//Initialize Camera
	m_Camera.Initialize(m_FovAngle, { .0f,.0f, 0.f }, m_AspectRatio);

	m_CurrentDisplayMode = DisplayMode::FinalColor;
	m_CurrentShadingMode = ShadingMode::Combined;

}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pUVGridTexture;
	delete m_pTukTukTexture;
	delete m_pVehicleDiffuse;
	delete m_pVehicleNormalMap;
	delete m_pGlossinessMap;
	delete m_pSpecularMap;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	if (m_IsRotating)
	{
		constexpr float rotationSpeed{ 30 * TO_RADIANS };
#ifdef MESH_TUKTUK
		m_TukTukMesh.worldMatrix = Matrix::CreateRotationY(rotationSpeed * pTimer->GetElapsed()) * m_TukTukMesh.worldMatrix;
#elif defined(MESH_VEHICLE)
		m_VehicleMesh.worldMatrix = Matrix::CreateRotationY(rotationSpeed * pTimer->GetElapsed()) * m_VehicleMesh.worldMatrix;
#endif
	}
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
	//Render_W2_Part2();	// QUAD (TriangleStrip)
	//Render_W2_Part3();	// Texture
	//Render_W2_Part4();	// Correct Interpolation

#ifdef MESH_TUKTUK
	Render_W3();
#elif defined(MESH_VEHICLE)
	Render_W4();
#endif
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
void Renderer::VertexTransformationFunction_W2(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const
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
void Renderer::VertexTransformationFunction_W3(std::vector<Mesh>& meshes) const
{
	for (auto& m : meshes)
	{
		m.vertices_out.clear();
		m.vertices_out.reserve(m.vertices.size());

		Matrix worldViewProjectionMatrix = m.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

		for (const auto& v : m.vertices)
		{
			Vertex_Out vertexOut{};

			// to NDC-Space
			vertexOut.position = worldViewProjectionMatrix.TransformPoint(v.position.ToVector4());

			vertexOut.position.x /= vertexOut.position.w;
			vertexOut.position.y /= vertexOut.position.w;
			vertexOut.position.z /= vertexOut.position.w;

			// TODO: temporary fix, problem is probably in one of the matrices
			vertexOut.position.z = 1 - vertexOut.position.z;

			vertexOut.uv = v.uv;

			m.vertices_out.emplace_back(vertexOut);
		}
	}
}
void Renderer::VertexTransformationFunction_W4(std::vector<Mesh>& meshes) const
{
	for (auto& m : meshes)
	{
		m.vertices_out.clear();
		m.vertices_out.reserve(m.vertices.size());

		Matrix worldViewProjectionMatrix = m.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

		for (const auto& v : m.vertices)
		{
			Vertex_Out vertexOut{};

			// to NDC-Space
			vertexOut.position = worldViewProjectionMatrix.TransformPoint(v.position.ToVector4());

			vertexOut.viewDirection = Vector3{ vertexOut.position.GetXYZ() };
			vertexOut.viewDirection.Normalize();

			vertexOut.position.x /= vertexOut.position.w;
			vertexOut.position.y /= vertexOut.position.w;
			vertexOut.position.z /= vertexOut.position.w;

			vertexOut.color = v.color;
			vertexOut.normal = m.worldMatrix.TransformVector(v.normal).Normalized();
			vertexOut.uv = v.uv;
			vertexOut.tangent = m.worldMatrix.TransformVector(v.tangent).Normalized();

			m.vertices_out.emplace_back(vertexOut);
		}
	}
}

void Renderer::PixelShading(Vertex_Out& v) const
{
	ColorRGB tempColor{ colors::Black };

	const Vector3 directionToLight = -Vector3{ .577f,-.577f,.577f };
	constexpr float lightIntensity = 7.f;
	constexpr float specularShininess = 25.f;

	Vector3 normal;

	if (m_EnableNormalMap)
	{
		// Normal map
		const Vector3 biNormal = Vector3::Cross(v.normal, v.tangent);
		const Matrix tangentSpaceAxis = { v.tangent, biNormal, v.normal, Vector3::Zero };

		const ColorRGB normalColor = m_pVehicleNormalMap->Sample(v.uv);
		Vector3 sampledNormal = { normalColor.r, normalColor.g, normalColor.b }; // => range [0, 1]
		sampledNormal = 2.f * sampledNormal - Vector3{ 1, 1, 1 }; // => [0, 1] to [-1, 1]

		sampledNormal = tangentSpaceAxis.TransformVector(sampledNormal);

		normal = sampledNormal.Normalized();
		////////////////////
	}
	else
		normal = v.normal;

	// Observed Area
	const float lambertCos = Vector3::Dot(normal, directionToLight);
	if (lambertCos < 0)
		return;

	const ColorRGB observedArea = { lambertCos, lambertCos, lambertCos };
	////////////////////

	ColorRGB diffuse;

	// phong specular
	const ColorRGB gloss = m_pGlossinessMap->Sample(v.uv);
	const float exponent = gloss.r * specularShininess;

	ColorRGB specular;
	////////////////////////

	switch (m_CurrentShadingMode)
	{
	case ShadingMode::ObservedArea:
		tempColor += observedArea;
		break;
	case ShadingMode::Diffuse:
		diffuse = BRDF::Lambert(m_pVehicleDiffuse->Sample(v.uv));

		tempColor += diffuse * observedArea * lightIntensity;
		break;
	case ShadingMode::Specular:
		specular = BRDF::Phong(m_pSpecularMap->Sample(v.uv), exponent, directionToLight, v.viewDirection, normal);

		tempColor += specular * observedArea;
		break;
	case ShadingMode::Combined:		 
		specular = BRDF::Phong(m_pSpecularMap->Sample(v.uv), exponent, directionToLight, v.viewDirection, normal);
		diffuse = BRDF::Lambert(m_pVehicleDiffuse->Sample(v.uv));

		tempColor += diffuse * observedArea * lightIntensity + specular;
		break;
	}

	constexpr ColorRGB ambient = { .025f,.025f,.025f };

	tempColor += ambient;

	v.color = tempColor;
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
#pragma region Week3
void Renderer::Render_W3() const
{
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();
	
	std::vector<Mesh> meshes_world{ m_TukTukMesh };

	VertexTransformationFunction_W3(meshes_world);

	for (auto& m : meshes_world)
	{
		switch (m_TukTukMesh.primitiveTopology)
		{
		case PrimitiveTopology::TriangleList:
			RenderTriangleListW3(m);
			break;
		case PrimitiveTopology::TriangleStrip:
			RenderTriangleStripW3(m);
			break;
		}
	}
}

void Renderer::RenderTriangleListW3(Mesh& mesh) const
{
	ColorRGB finalColor{ };

	for (size_t i{}; i < mesh.indices.size(); i += 3)
	{
		Vertex_Out vOut0 = mesh.vertices_out[mesh.indices[i]];
		Vertex_Out vOut1 = mesh.vertices_out[mesh.indices[i + 1]];
		Vertex_Out vOut2 = mesh.vertices_out[mesh.indices[i + 2]];

		// frustum culling check
		if (!IsInFrustum(vOut0)
			|| !IsInFrustum(vOut1)
			|| !IsInFrustum(vOut2))
			continue;

		// from NDC space to Raster space
		NDCToRaster(vOut0);
		NDCToRaster(vOut1);
		NDCToRaster(vOut2);

		const Vector2 v0 = { vOut0.position.x, vOut0.position.y };
		const Vector2 v1 = { vOut1.position.x, vOut1.position.y };
		const Vector2 v2 = { vOut2.position.x, vOut2.position.y };

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float reversedAreaTriangle = 1 / Vector2::Cross(edge01, edge12);

		// create bounding box for triangle
		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		// check if bounding box is in screen
		if (left <= 0 || right >= m_Width - 1)
			continue;

		if (bottom <= 0 || top >= m_Height - 1)
			continue;

		constexpr INT offSet{ 1 };

		// iterate over every pixel in the bounding box, with an offset we enlarge the BB
		// in case of overlooked pixels
		for (INT px = left - offSet; px < right + offSet; ++px)
		{
			for (INT py = bottom - offSet; py < top + offSet; ++py)
			{
				finalColor = colors::Black;

				Vector2 pixelPos = { (float)px,(float)py };

				const Vector2 directionV0 = pixelPos - v0;
				const Vector2 directionV1 = pixelPos - v1;
				const Vector2 directionV2 = pixelPos - v2;

				// weights are all negative => back-face culling
				// vs all positive => front-face culling
				float weightV2 = Vector2::Cross(edge01, directionV0);
				if (weightV2 < 0)
					continue;

				float weightV0 = Vector2::Cross(edge12, directionV1);
				if (weightV0 < 0)
					continue;

				float weightV1 = Vector2::Cross(edge20, directionV2);
				if (weightV1 < 0)
					continue;

				weightV0 *= reversedAreaTriangle;
				weightV1 *= reversedAreaTriangle;
				weightV2 *= reversedAreaTriangle;

				if (weightV0 + weightV1 + weightV2 < 1
					&& weightV0 + weightV1 + weightV2 > 1)
					continue;

				// This Z-BufferValue is the one we compare in the Depth Test and
				// the value we store in the Depth Buffer (uses position.z).
				float interpolatedZDepth = {
					1.f /
					((1 / vOut0.position.z) * weightV0 +
					(1 / vOut1.position.z) * weightV1 +
					(1 / vOut2.position.z) * weightV2)
				};

				if (interpolatedZDepth < 0 || interpolatedZDepth > 1)
					continue;


				if (interpolatedZDepth > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepth;

				switch (m_CurrentDisplayMode)
				{
				case DisplayMode::FinalColor:
				{
					// When we want to interpolate vertex attributes with a correct depth(color, uv, normals, etc.),
					// we still use the View Space depth(uses position.w)
					const float interpolatedWDepth = {
						1.f /
						((1 / vOut0.position.w) * weightV0 +
						(1 / vOut1.position.w) * weightV1 +
						(1 / vOut2.position.w) * weightV2)
					};

					const Vector2 interpolatedUV = {
						((vOut0.uv / vOut0.position.w) * weightV0 +
						(vOut1.uv / vOut1.position.w) * weightV1 +
						(vOut2.uv / vOut2.position.w) * weightV2) * interpolatedWDepth
					};

					finalColor = m_pTukTukTexture->Sample(interpolatedUV);
					break;
				}
				case DisplayMode::DepthBuffer:
				{
					const float depthBufferColor = Remap(m_pDepthBufferPixels[px + (py * m_Width)], 0.985f, 1.0f);

					finalColor = { depthBufferColor, depthBufferColor, depthBufferColor };
					break;
				}
				}

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

void Renderer::RenderTriangleStripW3(const Mesh& mesh) const
{
	ColorRGB finalColor{};

	for (size_t i{}; i < mesh.vertices_out.size() - 2; ++i)
	{
		// if isOdd == true, the second and third vertex have to be swapped.
		// see lesson 2
		const bool isOdd = i % 2 == 1;

		const int vIdx0{ (int)mesh.indices[i] };
		const int vIdx1{ (int)mesh.indices[(int)i + 1 * !isOdd + 2 * isOdd] };
		const int vIdx2{ (int)mesh.indices[(int)i + 2 * !isOdd + 1 * isOdd] };

		const Vector2 v0 = { mesh.vertices_out[mesh.indices[vIdx0]].position.x, mesh.vertices_out[mesh.indices[vIdx0]].position.y };
		const Vector2 v1 = { mesh.vertices_out[mesh.indices[vIdx1]].position.x, mesh.vertices_out[mesh.indices[vIdx1]].position.y };
		const Vector2 v2 = { mesh.vertices_out[mesh.indices[vIdx2]].position.x, mesh.vertices_out[mesh.indices[vIdx2]].position.y };
		 
		const float depthV0 = mesh.vertices_out[mesh.indices[vIdx0]].position.z;
		const float depthV1 = mesh.vertices_out[mesh.indices[vIdx1]].position.z;
		const float depthV2 = mesh.vertices_out[mesh.indices[vIdx2]].position.z;
		 
		const float wV0 = mesh.vertices_out[mesh.indices[vIdx0]].position.w;
		const float wV1 = mesh.vertices_out[mesh.indices[vIdx1]].position.w;
		const float wV2 = mesh.vertices_out[mesh.indices[vIdx2]].position.w;
		 
		const Vector2 uvV0 = mesh.vertices_out[mesh.indices[vIdx0]].uv;
		const Vector2 uvV1 = mesh.vertices_out[mesh.indices[vIdx1]].uv;
		const Vector2 uvV2 = mesh.vertices_out[mesh.indices[vIdx2]].uv;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 1|| right >= (m_Width - 1))
			continue;

		if (bottom <= 1 || top >= (m_Height - 1))
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

				float weightV0 = Vector2::Cross(edge12, directionV1);
				if (weightV0 > 0)
					continue;

				float weightV1 = Vector2::Cross(edge20, directionV2);
				if (weightV1 > 0)
					continue;

				float weightV2 = Vector2::Cross(edge01, directionV0);
				if (weightV2 > 0)
					continue;

				weightV0 /= areaTriangle;
				weightV1 /= areaTriangle;
				weightV2 /= areaTriangle;

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				// This Z-BufferValue is the one we compare in the Depth Test and
				// the value we store in the Depth Buffer (uses position.z).
				const float interpolatedZDepthWeight = {
					1.f /
					((1 / depthV0) * weightV0 +
					(1 / depthV1) * weightV1 +
					(1 / depthV2) * weightV2)
				};

				// frustum clipping
				if (interpolatedZDepthWeight < 0 || interpolatedZDepthWeight > 1)
					continue;

				if (interpolatedZDepthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepthWeight;

				switch (m_CurrentDisplayMode)
				{
				case DisplayMode::FinalColor:
				{
					// When we want to interpolate vertex attributes with a correct depth(color, uv, normals, etc.),
					// we still use the View Space depth(uses position.w)
					const float interpolatedWDepthWeight = {
						1.f /
						((1 / wV0) * weightV0 +
						(1 / wV1) * weightV1 +
						(1 / wV2) * weightV2)
					};

					const Vector2 interpolatedUV = {
						((uvV0 / wV0) * weightV0 +
						(uvV1 / wV1) * weightV1 +
						(uvV2 / wV2) * weightV2) * interpolatedWDepthWeight
					};

					finalColor = m_pTukTukTexture->Sample(interpolatedUV);
					break;
				}
				case DisplayMode::DepthBuffer:
				{
					const float depthBufferColor = Remap(m_pDepthBufferPixels[px + (py * m_Width)], 0.985f, 1.0f);

					finalColor = { depthBufferColor, depthBufferColor, depthBufferColor };
					break;
				}
				}

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
#pragma region Week4
void Renderer::Render_W4() const
{
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	ClearBackground();

	std::vector<Mesh> meshes_world{ m_VehicleMesh };

	VertexTransformationFunction_W4(meshes_world);

	for (auto& m : meshes_world)
	{
		switch (m_VehicleMesh.primitiveTopology)
		{
		case PrimitiveTopology::TriangleList:
			RenderTriangleListW4(m);
			break;
		case PrimitiveTopology::TriangleStrip:
			RenderTriangleStripW4(m);
			break;
		}
	}
}

void Renderer::RenderTriangleListW4(Mesh& mesh) const
{
	ColorRGB finalColor{ };

	for (size_t i{}; i < mesh.indices.size(); i += 3)
	{
		Vertex_Out vOut0 = mesh.vertices_out[mesh.indices[i]];
		Vertex_Out vOut1 = mesh.vertices_out[mesh.indices[i + 1]];
		Vertex_Out vOut2 = mesh.vertices_out[mesh.indices[i + 2]];

		// frustum culling check
		if (!IsInFrustum(vOut0)
			|| !IsInFrustum(vOut1)
			|| !IsInFrustum(vOut2))
			continue;

		// from NDC space to Raster space
		NDCToRaster(vOut0);
		NDCToRaster(vOut1);
		NDCToRaster(vOut2);

		const Vector2 v0 = { vOut0.position.x, vOut0.position.y };
		const Vector2 v1 = { vOut1.position.x, vOut1.position.y };
		const Vector2 v2 = { vOut2.position.x, vOut2.position.y };

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(edge01, edge12);

		// create bounding box for triangle
		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		// check if bounding box is in screen
		if (left <= 0 || right >= m_Width - 1)
			continue;

		if (bottom <= 0 || top >= m_Height - 1)
			continue;

		constexpr INT offSet{ 1 };

		// iterate over every pixel in the bounding box, with an offset we enlarge the BB
		// in case of overlooked pixels
		for (INT px = left - offSet; px < right + offSet; ++px)
		{
			for (INT py = bottom - offSet; py < top + offSet; ++py)
			{
				finalColor = colors::Black;

				Vector2 pixelPos = { (float)px,(float)py };

				const Vector2 directionV0 = pixelPos - v0;
				const Vector2 directionV1 = pixelPos - v1;
				const Vector2 directionV2 = pixelPos - v2;

				// weights are all negative => back-face culling
				// vs all positive => front-face culling
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

				// This Z-BufferValue is the one we compare in the Depth Test and
				// the value we store in the Depth Buffer (uses position.z).
				float interpolatedZDepth = {
					1.f /
					((1 / vOut0.position.z) * weightV0 +
					(1 / vOut1.position.z) * weightV1 +
					(1 / vOut2.position.z) * weightV2)
				};

				if (interpolatedZDepth < 0 || interpolatedZDepth > 1)
					continue;

				if (interpolatedZDepth > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepth;

				switch (m_CurrentDisplayMode)
				{
				case DisplayMode::FinalColor:
				{
					// When we want to interpolate vertex attributes with a correct depth(color, uv, normals, etc.),
					// we still use the View Space depth(uses position.w)
					const float interpolatedWDepth = {
						1.f /
						((1 / vOut0.position.w) * weightV0 +
						(1 / vOut1.position.w) * weightV1 +
						(1 / vOut2.position.w) * weightV2)
					};

					const Vector2 interpolatedUV = {
						((vOut0.uv / vOut0.position.w) * weightV0 +
						(vOut1.uv / vOut1.position.w) * weightV1 +
						(vOut2.uv / vOut2.position.w) * weightV2) * interpolatedWDepth
					};

					const Vector3 interpolatedNormal = {
						((vOut0.normal / vOut0.position.w) * weightV0 +
						(vOut1.normal / vOut1.position.w) * weightV1 +
						(vOut2.normal / vOut2.position.w) * weightV2) * interpolatedWDepth
					};

					const Vector3 interpolatedTangent = {
						((vOut0.tangent / vOut0.position.w) * weightV0 +
						(vOut1.tangent / vOut1.position.w) * weightV1 +
						(vOut2.tangent / vOut2.position.w) * weightV2) * interpolatedWDepth
					};

					const Vector3 interpolatedViewDirection = {
						((vOut0.viewDirection / vOut0.position.w) * weightV0 +
						(vOut1.viewDirection / vOut1.position.w) * weightV1 +
						(vOut2.viewDirection / vOut2.position.w) * weightV2) * interpolatedWDepth
					};
					
					//Interpolated Vertex Attributes for Pixel
					Vertex_Out pixel;
					pixel.position = { pixelPos.x, pixelPos.y, interpolatedZDepth, interpolatedWDepth };
					pixel.color = finalColor;
					pixel.uv = interpolatedUV;
					pixel.normal = interpolatedNormal;
					pixel.tangent = interpolatedTangent;
					pixel.viewDirection = interpolatedViewDirection;

					PixelShading(pixel);

					finalColor = pixel.color;

					break;
				}
				case DisplayMode::DepthBuffer:
				{
					const float depthBufferColor = Remap(m_pDepthBufferPixels[px + (py * m_Width)], 0.995f, 1.0f);
					
					finalColor = { depthBufferColor, depthBufferColor, depthBufferColor };
					break;
				}
				}

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

void Renderer::DepthRemap(float& depth, float topPercentile) const
{
	depth = (depth - (1.f - topPercentile)) / topPercentile;

	depth = std::max(0.f, depth);
	depth = std::min(1.f, depth);
}

void Renderer::RenderTriangleStripW4(const Mesh& mesh) const
{
	ColorRGB finalColor{};

	for (size_t i{}; i < mesh.vertices_out.size() - 2; ++i)
	{
		// if isOdd == true, the second and third vertex have to be swapped.
		// see lesson 2
		const bool isOdd = i % 2 == 1;

		const int vIdx0{ (int)mesh.indices[i] };
		const int vIdx1{ (int)mesh.indices[(int)i + 1 * !isOdd + 2 * isOdd] };
		const int vIdx2{ (int)mesh.indices[(int)i + 2 * !isOdd + 1 * isOdd] };

		const Vector2 v0 = { mesh.vertices_out[mesh.indices[vIdx0]].position.x, mesh.vertices_out[mesh.indices[vIdx0]].position.y };
		const Vector2 v1 = { mesh.vertices_out[mesh.indices[vIdx1]].position.x, mesh.vertices_out[mesh.indices[vIdx1]].position.y };
		const Vector2 v2 = { mesh.vertices_out[mesh.indices[vIdx2]].position.x, mesh.vertices_out[mesh.indices[vIdx2]].position.y };

		const float depthV0 = mesh.vertices_out[mesh.indices[vIdx0]].position.z;
		const float depthV1 = mesh.vertices_out[mesh.indices[vIdx1]].position.z;
		const float depthV2 = mesh.vertices_out[mesh.indices[vIdx2]].position.z;

		const float wV0 = mesh.vertices_out[mesh.indices[vIdx0]].position.w;
		const float wV1 = mesh.vertices_out[mesh.indices[vIdx1]].position.w;
		const float wV2 = mesh.vertices_out[mesh.indices[vIdx2]].position.w;

		const Vector2 uvV0 = mesh.vertices_out[mesh.indices[vIdx0]].uv;
		const Vector2 uvV1 = mesh.vertices_out[mesh.indices[vIdx1]].uv;
		const Vector2 uvV2 = mesh.vertices_out[mesh.indices[vIdx2]].uv;

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;

		const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

		const INT top = std::max((INT)std::max(v0.y, v1.y), (INT)v2.y);
		const INT bottom = std::min((INT)std::min(v0.y, v1.y), (INT)v2.y);

		const INT left = std::min((INT)std::min(v0.x, v1.x), (INT)v2.x);
		const INT right = std::max((INT)std::max(v0.x, v1.x), (INT)v2.x);

		if (left <= 1 || right >= (m_Width - 1))
			continue;

		if (bottom <= 1 || top >= (m_Height - 1))
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

				float weightV0 = Vector2::Cross(edge12, directionV1);
				if (weightV0 > 0)
					continue;

				float weightV1 = Vector2::Cross(edge20, directionV2);
				if (weightV1 > 0)
					continue;

				float weightV2 = Vector2::Cross(edge01, directionV0);
				if (weightV2 > 0)
					continue;

				weightV0 /= areaTriangle;
				weightV1 /= areaTriangle;
				weightV2 /= areaTriangle;

				if (weightV0 + weightV1 + weightV2 < 1 - FLT_EPSILON
					&& weightV0 + weightV1 + weightV2 > 1 + FLT_EPSILON)
					continue;

				// This Z-BufferValue is the one we compare in the Depth Test and
				// the value we store in the Depth Buffer (uses position.z).
				const float interpolatedZDepthWeight = {
					1.f /
					((1 / depthV0) * weightV0 +
					(1 / depthV1) * weightV1 +
					(1 / depthV2) * weightV2)
				};

				// frustum clipping
				if (interpolatedZDepthWeight < 0 || interpolatedZDepthWeight > 1)
					continue;

				if (interpolatedZDepthWeight > m_pDepthBufferPixels[px + (py * m_Width)])
					continue;

				m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepthWeight;

				switch (m_CurrentDisplayMode)
				{
				case DisplayMode::FinalColor:
				{
					// When we want to interpolate vertex attributes with a correct depth(color, uv, normals, etc.),
					// we still use the View Space depth(uses position.w)
					const float interpolatedWDepthWeight = {
						1.f /
						((1 / wV0) * weightV0 +
						(1 / wV1) * weightV1 +
						(1 / wV2) * weightV2)
					};

					const Vector2 interpolatedUV = {
						((uvV0 / wV0) * weightV0 +
						(uvV1 / wV1) * weightV1 +
						(uvV2 / wV2) * weightV2) * interpolatedWDepthWeight
					};

					finalColor = m_pTukTukTexture->Sample(interpolatedUV);
					break;
				}
				case DisplayMode::DepthBuffer:
				{
					const float depthBufferColor = Remap(m_pDepthBufferPixels[px + (py * m_Width)], 0.985f, 1.0f);

					finalColor = { depthBufferColor, depthBufferColor, depthBufferColor };
					break;
				}
				}

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
bool Renderer::IsInFrustum(const Vertex_Out& v) const
{
	if (v.position.x < -1 || v.position.x > 1)
		return false;

	if (v.position.y < -1 || v.position.y > 1)
		return  false;

	if (v.position.z < 0 || v.position.z > 1)
		return  false;

	return true;
}

void Renderer::NDCToRaster(Vertex_Out& v) const
{
	v.position.x = (v.position.x + 1) * 0.5f * (float)m_Width;
	v.position.y = (1 - v.position.y) * 0.5f * (float)m_Height;
}

void Renderer::TukTukMeshInit()
{
	Utils::ParseOBJ("Resources/tuktuk.obj", m_TukTukMesh.vertices, m_TukTukMesh.indices);

	const Vector3 position{ m_Camera.origin + Vector3{ 0.0f, -3.0f, 15.0f } };
	const Vector3 rotation{ 0,0,0 };
	const Vector3 scale{ 0.5f, 0.5f, 0.5f };

	m_TukTukMesh.worldMatrix = Matrix::CreateScale(scale) * Matrix::CreateRotation(rotation) * Matrix::CreateTranslation(position);
	m_TukTukMesh.primitiveTopology = PrimitiveTopology::TriangleList;
}

void Renderer::VehicleMeshInit()
{
	Utils::ParseOBJ("Resources/vehicle.obj", m_VehicleMesh.vertices, m_VehicleMesh.indices);

	const Vector3 position{ m_Camera.origin + Vector3{ 0.0f, 0.0f, 50.f } };
	const Vector3 rotation{ Vector3{0, 0, 0 } };
	const Vector3 scale{ Vector3{ 1.f, 1.f, 1.f } };

	m_VehicleMesh.worldMatrix = Matrix::CreateScale(scale) * Matrix::CreateRotation(rotation) * Matrix::CreateTranslation(position);
	m_VehicleMesh.primitiveTopology = PrimitiveTopology::TriangleList;
}

void Renderer::SetFovAngle(const float newFovAngle)
{
	m_FovAngle = newFovAngle;

	m_Camera.SetFovOrAspectRatio(m_FovAngle, m_AspectRatio);
}

void Renderer::SetAspectRatio(const float newAspectRatio)
{
	m_AspectRatio = newAspectRatio;

	m_Camera.SetFovOrAspectRatio(m_FovAngle, m_AspectRatio);
}

void dae::Renderer::ClearBackground() const
{
	SDL_FillRect(m_pBackBuffer, nullptr, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::ToggleDisplayMode()
{
	m_CurrentDisplayMode = DisplayMode{ ((int)m_CurrentDisplayMode + 1) % 2 };
}

void Renderer::ToggleShadingMode()
{
	m_CurrentShadingMode = ShadingMode{ ((int)m_CurrentShadingMode + 1) % 4 };
}