#include <filesystem>
#include <iostream>
#include <ostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vector>
#include <boost/program_options.hpp>
using namespace slang;
namespace po = boost::program_options;
int main(int argc, char**argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message")
    ("root,r", po::value<std::vector<std::string>>(),"Top level folder containing shader files")
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
    std::vector<IModule*> entryPointModules;
    using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
    for (const auto& dirEntry : recursive_directory_iterator(root))
    {
        if (dirEntry.is_regular_file())
        {
            auto path = dirEntry.path();
            if (path.extension() == ".slang")
            {
                Slang::ComPtr<IBlob> diagnostics;
                IModule* module = session->loadModule(path.string().c_str(),diagnostics.writeRef());
                entryPointModules.push_back(module);

                if (diagnostics)
                {
                    std::cerr << diagnostics->getBufferPointer() << std::endl;
                    return 1;
                }
            }
        }

    }

    return 0;
}