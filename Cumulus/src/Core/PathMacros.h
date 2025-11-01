/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : This header file defines preprocessor 
helper macros for reaching certain paths 
----------------------------------------------*/
#ifndef PATHMACROS_H
#define PATHMACROS_H

#include <string>

// Helper macro for wide strings
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)

#define __WFILE__ WIDEN(__FILE__)
#define __WLINE__ WIDEN(__LINE__)

// Helper macros for getting correct paths. WILL ONLY WORK IN THIS PROJECT CONFIG
#define ASSETPATH "..\\Assets\\"
#define MODELPATH ASSETPATH ## "Models\\"
#define MODELPATHW WIDEN(MODELPATH)
#define TEXTUREPATH ASSETPATH ## "Textures\\"
#define SHADERPATH "..\\_bin\\Shaders\\"
#define SHADERPATHW WIDEN(SHADERPATH)

inline std::wstring GetShaderPathFromFile_W(std::wstring fileName)
{
    std::wstring path = SHADERPATHW;
    return path + fileName;
}

inline std::wstring GetModelPathFromFile_W(std::wstring fileName)
{
    std::wstring path = MODELPATHW;
    return path + fileName;
}

inline std::string GetModelPathFromFile(std::string fileName)
{
    std::string path = MODELPATH;
    return path + fileName;
}
#endif