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

#define ASSETPATH "..\\Assets\\"
#define MODELPATH ASSETPATH ## "Models\\"
#define MODELPATHW WIDEN(MODELPATH)
#define TEXTUREPATH ASSETPATH ## "Textures\\"
#define TGAPATH ASSETPATH ## "TGA\\"
#define TGAPATHW WIDEN(TGAPATH)
#define NVDFPATH TGAPATH ## "NVDF\\"
#define NVDFPATHW WIDEN(NVDFPATH)
#define TEX3DPATH TEXTUREPATH ## "3D\\"
#define TEX3DPATHW WIDEN(TEX3DPATH)
#define NOISEPATH TGAPATH ## "Noise\\"
#define NOISEPATHW WIDEN(NOISEPATH)
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