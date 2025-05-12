#ifndef SHADERFAX_DESCRIPTORSET_H
#define SHADERFAX_DESCRIPTORSET_H
#include <string>
#include <vector>

#include <slang.h>


enum DescriptorType
{
  ///Object that selects what texels to select from texture (layer, mip, etc)
  SAMPLER,
  ///Texure that requires a corresponding sampler to read
  SAMPLED_TEXTURE,
  ///Object that encapsulates both the texture and it's corresponding sampler
  SAMPLER_AND_TEXTURE,
  ///Texture that can have both be read on write operations can be perfomed on in the same shader (Generally GPU shaders)
  STORAGE_TEXTURE,
  ///Tightly packed 1D array of texels that image sampling operations can be performed on
  UNIFORM_TEXEL_BUFFER,
  ///Tighly packed 1D array of texels that image both read and write operations can be performed on in the same shader (Generally GPU shaders)
  STORAGE_TEXEL_BUFFER,
  ///Represents a section of a buffer that contains arbitrary data
  UNIFORM_BUFFER,
  ///Represents a section of a buffer that contains arbitrary data that both read and write operations can be performed on in the same shader (Generally GPU shaders or unsized arrays in pixel shaders)
  STORAGE_BUFFER,
  ///Texture that can be used for framebuffer local operations
  INPUT_ATTACHMENT,
  ///Object that is used in ray tracing and intersection testing
  ACCELERATION_STRUCTURE
};
struct Descriptor
{
  std::string name;
  DescriptorType type=UNIFORM_BUFFER;
  size_t index=0;
  size_t count=0;
};
class DescriptorSet 
{
private:
  size_t _index = SIZE_MAX;
  std::vector<Descriptor> _descriptors;
  void addAutomaticallyIntroducedUniformBuffer(const char* name);
  void addDescriptorRanges(slang::TypeLayoutReflection* typeLayout);
  void addDescriptorRange(slang::TypeLayoutReflection* typeLayout,int relativeSetIndex,int rangeIndex);
public:
  DescriptorSet(const char* name,slang::TypeLayoutReflection* elementTypeLayout, size_t index);
  size_t descriptorCount();
  Descriptor& at(size_t index);
  size_t index();

};

#endif //SHADERFAX_DESCRIPTORSET_H