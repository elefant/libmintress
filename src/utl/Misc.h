#ifndef UTL_MISC_H
#define UTL_MISC_H

#include <boost/function.hpp>

namespace utl
{
    bool waitUntil
        (
        boost::function<bool()> aExitCondition,
        int aTimeout = -1,
        int aPollingPeriodMs = 10
        );
}

#endif


