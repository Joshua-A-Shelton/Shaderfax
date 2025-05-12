#include "DescriptorSet.h"

#include <stdexcept>

void DescriptorSet::addAutomaticallyIntroducedUniformBuffer(const char* name)
{
    _descriptors.push_back({.name=name,.type = UNIFORM_BUFFER,.index = 0,.count = 1});
}

void DescriptorSet::addDescriptorRanges(slang::TypeLayoutReflection* typeLayout)
{
    int relativeSetIndex = 0;
    int rangeCount = typeLayout->getDescriptorSetDescriptorRangeCount(relativeSetIndex);

    for (int rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex)
    {
        addDescriptorRange(
            typeLayout,
            relativeSetIndex,
            rangeIndex);
    }
}

void DescriptorSet::addDescriptorRange(slang::TypeLayoutReflection* typeLayout, int relativeSetIndex, int rangeIndex)
{
    slang::BindingType bindingType = typeLayout->getDescriptorSetDescriptorRangeType(relativeSetIndex, rangeIndex);
    auto descriptorCount = typeLayout->getDescriptorSetDescriptorRangeDescriptorCount(relativeSetIndex, rangeIndex);



    size_t bindingIndex = 0;
    if (_descriptors.size() > 0)
    {
        auto& last = _descriptors.at(_descriptors.size()-1);
        bindingIndex = last.index+last.count;
    }

    auto n = typeLayout->getFieldByIndex(bindingIndex)->getName();

    Descriptor descriptor{};
    descriptor.name = n;
    descriptor.index = bindingIndex;
    descriptor.count = descriptorCount;
    switch (bindingType)
    {
        case slang::BindingType::Sampler:
            descriptor.type = SAMPLER;
            break;
        case slang::BindingType::Texture:
            descriptor.type = SAMPLED_TEXTURE;
            break;
        case slang::BindingType::CombinedTextureSampler:
            descriptor.type = SAMPLER_AND_TEXTURE;
            break;
        case slang::BindingType::MutableTexture:
            descriptor.type = STORAGE_TEXTURE;
            break;
        case slang::BindingType::TypedBuffer:
            descriptor.type = UNIFORM_TEXEL_BUFFER;
            break;
        case slang::BindingType::MutableTypedBuffer:
            descriptor.type = STORAGE_TEXEL_BUFFER;
            break;
        case slang::BindingType::ConstantBuffer:
            descriptor.type = UNIFORM_BUFFER;
            break;
        case slang::BindingType::RawBuffer:
            descriptor.type = STORAGE_BUFFER;
            break;
        case slang::BindingType::InputRenderTarget:
            descriptor.type = INPUT_ATTACHMENT;
            break;
        case slang::BindingType::RayTracingAccelerationStructure:
            descriptor.type = ACCELERATION_STRUCTURE;
            break;
        default:
            throw std::invalid_argument("Invalid binding type");
    }
    _descriptors.push_back(descriptor);

}

DescriptorSet::DescriptorSet(const char* name, slang::TypeLayoutReflection* elementTypeLayout, size_t index)
{
    _index = index;

    if(elementTypeLayout->getSize() > 0)
    {
        addAutomaticallyIntroducedUniformBuffer(name);
    }

    addDescriptorRanges(elementTypeLayout);
}

size_t DescriptorSet::descriptorCount()
{
    return _descriptors.size();
}

Descriptor& DescriptorSet::at(size_t index)
{
    return _descriptors.at(index);
}

size_t DescriptorSet::index()
{
    return _index;
}
