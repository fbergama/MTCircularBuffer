# MTCircularBuffer

MTCircularBuffer is a header-only C++ Library built on top of boost::thread to provide a
single-producer, multiple-consumer circular buffer.

The buffer is composed by N slots. Each slot can be accessed independently with two different
permission levels (Each one implemented via a corresponding C++ class):

`BufferSlotWriteAccess` : grants exclusive access to the slot
`BufferSlotReadAccess`/`BufferSlotConsumeAccess`: grants shared read access to the slot

Additionally, the `BufferSlotConsumeAccess` automatically marks a slot as "consumed" on destruction
so that the class can keep track on all the slots that contains data and were not consumed yet.


## Basic Usage

1) Create a new buffer
 ```
  MTCircularBuffer< int > buff(10);

 ```

2) In the producer thread, acquire a BufferSlotWriteAccess to write the data
 ```
    MTCircularBuffer<int>::BufferSlotWriteAccess wa;
    bool overwrite;
    try{
        buff.write_next( wa, &overwrite );  // overwrite is se to true if the next available slot
                                            // contains data that has was not yet consumed
        // Here the slot is locked and wa can be used to write the data into
        *(wa->data) = 10

    } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
    {
       // Exception is thrown if a timeout occurred while locking the next available slot
    }
    // When wa is destroyed, slot exclusive access is automatically released

 ```

3) In the consumer thread, acquire a BufferSlotConsumeAccess to read and consume the data
 ```
    MTCircularBuffer<int>::BufferSlotConsumeAccess ca;
    bool overwrite;
    try{
        buff.consume_next_available( ca );
        // Here the slot is locked and wa can be used to read the data
        int v = *(wa->data);

    } catch( MTCircularBuffer<int>::SlotAcqTimeout& ex )
    {
       // Exception is thrown if a timeout occurred while locking the next available slot
    }
    // When ca is destroyed, slot read access is automatically released and the data is consumed

 ```

---


The MIT License (MIT)
Copyright (c) 2015 Filippo Bergamasco

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


