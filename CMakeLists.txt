CMAKE_MINIMUM_REQUIRED( VERSION 2.8.0 )

PROJECT( MTCircularBuffer )

file(DOWNLOAD "https://raw.githubusercontent.com/philsquared/Catch/master/single_include/catch.hpp" ${CMAKE_CURRENT_SOURCE_DIR}/catch.hpp)

IF( NOT Boost_INCLUDE_DIRS )

	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_MULTITHREADED ON)  
	set(Boost_USE_STATIC_RUNTIME OFF)
	find_package(Boost 1.45.0 REQUIRED COMPONENTS thread system date_time )
	IF( NOT Boost_FOUND )
		MESSAGE( ERROR, " Boost was not found. Please set BOOST_ROOT path variable")
	ENDIF()

ENDIF()

MESSAGE(STATUS "  Boost include: ${Boost_INCLUDE_DIRS}")
MESSAGE(STATUS "Boost libraries: ${Boost_LIBRARIES}")

include_directories(${Boost_INCLUDE_DIRS})
ADD_EXECUTABLE( MTCircularBufferTEST MTCircularBufferTEST.cpp MTCircularBuffer.hpp catch.hpp )
TARGET_LINK_LIBRARIES(  MTCircularBufferTEST  ${Boost_LIBRARIES}  )
