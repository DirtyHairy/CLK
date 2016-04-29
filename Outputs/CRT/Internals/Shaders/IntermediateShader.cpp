//
//  IntermediateShader.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "IntermediateShader.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../../../SignalProcessing/FIRFilter.hpp"

using namespace OpenGL;

namespace {
	const OpenGL::Shader::AttributeBinding bindings[] =
	{
		{"inputPosition", 0},
		{"outputPosition", 1},
		{"phaseAmplitudeAndOffset", 2},
		{"phaseTime", 3},
		{nullptr}
	};
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_shader(const char *fragment_shader, bool use_usampler, bool input_is_inputPosition)
{
	const char *sampler_type = use_usampler ? "usampler2D" : "sampler2D";
	const char *input_variable = input_is_inputPosition ? "inputPosition" : "outputPosition";

	char *vertex_shader;
	asprintf(&vertex_shader,
		"#version 150\n"

		"in vec2 inputPosition;"
		"in vec2 outputPosition;"
		"in vec3 phaseAmplitudeAndOffset;"
		"in float phaseTime;"

		"uniform float phaseCyclesPerTick;"
		"uniform ivec2 outputTextureSize;"
		"uniform float extension;"
		"uniform %s texID;"

		"out vec2 phaseAndAmplitudeVarying;"
		"out vec2 inputPositionsVarying[11];"
		"out vec2 iInputPositionVarying;"
		"out vec2 delayLinePositionVarying;"

		"void main(void)"
		"{"
			"vec2 extensionVector = vec2(extension, 0.0) * 2.0 * (phaseAmplitudeAndOffset.z - 0.5);"
			"vec2 extendedInputPosition = %s + extensionVector;"
			"vec2 extendedOutputPosition = outputPosition + extensionVector;"

			"vec2 textureSize = vec2(textureSize(texID, 0));"
			"iInputPositionVarying = extendedInputPosition;"
			"vec2 mappedInputPosition = (extendedInputPosition + vec2(0.0, 0.5)) / textureSize;"

			"inputPositionsVarying[0] = mappedInputPosition - (vec2(10.0, 0.0) / textureSize);"
			"inputPositionsVarying[1] = mappedInputPosition - (vec2(8.0, 0.0) / textureSize);"
			"inputPositionsVarying[2] = mappedInputPosition - (vec2(6.0, 0.0) / textureSize);"
			"inputPositionsVarying[3] = mappedInputPosition - (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[4] = mappedInputPosition - (vec2(2.0, 0.0) / textureSize);"
			"inputPositionsVarying[5] = mappedInputPosition;"
			"inputPositionsVarying[6] = mappedInputPosition + (vec2(2.0, 0.0) / textureSize);"
			"inputPositionsVarying[7] = mappedInputPosition + (vec2(4.0, 0.0) / textureSize);"
			"inputPositionsVarying[8] = mappedInputPosition + (vec2(6.0, 0.0) / textureSize);"
			"inputPositionsVarying[9] = mappedInputPosition + (vec2(8.0, 0.0) / textureSize);"
			"inputPositionsVarying[10] = mappedInputPosition + (vec2(10.0, 0.0) / textureSize);"
			"delayLinePositionVarying = mappedInputPosition - vec2(0.0, 1.0);"

			"phaseAndAmplitudeVarying.x = (phaseCyclesPerTick * (extendedOutputPosition.x - phaseTime) + phaseAmplitudeAndOffset.x) * 2.0 * 3.141592654;"
			"phaseAndAmplitudeVarying.y = 0.33;"

			"vec2 eyePosition = 2.0*(extendedOutputPosition / outputTextureSize) - vec2(1.0) + vec2(0.5)/textureSize;"
			"gl_Position = vec4(eyePosition, 0.0, 1.0);"
		"}", sampler_type, input_variable);

	std::unique_ptr<IntermediateShader> shader = std::unique_ptr<IntermediateShader>(new IntermediateShader(vertex_shader, fragment_shader, bindings));
	free(vertex_shader);

	shader->texIDUniform				= shader->get_uniform_location("texID");
	shader->outputTextureSizeUniform	= shader->get_uniform_location("outputTextureSize");
	shader->phaseCyclesPerTickUniform	= shader->get_uniform_location("phaseCyclesPerTick");
	shader->extensionUniform			= shader->get_uniform_location("extension");
	shader->weightsUniform				= shader->get_uniform_location("weights");
	shader->rgbToLumaChromaUniform		= shader->get_uniform_location("rgbToLumaChroma");
	shader->lumaChromaToRGBUniform		= shader->get_uniform_location("lumaChromaToRGB");

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_source_conversion_shader(const char *composite_shader, const char *rgb_shader)
{
	char *composite_sample = (char *)composite_shader;
	if(!composite_sample)
	{
		asprintf(&composite_sample,
			"%s\n"
			"uniform mat3 rgbToLumaChroma;"
			"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
			"{"
				"vec3 rgbColour = clamp(rgb_sample(texID, coordinate, iCoordinate), vec3(0.0), vec3(1.0));"
				"vec3 lumaChromaColour = rgbToLumaChroma * rgbColour;"
				"vec2 quadrature = vec2(cos(phase), -sin(phase)) * amplitude;"
				"return dot(lumaChromaColour, vec3(1.0 - amplitude, quadrature));"
			"}",
			rgb_shader);
	}

	char *fragment_shader;
	asprintf(&fragment_shader,
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"in vec2 iInputPositionVarying;"
		"in vec2 phaseAndAmplitudeVarying;"

		"out vec4 fragColour;"

		"uniform usampler2D texID;"

		"\n%s\n"

		"void main(void)"
		"{"
			"fragColour = vec4(composite_sample(texID, inputPositionsVarying[5], iInputPositionVarying, phaseAndAmplitudeVarying.x, phaseAndAmplitudeVarying.y));"
		"}"
	, composite_sample);
	if(!composite_shader) free(composite_sample);

	std::unique_ptr<IntermediateShader> shader = make_shader(fragment_shader, true, true);
	free(fragment_shader);

	return shader;
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_luma_separation_shader()
{
	return make_shader(
		"#version 150\n"

		"in vec2 phaseAndAmplitudeVarying;"
		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"

		"void main(void)"
		"{"
			"vec4 samples[3] = vec4[]("
				"vec4("
					"texture(texID, inputPositionsVarying[0]).r,"
					"texture(texID, inputPositionsVarying[1]).r,"
					"texture(texID, inputPositionsVarying[2]).r,"
					"texture(texID, inputPositionsVarying[3]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[4]).r,"
					"texture(texID, inputPositionsVarying[5]).r,"
					"texture(texID, inputPositionsVarying[6]).r,"
					"texture(texID, inputPositionsVarying[7]).r"
				"),"
				"vec4("
					"texture(texID, inputPositionsVarying[8]).r,"
					"texture(texID, inputPositionsVarying[9]).r,"
					"texture(texID, inputPositionsVarying[10]).r,"
					"0.0"
				")"
			");"

			"float luminance = "
				"dot(vec3("
					"dot(samples[0], weights[0]),"
					"dot(samples[1], weights[1]),"
					"dot(samples[2], weights[2])"
				"), vec3(1.0)) / (1.0 - phaseAndAmplitudeVarying.y);"

			"float chrominance = 0.5 * (samples[1].y - luminance) / phaseAndAmplitudeVarying.y;"
			"vec2 quadrature = vec2(cos(phaseAndAmplitudeVarying.x), -sin(phaseAndAmplitudeVarying.x));"

			"fragColour = vec3(luminance, vec2(0.5) + (chrominance * quadrature));"
		"}",false, false);
}

std::unique_ptr<IntermediateShader> IntermediateShader::make_chroma_filter_shader()
{
	return make_shader(
		"#version 150\n"

		"in vec2 inputPositionsVarying[11];"
		"uniform vec4 weights[3];"

		"out vec3 fragColour;"

		"uniform sampler2D texID;"
		"uniform mat3 lumaChromaToRGB;"

		"void main(void)"
		"{"
			"vec3 centreSample = texture(texID, inputPositionsVarying[5]).rgb;"
			"vec2 samples[] = vec2[]("
				"texture(texID, inputPositionsVarying[0]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[1]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[2]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[3]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[4]).gb - vec2(0.5),"
				"centreSample.gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[6]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[7]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[8]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[9]).gb - vec2(0.5),"
				"texture(texID, inputPositionsVarying[10]).gb - vec2(0.5)"
			");"

			"vec4 channel1[] = vec4[]("
				"vec4(samples[0].r, samples[1].r, samples[2].r, samples[3].r),"
				"vec4(samples[4].r, samples[5].r, samples[6].r, samples[7].r),"
				"vec4(samples[8].r, samples[9].r, samples[10].r, 0.0)"
			");"
			"vec4 channel2[] = vec4[]("
				"vec4(samples[0].g, samples[1].g, samples[2].g, samples[3].g),"
				"vec4(samples[4].g, samples[5].g, samples[6].g, samples[7].g),"
				"vec4(samples[8].g, samples[9].g, samples[10].g, 0.0)"
			");"

			"vec3 lumaChromaColour = vec3(centreSample.r,"
				"dot(vec3("
					"dot(channel1[0], weights[0]),"
					"dot(channel1[1], weights[1]),"
					"dot(channel1[2], weights[2])"
				"), vec3(1.0)) + 0.5,"
				"dot(vec3("
					"dot(channel2[0], weights[0]),"
					"dot(channel2[1], weights[1]),"
					"dot(channel2[2], weights[2])"
				"), vec3(1.0)) + 0.5"
			");"

			"vec3 lumaChromaColourInRange = (lumaChromaColour - vec3(0.0, 0.5, 0.5)) * vec3(1.0, 2.0, 2.0);"
			"fragColour = lumaChromaToRGB * lumaChromaColourInRange;"
		"}", false, false);
}

void IntermediateShader::set_output_size(unsigned int output_width, unsigned int output_height)
{
	bind();
	glUniform2i(outputTextureSizeUniform, (GLint)output_width, (GLint)output_height);
}

void IntermediateShader::set_source_texture_unit(GLenum unit)
{
	bind();
	glUniform1i(texIDUniform, (GLint)(unit - GL_TEXTURE0));
}

void IntermediateShader::set_filter_coefficients(float sampling_rate, float cutoff_frequency)
{
	bind();

	sampling_rate *= 0.5f;
	cutoff_frequency *= 0.5f;

	float weights[12];
	weights[11] = 0.0f;
	SignalProcessing::FIRFilter luminance_filter(11, sampling_rate, 0.0f, cutoff_frequency, SignalProcessing::FIRFilter::DefaultAttenuation);
	luminance_filter.get_coefficients(weights);
	glUniform4fv(weightsUniform, 3, weights);
}

void IntermediateShader::set_phase_cycles_per_sample(float phase_cycles_per_sample, bool extend_runs_to_full_cycle)
{
	bind();
	glUniform1f(phaseCyclesPerTickUniform, phase_cycles_per_sample);
	glUniform1f(extensionUniform, extend_runs_to_full_cycle ? ceilf(1.0f / phase_cycles_per_sample) : 0.0f);
}

void IntermediateShader::set_colour_conversion_matrices(float *fromRGB, float *toRGB)
{
	bind();
	glUniformMatrix3fv(lumaChromaToRGBUniform, 1, GL_FALSE, toRGB);
	glUniformMatrix3fv(rgbToLumaChromaUniform, 1, GL_FALSE, fromRGB);
}
