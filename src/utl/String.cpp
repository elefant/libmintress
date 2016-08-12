#include "String.h"
#include <stdio.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace utl
{
    /**
     * 
     * @param aFormat
     * @param aVaList
     * @return 
     */
    std::string strFormat
        (
        const char* aFormat,
        const va_list& aVaList
        )
    {
        std::string msg;
        std::vector<char> vbuf( 1024 );
        while( true )
        {
            int needed = vsnprintf( &vbuf[0], vbuf.size(), aFormat, aVaList );
            if( needed <= (int)vbuf.size() && needed >= 0 ) 
            {
                msg = std::string( &vbuf[0], (size_t)needed );
                break;
            }
            vbuf.resize( vbuf.size() * 2 ); // weird
        }
        
        return msg;
    }
    
    /**
     * 
     * @param aFormat
     * @param ...
     * @return 
     */
    std::string strFormat
        (
        const char* aFormat,
        ...
        )
    {
        va_list ap;
        va_start( ap, aFormat );
        va_end( ap );
        return strFormat( aFormat, ap );
    }

    /**
     * string to int
     * @param aValue
     * @param aDefault
     * @param aMax
     * @param aMin
     * @return 
     */
    int strToInt
        (
        const std::string& aValue, 
        const int aDefault,
        const int aMax, 
        const int aMin
        )
    {
        int ret = aDefault;
        
        if( !aValue.empty() )
        {
            try 
            {
                ret = boost::lexical_cast<int>( aValue );
                if( aMax != 0 && ret > aMax ) 
                { 
                    ret  = aMax;
                }
                if( aMin != 0 && ret < aMin )
                {
                    ret  = aMin;
                }
            }
            catch (...) 
            {
                // error 
            }
        }
        
        return ret;
    }

    /**
     * int to string
     * @param aValue
     * @param aDefault
     * @return 
     */
    std::string intTostr
        (
        const int aValue, 
        const std::string& aDefault
        )
    {
        std::string ret = aDefault;
        
        try 
        {
            ret = boost::lexical_cast<std::string>( aValue );
        }
        catch (...) 
        {
            // error 
        }
        
        return ret;   
    }
    
    /**
     * Parse string to argv
     * @param args
     * @return 
     */
    bool strToArgv
        (
        const std::string& aArgString,
        std::vector<std::string>& aArgv
        )
    {
        std::stringstream ain( aArgString );    // used to iterate over input string
        ain >> std::noskipws;           // do not skip white spaces
        aArgv.clear(); // output list of arguments

        std::stringstream currentArg( "" );
        currentArg >> std::noskipws;

        // current state
        enum State 
        {
            InArg,      // currently scanning an argument
            InArgQuote, // currently scanning an argument (which started with quotes)
            OutOfArg    // currently not scanning an argument
        };
        State currentState = OutOfArg;

        char currentQuoteChar = '\0'; // to distinguish between ' and " quotations
                                        // this allows to use "foo'bar"

        char c;
        while( !ain.eof() && ( ain >> c ) )  // iterate char by char
        {
            if( '\'' == c || '\"' == c ) 
            {
                switch( currentState ) 
                {
                case OutOfArg:
                    currentArg.str(std::string());
                case InArg:
                    currentState = InArgQuote;
                    currentQuoteChar = c;
                    break;

                case InArgQuote:
                    if(c == currentQuoteChar)
                        currentState = InArg;
                    else
                        currentArg << c;
                break;
                }
            }
            else if( ' ' == c || '\t' == c ) 
            {
                switch( currentState ) 
                {
                    case InArg:
                        aArgv.push_back( currentArg.str() );
                        currentState = OutOfArg;
                        break;
                    case InArgQuote:
                        currentArg << c;
                        break;
                    case OutOfArg:
                        // nothing
                        break;
                }
            }
            else if( '\\' == c ) 
            {
                switch(currentState) 
                {
                case OutOfArg:
                    currentArg.str(std::string());
                    currentState = InArg;

                case InArg:
                case InArgQuote:
                    if( ain.eof() )
                    {
                        //throw(std::runtime_error("Found Escape Character at end of file."));
                    }
                    else
                    {
                        ain >> c;
                        currentArg << c;
                    }
                    break;
                }
            }
            else 
            {
                switch( currentState )
                {
                case InArg:
                case InArgQuote:
                    currentArg << c;
                    break;

                case OutOfArg:
                    currentArg.str(std::string());
                    currentArg << c;
                    currentState = InArg;
                    break;
                }
            }
        }

        if( currentState == InArg )
        {
            aArgv.push_back(currentArg.str());
        }
        else if( currentState == InArgQuote )
        {
            return false;
        }

        return true;
    }
}