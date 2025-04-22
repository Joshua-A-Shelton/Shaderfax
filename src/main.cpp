#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/endian/conversion.hpp>
using namespace slang;
namespace po = boost::program_options;

enum attributeFlags: uint16_t
{
    POSITION3D=0b0000000000000001,
    POSITION2D=0b0000000000000010,
    NORMAL=0b0000000000000100,
    UVCOORDS=0b0000000000001000,
    VERTEXCOLOR=0b0000000000010000,
    BONEWIEGHTS=0b0000000000100000

};
enum ShaderType
{
    UNKNOWN=0,
    GRAPHICS,
    COMPUTE,
    RAY
};
enum GeometryPipelineType
{
    NA,
    VERTEX,
    MESH
};


bool getModules(std::vector<IModule*>& modules,std::filesystem::path& root,Slang::ComPtr<ISession>& session);
bool isSameShaderType(ShaderType& existingType, ShaderType comparisonType, GeometryPipelineType& existingPipeline, GeometryPipelineType comparisonPipeline, const std::filesystem::path& currentFile);
bool isValidColorTarget(const std::string& colorTarget);
bool isValidDepthTarget(const std::string & depthTarget);

std::vector<std::string> getVertexParameters(FunctionReflection* reflection);
std::vector<std::string> getHullParameters(FunctionReflection* reflection);
std::vector<std::string> getDomainParameters(FunctionReflection* reflection);
std::vector<std::string> getGeometryParameters(FunctionReflection* reflection);
bool getFragmentParameters(FunctionReflection* reflection, std::vector<std::string>& parameters, const std::filesystem::path& currentFile);
std::vector<std::string> getComputeParameters(FunctionReflection* reflection);
std::vector<std::string> getRayGenerationParameters(FunctionReflection* reflection);
std::vector<std::string> getIntersectionParameters(FunctionReflection* reflection);
std::vector<std::string> getAnyHitParameters(FunctionReflection* reflection);
std::vector<std::string> getClosestHitParameters(FunctionReflection* reflection);
std::vector<std::string> getMissParameters(FunctionReflection* reflection);
std::vector<std::string> getCallableParameters(FunctionReflection* reflection);
std::vector<std::string> getMeshParameters(FunctionReflection* reflection);
std::vector<std::string> getAmplificationParameters(FunctionReflection* reflection);

struct ShaderOutData
{
    std::string stage;
    std::vector<std::string> parameters;
    Slang::ComPtr<IBlob> spirvCode=nullptr;
};

int main(int argc, char**argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message")
    ("root,r", po::value<std::string>(),"Top level folder containing shader files")
    ("output,o", po::value<std::string>()->default_value("output"),"Output folder for compiled files")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc,argv,desc),vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    std::filesystem::path root = vm["root"].as<std::string>();
    std::filesystem::path output = vm["output"].as<std::string>();



    Slang::ComPtr<IGlobalSession> globalSession;
    SlangGlobalSessionDesc globalDesc{};
    createGlobalSession(&globalDesc,globalSession.writeRef());

    std::string absRoot = absolute(root).string();
    const char* rootPath = absRoot.c_str();
    TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;

    SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.flags = kSessionFlags_None;
    sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
    sessionDesc.searchPaths = &rootPath;
    sessionDesc.searchPathCount = 1;
    sessionDesc.preprocessorMacros = nullptr;
    sessionDesc.preprocessorMacroCount = 0;
    sessionDesc.fileSystem = nullptr;
    sessionDesc.enableEffectAnnotations = false;
    sessionDesc.enableEffectAnnotations = false;



    Slang::ComPtr<ISession> session;
    globalSession->createSession(sessionDesc, session.writeRef());
    std::vector<IModule*> modules;

    auto success = getModules(modules,root,session);
    if (!success)
    {
        return EXIT_FAILURE;
    }

    std::unordered_map<std::string,std::vector<ShaderOutData>> shaderWriteData;
    for (auto i=0; i< modules.size(); i++)
    {
        auto module = modules[i];
        std::filesystem::path file = module->getFilePath();
        file = absolute(file);
        auto relative = std::filesystem::relative(file,root).replace_extension(".cshdr");


        if (shaderWriteData.contains(relative.string()))
        {
            std::cerr << "Shader is duplicating relative file path: "<< file << std::endl;
            return EXIT_FAILURE;
        }

        auto insertData = shaderWriteData.insert({relative.string(),{}});
        auto& stages = insertData.first->second;

        for (auto entryPointIndex = 0; entryPointIndex < module->getDefinedEntryPointCount(); entryPointIndex++)
        {
            Slang::ComPtr<IEntryPoint> entryPoint = nullptr;
            module->getDefinedEntryPoint(entryPointIndex,entryPoint.writeRef());
            auto reflection =entryPoint->getFunctionReflection();
            auto funcName = reflection->getName();
            auto layout = entryPoint->getLayout();
            auto ep = layout->findEntryPointByName(funcName);
            auto stage = ep->getStage();
            std::string stageName = "";
            std::vector<std::string> parameters;
            ShaderType currentType = ShaderType::UNKNOWN;
            GeometryPipelineType pipelineType = GeometryPipelineType::NA;

            switch (stage)
            {
                case SLANG_STAGE_NONE:
                    std::cerr << "encountered unknown entry point stage"<< module->getFilePath()<<": "<<reflection->getName()<<"\n";
                    return EXIT_FAILURE;
                    break;
                case SLANG_STAGE_VERTEX:
                    if (!isSameShaderType(currentType,ShaderType::GRAPHICS,pipelineType,GeometryPipelineType::VERTEX,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "vertex";
                    parameters = getVertexParameters(reflection);
                    break;
                case SLANG_STAGE_HULL:
                    if (!isSameShaderType(currentType,ShaderType::GRAPHICS,pipelineType,GeometryPipelineType::VERTEX,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "hull";
                    parameters = getHullParameters(reflection);
                    break;
                case SLANG_STAGE_DOMAIN:
                    if (!isSameShaderType(currentType,ShaderType::GRAPHICS,pipelineType,GeometryPipelineType::VERTEX,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "domain";
                    parameters = getDomainParameters(reflection);
                    break;
                case SLANG_STAGE_GEOMETRY:
                    if (!isSameShaderType(currentType,ShaderType::GRAPHICS,pipelineType,GeometryPipelineType::VERTEX,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "geometry";
                    parameters = getGeometryParameters(reflection);
                    break;
                case SLANG_STAGE_FRAGMENT:
                    if (!isSameShaderType(currentType,ShaderType::GRAPHICS,pipelineType,GeometryPipelineType::VERTEX,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "fragment";
                    if (!getFragmentParameters(reflection,parameters,file))
                    {
                        return EXIT_FAILURE;
                    }
                    break;
                case SLANG_STAGE_COMPUTE:
                    if (!isSameShaderType(currentType,ShaderType::COMPUTE,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "compute";
                    parameters = getComputeParameters(reflection);
                    break;
                case SLANG_STAGE_RAY_GENERATION:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "rayGeneration";
                    parameters = getRayGenerationParameters(reflection);
                    break;
                case SLANG_STAGE_INTERSECTION:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "intersection";
                    parameters = getIntersectionParameters(reflection);
                    break;
                case SLANG_STAGE_ANY_HIT:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "anyHit";
                    parameters = getAnyHitParameters(reflection);
                    break;
                case SLANG_STAGE_CLOSEST_HIT:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "closestHit";
                    parameters = getClosestHitParameters(reflection);
                    break;
                case SLANG_STAGE_MISS:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "miss";
                    parameters = getMissParameters(reflection);
                    break;
                case SLANG_STAGE_CALLABLE:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::NA,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "callable";
                    parameters = getCallableParameters(reflection);
                    break;
                case SLANG_STAGE_MESH:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::MESH,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "mesh";
                    parameters = getMeshParameters(reflection);
                    break;
                case SLANG_STAGE_AMPLIFICATION:
                    if (!isSameShaderType(currentType,ShaderType::RAY,pipelineType,GeometryPipelineType::MESH,module->getFilePath())){return EXIT_FAILURE;}
                    stageName = "task";
                    parameters = getAmplificationParameters(reflection);
                    break;
                default:
                    std::cerr << "encountered unknown entry point stage"<< module->getFilePath()<<": "<<reflection->getName()<<"\n";
                    return EXIT_FAILURE;
                    break;

            }

            Slang::ComPtr<IComponentType> componentType;
            Slang::ComPtr<IBlob> diagnostics;
            entryPoint->link(componentType.writeRef(),diagnostics.writeRef());
            if (diagnostics.get())
            {
                std::cerr<<(char*)diagnostics->getBufferPointer()<<"\n";
                return EXIT_FAILURE;
            }
            Slang::ComPtr<IBlob> spirv = nullptr;
            diagnostics = nullptr;
            componentType->getTargetCode(0,spirv.writeRef(),diagnostics.writeRef());
            if (diagnostics != nullptr)
            {
                std::cerr<<(char*)diagnostics->getBufferPointer()<<"\n";
                return EXIT_FAILURE;
            }
            diagnostics = nullptr;
            stages.push_back({.stage = stageName,.parameters = std::move(parameters),.spirvCode = spirv});
        }

    }
    std::filesystem::remove_all(output);
    std::filesystem::create_directory(output);
    for (auto& kvpair: shaderWriteData)
    {
        std::string relativeName = kvpair.first;
        std::filesystem::path file = output/relativeName;
        auto& writeData = kvpair.second;
        std::vector<char> data;
        data.push_back('c');
        data.push_back('s');
        data.push_back('h');
        data.push_back('d');
        data.push_back('r');
        data.push_back('\n');
        for (auto stageIndex=0; stageIndex<writeData.size(); ++stageIndex)
        {
            auto& stage = writeData[stageIndex];
            for (auto i=0; i<stage.stage.size(); ++i)
            {
                data.push_back(stage.stage[i]);
            }
            data.push_back(':');
            data.push_back('<');
            for (auto i=0; i<stage.parameters.size(); ++i)
            {
                auto& parameter = stage.parameters[i];
                for (auto j=0; j<parameter.size(); ++j)
                {
                    data.push_back(parameter[j]);
                }
                if (i<stage.parameters.size()-1)
                {
                    data.push_back(',');
                }
            }
            data.push_back('>');
            uint32_t bufferSize = stage.spirvCode->getBufferSize();
            if constexpr (std::endian::native == std::endian::big)
            {
                boost::endian::little_to_native_inplace(bufferSize);
            }
            char* location = (char*)&bufferSize;
            for (auto byte=0; byte<sizeof(bufferSize); ++byte)
            {
                data.push_back(location[byte]);
            }
            char* bufferBegin = (char*)stage.spirvCode->getBufferPointer();
            for (auto currentByte = 0; currentByte<stage.spirvCode->getBufferSize(); ++currentByte)
            {
                data.push_back(bufferBegin[currentByte]);
            }
        }
        auto directory = file.parent_path();
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directory(directory);
        }
        std::ofstream outFile(file, std::ios::trunc|std::ios::binary);
        if (outFile.is_open())
        {
            outFile.write(data.data(),data.size());
            outFile.close();
        }
        else
        {
            std::cerr<< "Unable to write to file "<<file<<"\n";
            return EXIT_FAILURE;
        }

    }
    return 0;
}

bool getModules(std::vector<IModule*>& modules,std::filesystem::path& root,Slang::ComPtr<ISession>& session)
{
    using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
    for (const auto& dirEntry : recursive_directory_iterator(root))
    {
        if (dirEntry.is_regular_file())
        {
            auto path = dirEntry.path();
            path = std::filesystem::relative(path, root);
            if (path.extension() == ".slang")
            {
                Slang::ComPtr<IBlob> diagnostics;
                IModule* module = session->loadModule(path.string().c_str(),diagnostics.writeRef());
                if (module && module->getDefinedEntryPointCount())
                {
                    bool compute = false;
                    bool graphics = false;
                    for (auto i=0; i< module->getDefinedEntryPointCount(); i++)
                    {
                        IEntryPoint* entryPoint = nullptr;
                        module->getDefinedEntryPoint(i,&entryPoint);
                        auto reflection =entryPoint->getFunctionReflection();
                        auto funcName = reflection->getName();
                        auto layout = entryPoint->getLayout();
                        auto ep = layout->findEntryPointByName(funcName);
                        auto stage = ep->getStage();
                        if (stage == SLANG_STAGE_COMPUTE)
                        {
                            compute = true;
                        }
                        else
                        {
                            graphics = true;
                        }
                    }
                    if (compute && graphics)
                    {
                        std::cerr << path << " contains both compute and graphics entry points\n";
                        return false;
                    }
                    else
                    {
                        modules.push_back(module);
                    }
                }

                if (diagnostics)
                {
                    std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
                    return false;
                }
            }
        }
    }
    return true;
}

bool isSameShaderType(ShaderType& existingType, ShaderType comparisonType, GeometryPipelineType& existingPipeline, GeometryPipelineType comparisonPipeline, const std::filesystem::path& currentFile)
{
    if (existingType == ShaderType::UNKNOWN)
    {
        existingType = comparisonType;
        existingPipeline = comparisonPipeline;
        return true;
    }
    else
    {
        if (existingType == comparisonType)
        {
            if (comparisonPipeline == GeometryPipelineType::NA)
            {
                return true;
            }
            else if (existingPipeline == GeometryPipelineType::NA)
            {
                existingPipeline = comparisonPipeline;
                return true;
            }
            else
            {
                std::cerr << "Shader defines multiple pipeline geometry execution paths";
                return false;
            }

        }
        std::cerr << "Shader defines multiple pipelines: "<<currentFile<<"\n";
        return false;
    }
}


std::vector<std::string> getVertexParameters(FunctionReflection* reflection)
{
    std::vector<std::string> parameters;
    auto parameterCount = reflection->getParameterCount();
    for (auto i = 0; i < parameterCount; i++)
    {
        auto parameter = reflection->getParameterByIndex(i);
        std::string inputType = parameter->getType()->getName();
        if (inputType == "Vertex3D")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "Vertex2D")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "Normal")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "Tangent")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "UVCoordinates")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "VertexColor")
        {
            parameters.push_back(inputType);
        }
        else if (inputType == "BoneWeights")
        {
            parameters.push_back(inputType);
        }
    }

    return parameters;
}
std::vector<std::string> getHullParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getDomainParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getGeometryParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
bool getFragmentParameters(FunctionReflection* reflection, std::vector<std::string>& parameters, const std::filesystem::path& currentFile)
{
    auto attributeCount = reflection->getUserAttributeCount();
    if (attributeCount)
    {
        for (auto i = 0; i < attributeCount; i++)
        {
            auto attribute = reflection->getUserAttributeByIndex(i);
            if (attribute->getName()=="targets")
            {
                auto argCount = attribute->getArgumentCount();
                bool foundDepth = false;
                for (auto j = 0; j < argCount; j++)
                {
                    size_t size = 0;
                    auto targetPointer = attribute->getArgumentValueString(j,&size);
                    std::string target(targetPointer,size);
                    if (isValidColorTarget(target))
                    {
                        parameters.push_back(target);
                    }
                    else if (isValidDepthTarget(target))
                    {
                        if (!foundDepth)
                        {
                            foundDepth = true;
                            parameters.push_back(target);
                        }
                        else
                        {
                            std::cerr << "Multiple Depth targets defined for fragment stage: "<<currentFile<<"\n";
                            return false;
                        }
                    }
                    else
                    {
                        std::cerr << "Unknown target format defined in fragment stage ("<<target<<"): " << currentFile<<"\n";
                        return false;
                    }
                }
            }
        }
    }
    else
    {
        parameters.push_back("R8G8B8A8_UNORM");
        parameters.push_back("D32_FLOAT");
    }
    return true;
}
std::vector<std::string> getComputeParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getRayGenerationParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getIntersectionParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getAnyHitParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getClosestHitParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getMissParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getCallableParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getMeshParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}
std::vector<std::string> getAmplificationParameters(FunctionReflection* reflection)
{
    return std::vector<std::string>();
}

bool isValidColorTarget(const std::string& colorTarget)
{
    if( colorTarget == "R32G32B32A32_FLOAT"){return true;}
    else if( colorTarget == "R32G32B32A32_UINT"){return true;}
    else if( colorTarget == "R32G32B32A32_SINT"){return true;}
    else if( colorTarget == "R32G32B32_FLOAT"){return true;}
    else if( colorTarget == "R32G32B32_UINT"){return true;}
    else if( colorTarget == "R32G32B32_SINT"){return true;}
    else if( colorTarget == "R16G16B16A16_FLOAT"){return true;}
    else if( colorTarget == "R16G16B16A16_UNORM"){return true;}
    else if( colorTarget == "R16G16B16A16_UINT"){return true;}
    else if( colorTarget == "R16G16B16A16_SNORM"){return true;}
    else if( colorTarget == "R16G16B16A16_SINT"){return true;}
    else if( colorTarget == "R32G32_FLOAT"){return true;}
    else if( colorTarget == "R32G32_UINT"){return true;}
    else if( colorTarget == "R32G32_SINT"){return true;}
    else if( colorTarget == "R10G10B10A2_UNORM"){return true;}
    else if( colorTarget == "R10G10B10A2_UINT"){return true;}
    else if( colorTarget == "R11G11B10_FLOAT"){return true;}
    else if( colorTarget == "R8G8B8A8_UNORM"){return true;}
    else if( colorTarget == "R8G8B8A8_UNORM_SRGB"){return true;}
    else if( colorTarget == "R8G8B8A8_UINT"){return true;}
    else if( colorTarget == "R8G8B8A8_SNORM"){return true;}
    else if( colorTarget == "R8G8B8A8_SINT"){return true;}
    else if( colorTarget == "R16G16_FLOAT"){return true;}
    else if( colorTarget == "R16G16_UNORM"){return true;}
    else if( colorTarget == "R16G16_UINT"){return true;}
    else if( colorTarget == "R16G16_SNORM"){return true;}
    else if( colorTarget == "R16G16_SINT"){return true;}
    else if( colorTarget == "R32_FLOAT"){return true;}
    else if( colorTarget == "R32_UINT"){return true;}
    else if( colorTarget == "R32_SINT"){return true;}
    else if( colorTarget == "R8G8_UNORM"){return true;}
    else if( colorTarget == "R8G8_UINT"){return true;}
    else if( colorTarget == "R8G8_SNORM"){return true;}
    else if( colorTarget == "R8G8_SINT"){return true;}
    else if( colorTarget == "R16_FLOAT"){return true;}
    else if( colorTarget == "R16_UNORM"){return true;}
    else if( colorTarget == "R16_UINT"){return true;}
    else if( colorTarget == "R16_SNORM"){return true;}
    else if( colorTarget == "R16_SINT"){return true;}
    else if( colorTarget == "R8_UNORM"){return true;}
    else if( colorTarget == "R8_UINT"){return true;}
    else if( colorTarget == "R8_SNORM"){return true;}
    else if( colorTarget == "R8_SINT"){return true;}
    else if( colorTarget == "A8_UNORM"){return true;}
    else if( colorTarget == "R9G9B9E5_SHAREDEXP"){return true;}
    else if( colorTarget == "R8G8_B8G8_UNORM"){return true;}
    else if( colorTarget == "G8R8_G8B8_UNORM"){return true;}
    else if( colorTarget == "BC1_UNORM"){return true;}
    else if( colorTarget == "BC1_UNORM_SRGB"){return true;}
    else if( colorTarget == "BC2_UNORM"){return true;}
    else if( colorTarget == "BC2_UNORM_SRGB"){return true;}
    else if( colorTarget == "BC3_UNORM"){return true;}
    else if( colorTarget == "BC3_UNORM_SRGB"){return true;}
    else if( colorTarget == "BC4_UNORM"){return true;}
    else if( colorTarget == "BC4_SNORM"){return true;}
    else if( colorTarget == "BC5_UNORM"){return true;}
    else if( colorTarget == "BC5_SNORM"){return true;}
    else if( colorTarget == "B5G6R5_UNORM"){return true;}
    else if( colorTarget == "B5G5R5A1_UNORM"){return true;}
    else if( colorTarget == "B8G8R8A8_UNORM"){return true;}
    else if( colorTarget == "B8G8R8X8_UNORM"){return true;}
    else if( colorTarget == "B8G8R8A8_UNORM_SRGB"){return true;}
    else if( colorTarget == "B8G8R8X8_UNORM_SRGB"){return true;}
    else if( colorTarget == "BC6H_UF16"){return true;}
    else if( colorTarget == "BC6H_SF16"){return true;}
    else if( colorTarget == "BC7_UNORM"){return true;}
    else if( colorTarget == "BC7_UNORM_SRGB"){return true;}
    else if( colorTarget == "AYUV"){return true;}
    else if( colorTarget == "NV12"){return true;}
    else if( colorTarget == "OPAQUE_420"){return true;}
    else if( colorTarget == "YUY2"){return true;}
    else if( colorTarget == "B4G4R4A4_UNORM"){return true;}
    return false;

}

bool isValidDepthTarget(const std::string & depthTarget)
{
    if( depthTarget == "D32_FLOAT_S8X24_UINT"){return true;}
    else if( depthTarget == "D32_FLOAT"){return true;}
    else if( depthTarget == "D24_UNORM_S8_UINT"){return true;}
    else if( depthTarget == "D16_UNORM"){return true;}
    else if( depthTarget == "none"){return true;}
    return false;
}

