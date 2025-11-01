/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : Interface for custom COM Exception reporting
----------------------------------------------*/
#ifndef COMEXCEPTION_H
#define COMEXCEPTION_H

#if defined(MN_DEBUG)
#include <windows.h>
#include <winbase.h>
#include <string>
#include <sstream>
#include <exception>

// Putting this in the graphics namespace, since it's primarily meant to interface with DeviceResources
namespace Muon
{

class COMException : public std::exception
{
public:
    COMException(int line, const char* pFile, HRESULT hresult) noexcept :
        mHRESULT(hresult),
        mLine(line),
        mFile(pFile),
        errorMsgBuffer(NULL)
    {}

    COMException() = delete;

    ~COMException()
    {
        LocalFree(errorMsgBuffer);
    }

    const char* what() const noexcept override
    {
        std::ostringstream oss;
        oss << GetType() << std::endl
            << "[Error Code:] 0x" << std::hex << std::uppercase << mHRESULT
            << std::dec << " (" << (unsigned long)mHRESULT << " )" << std::endl
            << "[Description: ]" << GetErrorMessage() << std::endl
            << GetOriginString();

        whatBuffer = oss.str();
        return whatBuffer.c_str();
    }
    
    const char* GetType() const noexcept
    {
        return "COM Exception";
    }

    HRESULT GetErrorCode() const noexcept
    {
        return mHRESULT;
    }

    const CHAR* GetErrorMessage() const noexcept
    {
      FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
          NULL,
          mHRESULT,
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPSTR)&errorMsgBuffer,
          0,
          NULL);

        return errorMsgBuffer;
    }

    int GetLine() const noexcept
    {
        return mLine;
    }

    const std::string& GetFile() const noexcept
    {
        return mFile;
    }

    std::string GetOriginString() const noexcept
    {
        std::ostringstream oss;
        oss << "[File:] " << mFile << std::endl
            << "[Line:] " << mLine;
        return oss.str();
    }

private:
    HRESULT     mHRESULT;
    int         mLine;
    std::string mFile;

    // must be mutable to be modifiable through const function
    mutable std::string whatBuffer;
    mutable CHAR* errorMsgBuffer;
};

}
#endif
#endif