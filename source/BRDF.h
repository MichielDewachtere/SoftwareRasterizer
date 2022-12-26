#pragma once
#include "Math.h"
#include "Texture.h"

namespace dae
{
	namespace BRDF
	{
		/**
		 * \param kd Diffuse Reflection Coefficient
		 * \param cd Diffuse Color
		 * \return Lambert Diffuse Color
		 */
		static ColorRGB Lambert(const ColorRGB cd, const float kd = 1)
		{
			const ColorRGB rho = kd * cd;
			return rho / PI;
		}

		static ColorRGB Lambert(const ColorRGB cd, const ColorRGB& kd)
		{
			const ColorRGB rho = kd * cd;
			return rho / PI;
		}

		/**
		 * \brief todo
		 * \param ks Specular Reflection Coefficient
		 * \param exp Phong Exponent
		 * \param l Incoming (incident) Light Direction
		 * \param v View Direction
		 * \param n Normal of the Surface
		 * \return Phong Specular Color
		 */
		static ColorRGB Phong(const ColorRGB ks, const float exp, const Vector3& l, const Vector3& v, const Vector3& n)
		{
			const Vector3 reflect = l - (2 * std::max(Vector3::Dot(n, l), 0.f) * n);
			const float cosAlpha = std::max(Vector3::Dot(reflect, v), 0.f);

			return ks * std::powf(cosAlpha, exp);
		}
	}
}