/**
  *  MTCircularBuffer is a header-only C++ Library that implements a Multi-Threaded Circular Buffer
  * ---------------------------------------------------------------------------------------------------
  *
  *  MTCircularBuffer is built on top of boost::thread to provide a single-producer, multiple-consumer
  *  circular buffer.
  *
  *  The buffer is composed by N slots. Each slot can be accessed independently with two different
  *  permission levels:
  *
  *  BufferSlotWriteAccess: grants exclusive access to the slot
  *  BufferSlotReadAccess/BufferSlotConsumeAccess: grants shared read access to the slot
  *
  *  Additionally, the BufferSlotConsumeAccess automatically marks a slot as "consumed" on destruction
  *  so that the class can keep track on all the slots that contains data and were not consumed yet.
  *
  *
  *  Basic Usage:
  *
  *  1) Create a new buffer
  *   ```
  *    MTCircularBuffer< int > buff(10);
  *
  *   ```
  *
  *  2) In the producer thread, acquire a BufferSlotWriteAccess to write the data
  *   ```
  *      MTCircularBuffer<int>::BufferSlotWriteAccess wa;
  *      bool overwrite;
  *      try{
  *          buff.write_next( wa, &overwrite );  // overwrite is se to true if the next available slot
  *                                              // contains data that has was not yet consumed
  *          // Here the slot is locked and wa can be used to write the data into
  *          *(wa->data) = 10
  *
  *      } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
  *      {
  *         // Exception is thrown if a timeout occurred while locking the next available slot
  *      }
  *      // When wa is destroyed, slot exclusive access is automatically released
  *
  *   ```
  *
  *  3) In the consumer thread, acquire a BufferSlotConsumeAccess to read and consume the data
  *   ```
  *      MTCircularBuffer<int>::BufferSlotConsumeAccess ca;
  *      bool overwrite;
  *      try{
  *          buff.consume_next_available( ca );
  *          // Here the slot is locked and wa can be used to read the data
  *          int v = *(wa->data);
  *
  *      } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
  *      {
  *         // Exception is thrown if a timeout occurred while locking the next available slot
  *      }
  *      // When ca is destroyed, slot read access is automatically released and the data is consumed
  *
  *   ```
  *
  *
  * The MIT License (MIT)
  * Copyright (c) 2015 Filippo Bergamasco
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE.
  *
  */

#if !defined(MT_CIRCULAR_BUFFER_HPP)
#define MT_CIRCULAR_BUFFER_HPP

#if defined(_MSC_VER) && _MSC_VER >= 1200
    #pragma once
#endif


#include <boost/noncopyable.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <sstream>
#include <vector>
#include <queue>

#define DEFAULT_LOCK_TIMEOUT_SEC 1
#undef MT_CIRCULAR_BUFFER_DEBUG

template < typename T >
class MTCircularBuffer : private boost::noncopyable
{
public:

    struct ACCESS_OPT_WRITE;
    struct ACCESS_OPT_READ;
    struct ACCESS_OPT_CONSUME;

    template< typename LOCK_TYPE, typename OPT >
    class BufferSlotAccess : private boost::noncopyable
    {
    public:
        friend class MTCircularBuffer;
        BufferSlotAccess() :  _slot(-1), slot(_slot), srcBuffer(0), data(0) {}

        T* data;
        inline ~BufferSlotAccess()
        {
            data = 0;
            if( srcBuffer )
                srcBuffer->release_slot_access( *this );
        }

        const size_t& slot;

    private:
        LOCK_TYPE slot_lock;
        size_t _slot;
        MTCircularBuffer* srcBuffer;
    };

    /**
     * @brief BufferSlotWriteAccess provides exclusive write access to a buffer slot
     */
    typedef BufferSlotAccess< boost::unique_lock< boost::shared_mutex >, ACCESS_OPT_WRITE   > BufferSlotWriteAccess;
    /**
     * @brief BufferSlotReadAccess provides shared read access to a buffer slot.
     * The slot is not consumed after BufferSlotReadAccess destruction
     */
    typedef BufferSlotAccess< boost::shared_lock< boost::shared_mutex >, ACCESS_OPT_READ    > BufferSlotReadAccess;
    /**
     * @brief BufferSlotConsumeAccess provides shared read access to a buffer slot.
     * The slot is consumed after BufferSlotConsumeAccess destruction
     */
    typedef BufferSlotAccess< boost::shared_lock< boost::shared_mutex >, ACCESS_OPT_CONSUME > BufferSlotConsumeAccess;



    /**
     * @brief The SlotAcqTimeout exception is thrown if a timeout occurred while locking a slot
     */
    class SlotAcqTimeout : boost::exception {};
    /**
     * @brief The DataAvailableTimeout exception is thrown if a timeout occurred before data become available
     */
    class DataAvailableTimeout : boost::exception {};


    /**
     * @brief MTCircularBuffer constructs a new MTCircularBuffer of a given size
     * @param size Buffer size
     */
    inline explicit MTCircularBuffer( size_t size ) : buff( size ) , buff_desc( size ), curr_w_slot(0)
	{ 
		for( size_t i=0; i<buff_desc.size(); ++i )
		{
			buff_desc[i] = new BufferSlotDescriptor();
            buff_desc[i]->writing = false;
            buff_desc[i]->n_reading = 0;
        }
	}

    /**
     * @brief Discards all dirty slots and resets all the buffer slots (NOTE: this method
     *        is intented to be called when no other thread is accessing the buffer)
     *
     */
    inline void clear()
    {
        boost::posix_time::time_duration lock_timeout(0, 0, DEFAULT_LOCK_TIMEOUT_SEC);

        // Advance to next slot (we need to lock the entire circular buffer to change curr_w_slot)
        boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx, lock_timeout  );
        if( !sc_lock.owns_lock() ) //owns_lock is false if lock failed (probably timeout has occurred)
        {
            throw SlotAcqTimeout();
        }

        while( !dirty_slots.empty() )
            dirty_slots.pop();

        curr_w_slot = 0;

    }

    /**
     * @return number of buffer slots
     */
	inline size_t size() const { return buff.size(); }


    /**
     * @brief write_next Gain exclusive write access to the next available slot
     * @param acc A BufferSlotWriteAccess that will represent slot ownership
     * @param overwrite_occurred is set to true if write access is given to a non consumed slot
     */
    inline void write_next( BufferSlotWriteAccess& acc, bool* overwrite_occurred=0 )
	{
        boost::posix_time::time_duration lock_timeout(0, 0, DEFAULT_LOCK_TIMEOUT_SEC);

        boost::unique_lock< boost::shared_mutex > um(buff_desc[curr_w_slot]->slot_mtx , lock_timeout );
        if( !um.owns_lock() ) //owns_lock is false if lock failed (probably timeout has occurred)
        {
            throw SlotAcqTimeout();
        }

        if( overwrite_occurred != 0 )
            *overwrite_occurred = buff_desc[curr_w_slot]->is_dirty;

        acc._slot = curr_w_slot;
        acc.data = &(buff[curr_w_slot]);
        acc.srcBuffer = this;
        buff_desc[curr_w_slot]->writing = true;
        acc.slot_lock.swap( um );

        // Advance to next slot (we need to lock the entire circular buffer to change curr_w_slot)
        boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx, lock_timeout  );
        if( !sc_lock.owns_lock() ) //owns_lock is false if lock failed (probably timeout has occurred)
        {
            throw SlotAcqTimeout();
        }
        curr_w_slot = (++curr_w_slot)%buff.size();
    }


    /**
     * @brief read_slot Gain shared read access to a given slot
     * @param slot Slot number
     * @param acc A BufferSlotReadAccess that will represent slot ownership
     */
    inline void read_slot( const size_t slot, BufferSlotReadAccess& acc )
    {
        boost::posix_time::time_duration lock_timeout(0, 0, DEFAULT_LOCK_TIMEOUT_SEC);
        boost::shared_lock< boost::shared_mutex > um(buff_desc[slot]->slot_mtx , boost::get_system_time()+lock_timeout );
        if( !um.owns_lock() ) //owns_lock is false if lock failed
        {
            throw SlotAcqTimeout();
        }

        acc._slot = slot ;
        acc.data = &(buff[slot]);
        acc.srcBuffer = this;
        buff_desc[slot]->n_reading++;
        acc.slot_lock.swap( um );
    }

    /**
     * @brief read_newest_available Gain shared read access to the most recently produced slot
     * @param acc A BufferSlotReadAccess that will represent slot ownership
     */
    inline void read_newest_available( BufferSlotReadAccess& acc )
    {
        boost::posix_time::time_duration lock_timeout(0, 0, DEFAULT_LOCK_TIMEOUT_SEC);
        boost::unique_lock< boost::mutex > data_available_lock( data_available_mutex );

        // wait until some data is available
        while( dirty_slots.empty() )
        {
            if( !data_available.timed_wait( data_available_lock, boost::get_system_time() ) )
            {
                throw DataAvailableTimeout();
            }
        }

        const size_t slot = dirty_slots.back();
        boost::shared_lock< boost::shared_mutex > um(buff_desc[slot]->slot_mtx , boost::get_system_time()+lock_timeout );
        if( !um.owns_lock() ) //owns_lock is false if lock failed
        {
            throw SlotAcqTimeout();
        }

        acc._slot = slot;
        acc.data = &(buff[slot]);
        acc.srcBuffer = this;
        buff_desc[slot]->n_reading++;
        acc.slot_lock.swap( um );
    }

    /**
     * @brief consume_next_available Gain shared read access to the least recently produced slot
     * @param acc A BufferSlotConsumeAccess that will represent slot ownership
     */
    inline void consume_next_available( BufferSlotConsumeAccess& acc )
    {
        boost::posix_time::time_duration lock_timeout(0, 0, DEFAULT_LOCK_TIMEOUT_SEC);
        boost::unique_lock< boost::mutex > data_available_lock( data_available_mutex );

        // wait until some data is available
        while( dirty_slots.empty() )
        {
            if( !data_available.timed_wait( data_available_lock, boost::get_system_time() + lock_timeout)  )
            {
                throw DataAvailableTimeout();
            }
        }

        const size_t slot = dirty_slots.front();
        boost::shared_lock< boost::shared_mutex > um(buff_desc[slot]->slot_mtx , boost::get_system_time()+lock_timeout );
        if( !um.owns_lock() ) //owns_lock is false if lock failed
        {
            data_available.notify_all(); // We failed to lock this slot, maybe someone else will succeed
            throw SlotAcqTimeout();
        }

        // Now we got the access to this slot, so we can safely remove it from the consume queue
        dirty_slots.pop();

        acc._slot = slot;
        acc.data = &(buff[slot]);
        acc.srcBuffer = this;
        buff_desc[slot]->n_reading++;
        acc.slot_lock.swap( um );
    }

    /**
     * @brief is_written returns true if the specified slot is currently being written
     */
    inline bool is_written( size_t slot ) const
    {
        if( slot < buff_desc.size() )
        {
            return buff_desc[slot]->writing;
        }
        return false;
    }


    /**
     * @brief num_concurrent_read returns the number of shared read accessed currently granted for the specified slot
     */
    inline size_t num_concurrent_read( size_t slot ) const
    {
        if( slot < buff_desc.size() )
        {
            return buff_desc[slot]->n_reading;
        }
        return 0;
    }

    /**
     * @brief is_read returns true if the specified slot is currently being read
     */
    inline bool is_read( size_t slot ) const { return num_concurrent_read(slot)>0; }


    inline size_t num_consumable_slots() const { return dirty_slots.size(); }


    inline std::string to_string()
    {
        std::stringstream ss;
        ss << "[ ";

        boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx   );
        for( size_t i=0; i<buff_desc.size(); ++i )
        {
            if( buff_desc[i]->writing )
                ss << " W ";
            else if( buff_desc[i]->n_reading>0 )
            {
                ss << buff_desc[i]->n_reading << "R ";
            }
            else if( buff_desc[i]->is_dirty )
            {
                ss << " X ";
            }
            else {
                ss << " . ";
            }
        }
        ss << " ]";

        return ss.str();
    }

private:
		
	struct BufferSlotDescriptor : boost::noncopyable
	{
		boost::shared_mutex slot_mtx;
        bool writing;
        size_t n_reading;
        bool is_dirty;
    };

    inline void release_slot_access( const BufferSlotWriteAccess& acc )
    {
        //boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx );
        buff_desc[ acc.slot ]->writing = false;
        buff_desc[ acc.slot ]->is_dirty = true;
        dirty_slots.push( acc.slot );
        data_available.notify_one();
#ifdef MT_CIRCULAR_BUFFER_DEBUG
        std::cout << "Write access released on slot " << acc.slot << ", dirty slot produced" << std::endl;
#endif
    }
    inline void release_slot_access( const BufferSlotReadAccess& acc )
    {
        //boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx );
        buff_desc[ acc.slot ]->n_reading--;
#ifdef MT_CIRCULAR_BUFFER_DEBUG
        std::cout << "Read access released on slot " << acc.slot <<   std::endl;
#endif
    }
    inline void release_slot_access( const BufferSlotConsumeAccess& acc )
    {
        //boost::unique_lock< boost::timed_mutex > sc_lock( main_mtx );
        buff_desc[ acc.slot ]->is_dirty = false;
        buff_desc[ acc.slot ]->n_reading--;
#ifdef MT_CIRCULAR_BUFFER_DEBUG
        std::cout << "Consume access released on slot " << acc.slot << ", dirty slot consumed" << std::endl;
#endif
    }

    boost::timed_mutex main_mtx;

    boost::condition_variable data_available;
    boost::mutex data_available_mutex;

	std::vector< T > buff;
	std::vector< BufferSlotDescriptor* > buff_desc;
    std::queue< size_t > dirty_slots;
    size_t curr_w_slot;
};


#endif
