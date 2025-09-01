#ifndef X_OFFSET_DATA_STRUCTURE_2_X_BUFFER_HPP
#define X_OFFSET_DATA_STRUCTURE_2_X_BUFFER_HPP

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/detail/workaround.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/interprocess/detail/managed_memory_impl.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <vector>
#include <iostream>

namespace boost
{
   namespace interprocess
   {
      template <
          class CharType,
          class AllocationAlgorithm,
          template <class IndexConfig> class IndexType>
      class XManagedMemory
          : public ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType>
      {
      private:
         typedef ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType> base_t;
         BOOST_MOVABLE_BUT_NOT_COPYABLE(XManagedMemory)

      public:
         typedef typename base_t::size_type size_type;

         XManagedMemory() BOOST_NOEXCEPT
         {
         }

         ~XManagedMemory()
         {
            this->priv_close();
         }

         XManagedMemory(size_type size)
             : m_buffer(size, char(0))
         {
            void *addr = m_buffer.data();
            if (!base_t::create_impl(addr, size))
            {
               this->priv_close();
               throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
            }
         }

         XManagedMemory(std::vector<char> &externalBuffer) : m_buffer(std::move(externalBuffer))
         {
            void *addr = m_buffer.data();
            size_type size = m_buffer.size();
            BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
            if (!base_t::open_impl(addr, size))
            {
               throw interprocess_exception("Could not initialize m_buffer in x_managed_external_buffer constructor");
            }
         }

         XManagedMemory(BOOST_RV_REF(XManagedMemory) moved) BOOST_NOEXCEPT
         {
            this->swap(moved);
         }

         XManagedMemory &operator=(BOOST_RV_REF(XManagedMemory) moved) BOOST_NOEXCEPT
         {
            XManagedMemory tmp(boost::move(moved));
            this->swap(tmp);
            return *this;
         }

         bool grow(size_type extra_bytes)
         {
            BOOST_TRY
            {
               m_buffer.resize(m_buffer.size() + extra_bytes);
            }
            BOOST_CATCH(...)
            {
               return false;
            }
            BOOST_CATCH_END

            base_t::close_impl();
            base_t::open_impl(&m_buffer[0], m_buffer.size());
            base_t::grow(extra_bytes);
            return true;
         }

         void swap(XManagedMemory &other) BOOST_NOEXCEPT
         {
            base_t::swap(other);
            m_buffer.swap(other.m_buffer);
         }

         void update_after_shrink()
         {
            auto *pBuf = get_buffer();
            std::vector<char> new_buf(pBuf->data(), pBuf->data() + pBuf->size());
            XManagedMemory new_mem(new_buf);
            this->swap(new_mem);
         }

         void shrink_to_fit()
         {
            base_t::shrink_to_fit();
            m_buffer.resize(base_t::get_size());
            update_after_shrink();  // 直接调用成员方法
         }

         std::vector<char> *get_buffer()
         {
            return &m_buffer;
         }

      private:
         void priv_close()
         {
            base_t::destroy_impl();
            std::vector<char>().swap(m_buffer);
         }

         std::vector<char> m_buffer;
      };

   }
}

#endif
