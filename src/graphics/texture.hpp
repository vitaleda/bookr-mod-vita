/*
 * bookr-modern: a graphics based document reader 
 * Copyright (C) 2019 pathway27 (Sree)
 * IS A MODIFICATION OF THE ORIGINAL
 * Bookr and bookr-mod for PSP
 * Copyright (C) 2005 Carlos Carrasco Martinez (carloscm at gmail dot com),
 *               2007 Christian Payeur (christian dot payeur at gmail dot com),
 *               2009 Nguyen Chi Tam (nguyenchitam at gmail dot com),
 * AND VARIOUS OTHER FORKS, See Forks in README.md
 * Licensed under GPLv3+, see LICENSE
*/

#ifndef FZTEXTURE_H
#define FZTEXTURE_H

#ifdef MAC
	#include <SOIL.h>
#elif __vita__
	#include <vita2d.h>
#endif
#include "image.hpp"

namespace bookr {

/**
 * Represents a 2D texture.
 */
class Texture : public RefCounted {
	// common
	unsigned int width, height;
	int texenv, texMin, texMag;

	// psp
	unsigned int pixelFormat;
	unsigned int pixelComponent;
	Image* texImage;
	//void* imageData;
	//void* clutData;

	// ogl
	unsigned int textureObject;
	int internalFormat;
	int textureFormat;
	int textureType;

	bool validateFormat(Image*);

protected:
	Texture();
	static bool initFromImage(Texture* texture, Image* image, bool buildMipmaps);

public:
	// vita
	// refactor to fzimage with void*
#ifdef __vita__
		vita2d_texture* vita_texture;
#endif
	unsigned char*  soil_data;

	~Texture();

	/**
	 * Set texture op for combining with the current fragment color.
	 */
	void texEnv(int op);

	/**
	 * Set texture filtering.
	 */
	void filter(int min, int max);

	/**
	 * Make the texture the current texture. NOTE: remove this method ASAP
	 */
	void bind();

	/**
	 * Make the texture the current texture and reissue the declaration/upload.
	 */
	void bindForDisplay();

	/**
	 * Create a new 2D texture object from an image.
	 */
	static Texture* createFromImage(Image* image, bool buildMipmaps);

	//refactor
	#ifdef __vita__
		static Texture* createFromVitaTexture(vita2d_texture * texture);
	#endif

	static Texture* createFromBuffer(const void * buffer);
	static Texture* createFromSOIL(char* imagePath);

	unsigned int getWidth() const { return width; }
	unsigned int getHeight() const { return height; }
};

}

#endif
