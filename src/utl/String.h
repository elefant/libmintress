#ifndef UTL_STRING_H
#define UTL_STRING_H

#include <string>
#include <vector>
#include <stdarg.h>

namespace utl
{
    std::string strFormat
        (
        const char* aFormat,
        const va_list& aVaList
        );
    
    std::string strFormat
        (
        const char* aFormat,
        ...
        );
    
    int strToInt
        (
        const std::string& aValue, 
        const int aDefault = 0,
        const int aMax = 0, 
        const int aMin = 0
        );

    std::string intTostr
        (
        const int aValue, 
        const std::string& aDefault = std::string()
        );
    
    bool strToArgv
        (
        const std::string& aArgString,
        std::vector<std::string>& aArgv
        );
}

#endif