#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"
#include "DataTypes.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	class Timer;
	class Scene;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render() const;

		bool SaveBufferToImage() const;

	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};
		float m_AspectRatio{};

		//Function that transforms the vertices from the mesh from World space to Screen space
		void VertexTransformationFunction_W1(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const; //W1 Version
		void VertexTransformationFunction_W2(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const; //W1 Version

		void Render_W1_Part1() const;
		void Render_W1_Part2() const;
		void Render_W1_Part3() const;
		void Render_W1_Part4() const;
		void Render_W1_Part5() const;

		void Render_W2_Part1() const;
		void Render_W2_Part2() const;
		void Render_W2_Part3() const;
		void Render_W2_Part4() const;

		void ClearBackground() const;
	};
}
