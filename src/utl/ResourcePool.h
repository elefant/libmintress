#ifndef UTL_RESOURCE_POOL
#define UTL_RESOURCE_POOL

#include <set>

namespace utl
{
    template<typename R>
    class ResourcePool
    {
    private:
        typedef std::set<R> Pool; 
            
    public:
        void add
            (
            const R& aResource
            )
        {
            iAvailablePool.insert( aResource );
        }

        bool acquire
            (
            R& aResource
            )
        {
            if( iAvailablePool.empty() )
            {
                return false;
            }

            typename Pool::iterator ri = iAvailablePool.begin();
            aResource = *ri;
            iAvailablePool.erase( ri );
            iOccupiedPool.insert( aResource );

            return true;
        }

        void release
            (
            const R& aResource
            )
        {
            iAvailablePool.insert( aResource );
            iOccupiedPool.erase( aResource );
        }

        void clear()
        {
            iAvailablePool.clear();
            iOccupiedPool.clear();
        }

    private:
        Pool iAvailablePool;
        Pool iOccupiedPool;
    };
}

#endif

