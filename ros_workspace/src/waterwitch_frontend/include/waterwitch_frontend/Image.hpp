#ifndef IMAGE_HPP
#define IMAGE_HPP

#define STB_IMAGE_IMPLEMENTATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stb_image.h>

unsigned int loadEmbeddedTexture(const unsigned char* data, unsigned int length) {
  int width, height, channels;
  unsigned char* imageData = stbi_load_from_memory(data, length, &width, &height, &channels, 4);
  if (!imageData) {
      std::cerr << "Failed to load embedded texture." << std::endl;
      return 0;
  }

  unsigned int textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(imageData);
  return textureID;
}

#endif