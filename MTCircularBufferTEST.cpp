/**
 *  MTCircularBuffer TEST Cases
 *
 *
 */
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "MTCircularBuffer.hpp"


SCENARIO("Basic single-threaded operations", "[Single]") 
{

    GIVEN( "Buffer with 5 slots" ) {
        MTCircularBuffer< int > buff(5);

        REQUIRE( buff.size() == 5 );
        REQUIRE( buff.is_read(0)==false );
        REQUIRE( buff.is_written(0)==false );

        REQUIRE( buff.is_written(6)==false );
        REQUIRE( buff.is_read(6)==false );


        WHEN("Write access requested on the next available slot")
        {
            MTCircularBuffer< int >::BufferSlotWriteAccess wa;
            buff.write_next( wa );

            THEN("Write access is granted")
            {
                REQUIRE(wa.data != 0);
            }
            THEN("Slot 0 is written")
            {
                REQUIRE( buff.is_written(0) );
            }
        }

        WHEN("Write access requested on all slots")
        {
            MTCircularBuffer<int>::BufferSlotWriteAccess wa[5];
            THEN("Write access is granted to all slots")
            {
                for( int i=0; i<5; ++i )
                {
                    REQUIRE( !buff.is_written(i) );
                    buff.write_next( wa[i] );
                    REQUIRE( wa[i].data != 0);
                    REQUIRE( buff.is_written(i) );
                }
            }
        }

        WHEN("Buffer is cleared")
        {
            {
                MTCircularBuffer< int >::BufferSlotWriteAccess wa;
                buff.write_next( wa );
            }
            REQUIRE( buff.num_consumable_slots() > 0 );
            buff.clear();
            THEN("Clear succeeded")
            {
                REQUIRE( buff.num_consumable_slots() == 0 );
            }
        }
    }

    GIVEN( "Buffer with 1 slots, write access granted" ) {
        MTCircularBuffer< int > buff(1);
        MTCircularBuffer< int >::BufferSlotWriteAccess* wa = new MTCircularBuffer< int >::BufferSlotWriteAccess();
        buff.write_next( *wa );

        REQUIRE( buff.is_written(0) );
        REQUIRE(wa->data != 0);

        WHEN("Write access grant is destroyed")
        {
            delete wa;
            THEN("Write access is revoked")
            {
                REQUIRE( !buff.is_written(0) );
            }
        }
    }

    GIVEN( "Buffer with 1 slots, write access granted" ) {
        MTCircularBuffer< int > buff(1);
        MTCircularBuffer< int >::BufferSlotWriteAccess* wa = new MTCircularBuffer< int >::BufferSlotWriteAccess();
        buff.write_next( *wa );

        REQUIRE( buff.is_written(0) );
        REQUIRE(wa->data != 0);

        MTCircularBuffer< int >::BufferSlotWriteAccess* wa2 = new MTCircularBuffer< int >::BufferSlotWriteAccess();

        WHEN("Request another write access")
        {
            THEN("Write access is not granted")
            {
                try
                {
                    buff.write_next( *wa2 );
                    REQUIRE( false );

                } catch( MTCircularBuffer< int >::SlotAcqTimeout& ex )
                {
                    REQUIRE( true );
                }

            }
        }
        WHEN("Previous write access grant is destroyed")
        {
            delete wa; wa = 0;
            REQUIRE( !buff.is_written(0) );

            THEN("Another write access is granted")
            {
                buff.write_next(*wa2);
                REQUIRE( wa2->data != 0 );
                REQUIRE( buff.is_written(0) );
            }
        }
    }

    GIVEN( "Buffer with 1 slots, write access granted" ) {
        MTCircularBuffer< int > buff(1);
        MTCircularBuffer< int >::BufferSlotWriteAccess* wa = new MTCircularBuffer< int >::BufferSlotWriteAccess();
        buff.write_next( *wa );

        REQUIRE( buff.is_written(0) );
        REQUIRE(wa->data != 0);

        WHEN("Read access is requested on slot 0")
        {
            THEN("Read access is not granted")
            {
                MTCircularBuffer< int >::BufferSlotReadAccess* ra = new MTCircularBuffer< int >::BufferSlotReadAccess();
                try
                {
                    buff.read_slot(0, *ra);
                    REQUIRE( false );

                } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
                {
                    REQUIRE( true );
                }
            }
        }
        WHEN("Write access is destroyed")
        {
            delete wa;
            MTCircularBuffer< int >::BufferSlotReadAccess* ra = new MTCircularBuffer< int >::BufferSlotReadAccess();
            MTCircularBuffer< int >::BufferSlotReadAccess* ra2 = new MTCircularBuffer< int >::BufferSlotReadAccess();

            THEN("Read access 1 granted")
            {
                try
                {
                    buff.read_slot(0, *ra);
                    REQUIRE( true );

                } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
                {
                    REQUIRE( false );
                }
            }
            THEN("Read access 2 granted")
            {
                try
                {
                    buff.read_slot(0, *ra2);
                    REQUIRE( true );

                } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
                {
                    REQUIRE( false );
                }
            }

            delete ra;
            delete ra2;
        }
    }

    GIVEN( "Buffer with 5 slots" ) {
        MTCircularBuffer< int > buff(5);

        WHEN("Consume access is requested")
        {
            MTCircularBuffer< int >::BufferSlotConsumeAccess* ca = new MTCircularBuffer< int >::BufferSlotConsumeAccess();
            THEN("Timeout occurs since no data is available")
            {
                try
                {
                    buff.consume_next_available( *ca );
                    REQUIRE(false);
                } catch( MTCircularBuffer<int>::DataAvailableTimeout& da )
                {
                    REQUIRE(true);
                }
            }
            delete ca;
        }

        WHEN("Data is produced")
        {
            REQUIRE( buff.num_consumable_slots()==0 );
            {
            MTCircularBuffer<int>::BufferSlotWriteAccess wa;
            buff.write_next( wa );
            }
            REQUIRE( buff.num_consumable_slots()==1 );
            THEN("Consume access is granted")
            {
                try
                {
                    MTCircularBuffer< int >::BufferSlotConsumeAccess ca;
                    buff.consume_next_available( ca );
                    REQUIRE(true);
                } catch( ... )
                {
                    REQUIRE(false);
                }
                REQUIRE( buff.num_consumable_slots()==0 );
            }
        }
    }
}


class SimpleProducerThread
{
public:
    SimpleProducerThread( MTCircularBuffer<int>& _buff ) : buff(_buff), running(true) { }
    void operator()()
    {

        while( running )
        {
            {
                MTCircularBuffer<int>::BufferSlotWriteAccess wa;
                bool overwrite = false;
                try{
                    buff.write_next( wa, &overwrite );
                } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
                {
                    std::cout << "Write lock timeout" << std::endl;
                }

                if( overwrite )
                    std::cout << "Overwrite occurred" << std::endl;
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            }
        }
    }
    inline void kill()
    {
        running = false;
    }

    bool running;
    MTCircularBuffer<int>& buff;
};

class SimpleConsumerThread
{
public:
    SimpleConsumerThread( MTCircularBuffer<int>& _buff ) : buff(_buff), running(true) { }
    void operator()()
    {

        while( running )
        {
            {
                MTCircularBuffer<int>::BufferSlotConsumeAccess ca;
                try
                {
                    buff.consume_next_available( ca );
                } catch( MTCircularBuffer<int>::DataAvailableTimeout& ex )
                {
                    std::cout << "DataAvailableTimeout" << std::endl;
                }

                boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
            }
        }
    }
    inline void kill()
    {
        running = false;
    }

    bool running;
    MTCircularBuffer<int>& buff;
};

class SimpleReaderThread
{
public:
    SimpleReaderThread( MTCircularBuffer<int>& _buff ) : buff(_buff), running(true) { }
    void operator()()
    {

        while( running )
        {
            {
                MTCircularBuffer<int>::BufferSlotReadAccess ra;
                try
                {
                    buff.read_newest_available( ra );
                } catch( ... )
                {
                }

                boost::this_thread::sleep(boost::posix_time::milliseconds(600));
            }
        }
    }
    inline void kill()
    {
        running = false;
    }

    bool running;
    MTCircularBuffer<int>& buff;
};


SCENARIO( "MultiThreaded single producer test", "[MT_Single_Producer]")
{
    GIVEN( "Buffer with 10 slots" ) {
        std::cout << "MultiThreaded single producer test" << std::endl;
        const int test_duration_sec=10;
        MTCircularBuffer< int > buff(10);

        SimpleProducerThread pr_thread( buff );
        boost::thread pr_thread_t( boost::ref( pr_thread ) );

        std::cout << std::endl;
        for( int i=0; i<test_duration_sec*10; ++i )
        {
            std::cout << "\r\r" << buff.to_string() << "    ";
            std::flush( std::cout );
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        std::cout << std::endl << "Killing producer thread..." << std::endl;
        pr_thread.kill();
        pr_thread_t.join();
        std::cout << std::endl << "Test done." << std::endl;
        REQUIRE(true);
    }

}

SCENARIO( "MultiThreaded producer/consumer test", "[MT_Producer_consumer]")
{
    GIVEN( "Buffer with 10 slots" ) {
        std::cout << "MultiThreaded producer/consumer test" << std::endl;

        const int test_duration_sec=13;
        MTCircularBuffer< int > buff(10);

        SimpleProducerThread pr_thread( buff );
        boost::thread pr_thread_t( boost::ref( pr_thread ) );
        SimpleConsumerThread cn_thread( buff );
        boost::thread cn_thread_t( boost::ref( cn_thread ) );

        std::cout << std::endl;
        for( int i=0; i<test_duration_sec*10; ++i )
        {
            std::cout << "\r\r" << buff.to_string() << "    ";
            std::flush( std::cout );
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        std::cout << std::endl << "Killing producer thread..." << std::endl;
        pr_thread.kill();
        pr_thread_t.join();
        std::cout << std::endl << "Killing consumer thread..." << std::endl;
        cn_thread.kill();
        cn_thread_t.join();
        std::cout << std::endl << "Test done." << std::endl;
        REQUIRE(true);
    }
}

SCENARIO( "MultiThreaded producer/consumer/reader test", "[MT_Producer_consumer_reader]")
{
    GIVEN( "Buffer with 10 slots" ) {
        std::cout << "MultiThreaded producer/consumer test" << std::endl;

        const int test_duration_sec=15;
        MTCircularBuffer< int > buff(10);

        SimpleProducerThread pr_thread( buff );
        boost::thread pr_thread_t( boost::ref( pr_thread ) );
        SimpleConsumerThread cn_thread( buff );
        boost::thread cn_thread_t( boost::ref( cn_thread ) );
        SimpleReaderThread rd_thread( buff );
        boost::thread rd_thread_t( boost::ref( rd_thread ) );

        std::cout << std::endl;
        for( int i=0; i<test_duration_sec*10; ++i )
        {
            std::cout << "\r\r" << buff.to_string() << "    ";
            std::flush( std::cout );
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        std::cout << std::endl << "Killing reader thread..." << std::endl;
        rd_thread.kill();
        rd_thread_t.join();
        std::cout << std::endl << "Killing producer thread..." << std::endl;
        pr_thread.kill();
        pr_thread_t.join();
        std::cout << std::endl << "Killing consumer thread..." << std::endl;
        cn_thread.kill();
        cn_thread_t.join();
        std::cout << std::endl << "Test done." << std::endl;
        REQUIRE(true);
    }

}
