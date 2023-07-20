# Consistent Overhead Byte Stuffing (COBS)

This is a C implementation of
[Consistent Overhead Byte Stuffing](http://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing).

## Features
- Designed for efficiency.
- Designed for robustness. Malformed data is detected and reported.
- No memory allocations within the library.
- Inplace decoder variant.
- Streaming encoders and decoders.
- Unit tests.
- Zephyr supports.

## History
This implementation was forked from Jacques Fortier and extended heavily.
For more information, see Jacques Fortiers blog bost on
[Consistent Overhead Byte Stuffing](http://www.jacquesf.com/2011/03/consistent-overhead-byte-stuffing).

## Implementations

### Normal
This is the slightly modified original implementation from Jacques Fortier.
They expect you to provide buffers that are large enough and encode/decode data
from one buffer into another.

### Inplace
Currently only supported for decoding. This removes the need for a second
buffer because it overrides the source data. Since the decoded data is always
smaller than the encoded data it will always fit.

### Streaming
Those allow passing data to the encoder or decoder as it is being received.
The encoder/decoder will tell you when the message is complete or when there
was an error.
