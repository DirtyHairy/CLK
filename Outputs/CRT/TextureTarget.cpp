//
//  TextureTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TextureTarget.hpp"

using namespace OpenGL;

TextureTarget::TextureTarget(unsigned int width, unsigned int height)
{
	glGenFramebuffers(1, &_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

	glGenTextures(1, &_texture);
	glBindTexture(GL_TEXTURE_2D, _texture);

	uint8_t *emptySpace = new uint8_t[width*height*4];
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptySpace);
	delete[] emptySpace;

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);

	// TODO: raise an exception if check framebuffer status fails.
//		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
//			return nil;
}

TextureTarget::~TextureTarget()
{
	glDeleteFramebuffers(1, &_framebuffer);
	glDeleteTextures(1, &_texture);
}

void TextureTarget::bind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
}

void TextureTarget::bind_texture()
{
	glBindTexture(GL_TEXTURE_2D, _texture);
}
