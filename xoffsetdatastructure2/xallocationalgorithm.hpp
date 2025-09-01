#ifndef X_OFFSET_DATA_STRUCTURE_2_X_FIT_HPP
#define X_OFFSET_DATA_STRUCTURE_2_X_FIT_HPP
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>

namespace boost
{
    namespace interprocess
    {
        // template<class MutexFamily, class VoidMutex = offset_ptr<void> >
        // class simple_seq_fit;
        // template<class MutexFamily, class VoidMutex = offset_ptr<void>, std::size_t MemAlignment = 0>
        // class rbtree_best_fit;

        template <class MutexFamily, class VoidPointer = offset_ptr<void>, std::size_t MemAlignment = 0>
        class x_best_fit : public rbtree_best_fit<mutex_family, VoidPointer, MemAlignment>
        {
        public:
            using supertype = rbtree_best_fit<mutex_family, VoidPointer, MemAlignment>;
            using size_type = supertype::size_type;

            x_best_fit(size_type size, size_type extra_hdr_bytes) : supertype(size, extra_hdr_bytes)
            {
            }
            // void grow(size_type extra_size)
            // {
            //     // Only modify the size in header.
            //     supertype::grow(extra_size);
            // }
        };

        template <class MutexFamily, class VoidPointer = offset_ptr<void>>
        class x_seq_fit : public simple_seq_fit<MutexFamily, VoidPointer>
        {
        public:
            using supertype = simple_seq_fit<MutexFamily, VoidPointer>;
            using size_type = supertype::size_type;
            x_seq_fit(size_type segment_size, size_type extra_hdr_bytes) : supertype(segment_size, extra_hdr_bytes)
            {
            }
        };
    }
}
#endif