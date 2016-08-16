# C bindings guide

The Mojo C bindings are a way to talk the Mojom protocol, the canonical protocol
for communication between Mojo programs. The library under `bindings/` provides
functionality for encoding, decoding and other computation, so it needs to be
linked together with C code generated from .mojom files. These C bindings are
lower-level than the C++ bindings (or any other language, for that matter),
are more error-prone, and require some knowledge of the C Mojo API and the
mojom encoding format. This document assumes the reader knows about (or knows
how to look up) this relevant information. Consequently, C bindings can also
be faster; generated bindings are smaller than the C++ equivalent, while
encoding and decoding is faster. The intention is to use them only when you
require speed and flexibility.

## Structs

Let's look at what the generated code looks like for the following struct:

``` mojom
module example;

enum Gender { MALE, FEMALE };
struct Person {
  uint32 age;
  string name;
  Gender gender;
};
```

A small snippet of the generated C code for the struct and enum:

```C
// Generated code for mojom enum 'example.Gender'.
typedef uint32_t example_Gender;
enum example_Gender_Enum {
  examples_Gender_MALE = 0,
  examples_Gender_MALE = 1,
};

// Generated code for mojom struct 'example.Person'.
union example_PersonPtr {
  struct example_Person* ptr;
  uint64_t offset;
};
struct example_Person {
  struct MojomStructHeader header_;
  uint32_t age;
  example_Gender gender;
  union MojomStringHeaderPtr name;
};
```

The mojom wire format of a struct is comparable to the C memory model of a
struct, with some restrictions; in the example above, we see that the order of
the fields is different between the mojom and C structs, since the generated C
structs are in packing order, not ordinal order. Although not applicable in this
example, there may be additional fields inserted in the generated C struct for
padding purposes -- since 4-byte data types need to be 4-byte aligned, the
generated C bindings may include some fields not explicitly present in the
mojom. Since it's not immediately obvious where padding fields could be
inserted, it helps to examine the generated C struct to make sure what the
fields are, and if possible, set them using field initializers. The
`example_PersonPtr` union is used to represent an offset in the encoded form, or
a pointer in the unencoded form.

Since mojom objects appear in depth-first order relative to their parent object,
we can use a `struct MojomBuffer` and calls to `MojomBuffer_Allocate(..)` to
linearly allocate space. The struct needs to be constructed and provided by the
user, and it contains 3 fields:  A pointer to the buffer, size of the buffer in
bytes, and the byte-position of the next allocation, typically set to 0.

For instance, to allocate space for the `name` parameter of an `example_Person`,
we can do so this way:
```C
char byte_buffer[512] = {0};
struct MojomBuffer buf = {byte_buffer, sizeof(byte_buffer), 0};

// First allocate space for the example_Person struct:
struct example_Person* person =
   (struct example_Person*)MojomBuffer_Allocate(&buf, sizeof(struct example_Person));

// Allocate enough space for a 10 character string.
person->name.ptr = (struct MojomStringHeader*)MojomBuffer_Allocate(
    &buf,
    sizeof(struct MojomStringHeader) + 10);
```

We can extract how much buffer space was used by reading `buf.num_byes_used`.

Along with the C struct, there are some functions generated that help encode and
decode mojom structs, amongst other things.  For the `example.Person` mojom
struct, the following functions are generated:

```c
struct example_Person* example_Person_DeepCopy(
  struct MojomBuffer* in_buffer,
  struct example_Person* in_data);

void example_Person_EncodePointersAndHandles(
  struct example_Person* inout_struct, uint32_t in_struct_size,
  struct MojomHandleBuffer* inout_handle_buffer);

void example_Person_DecodePointersAndHandles(
  struct example_Person* inout_struct, uint32_t in_struct_size,
  MojomHandle inout_handles[], uin32_t in_num_handles);

MojomValidationResult example_Person_Validate(
  const struct example_Person* in_struct, uint32_t in_struct_size,
  uint32_t in_num_handles);
```

The generated `example_Person_DeepCopy(..)`  function is used to copy over the
`in_data` into another buffer, specified by `MojomBuffer`. The primary purpose
of this function is "linearize" a given `struct example_Person` and its
referenced objects into the new buffer. This essentially recursively copies all
objects in encoding order. The returned copy can then be encoded.

Example usage copying a struct example_Person `person`:
```c
...
char byte_buffer[512] = {0};
struct MojomBuffer buf = {byte_buffer, sizeof(byte_buffer), 0};
struct example_Person* new_person = example_Person_DeepCopy(&buf, person);
assert(new_person != NULL);
...
```

The generated `example_Person_EncodePointersAndHandles(..)` is used to encode
a given C struct so that it's in wire-format, ready to send over a message pipe.
This encoding process involves translating pointers into relative offsets, and
extracting handles out of the struct into a separate handle array (and replacing
the handle values in the struct with references into the handle array). The
supplied `struct MojomHandleBuffer` needs to be constructed and provided by the
user and contains 3 fields: pointer to a handles array, the size of the array
(number of elements), and the starting offset into the array where handles can
be moved into (typically set to 0).

The generated `example_Person_DecodePointersAndHandles(..)` does the inverse --
it translates relative offsets into pointers, and moves handles out of the
handle array and into the struct (based on the encoded offset into the array).
In practice, before decoding a mojom struct into a usable C struct, it should be
first validated; this function may crash on invalid encoded data.

The generated `example_Person_Validate(..)` validates an encoded `struct
example_Person`. The function returns `MOJOM_VALIDATION_ERROR_NONE` if the
struct is valid, in which case the struct can be decoded. See
`bindings/validation.h` for more error codes.

## Interfaces

It isn't enough to talk to other mojo applications by encoding structs and
referenced objects alone; communication happens via interface message calls
(i.e., sending mojom messages), so we need to frame our structs this way. The
following example describes what's generated for interfaces. Consider an
interface `Population` with a message `GetPerson()` that returns a `Person`
object given their name:

```mojom
module example;

[ServiceName="example.EmployeeRegistry"]
interface EmployeeRegistry {
  GetPerson(string name) => (Person person);
};
```

The generated code:
```C
#define example_EmployeeRegistry__ServiceName \
  ((const char*)"example::EmployeeRegistry")
#define example_EmployeeRegistry__CurrentVersion ((uint32_t)0)

// For message GetPerson:
#define example_EmployeeRegistry_GetPerson__Ordinal ((uint32_t)0)
#define example_EmployeeRegistry_GetPerson__MinVersion ((uint32_t)0)

// Request struct for GetPerson():
struct example_EmployeeRegistry_GetPerson_Request {
  struct MojomStructHeader header_;
  struct MojomStringHeaderPtr name;
};

// Response struct for GetPerson():
struct example_EmployeeRegistry_GetPerson_Response {
  struct MojomStructHeader header_;
  struct example_PersonPtr person;
};
```

We see that the parameters (and return value) of the `GetPerson(..)` message are
contained within mojom structs. To send a `GetPerson(..)` request, an interface
request message must be constructed. An interface request message for
`GetPerson(..)` consists of the following data in the following order:

 1. `struct MojomMessageWithRequestId`.  This contains:
   - the message ordinal (generated above) which represents which message it is.
   - flags that say if it's a request or response.
   - a request ID, since this message is expecting a response.
   - (see `bindings/message.h`)
 2. `struct examples_EmployeeRegistry_GetPerson_Request`. This contains the
    actual parameters for GetPerson().

Since the request parameters are just a mojom struct, all relevant functions for
structs are also generated (see above), e.g, ` void
examples_EmployeeRegistry_GetPerson_Request_EncodePointersAndHandles()`. Once
the request struct has been encoded, the buffer containing the above two structs
can be written to a message pipe.

On the other hand, when reading an incoming message, the message header must
first be validated using
```
MojomValidationResult MojomMessage_ValidateHeader(const void* in_buf,
                                                  uint32_t in_buf_size);
```
- If the message arrives on the client side:
  1. It must be validated as a response message.
  2. You must check that the message ordinal is known and expects a response
     message.
- If the message arrives on the server side:
  1. It must be validated as a request message, and that it's a known ordinal.
  2. If the message's ordinal is known to expect a response, the request message
     must be validated as expecting a response.

If valid, it is safe to look at the `request_id` in `struct MojomMessage`, and
the `ordinal` describing the message. By checking if it's any of
`example_EmployeeRegistry_*__Ordinal`, you can further validate that it is a
request or expects a response. See `bindings/message.h` for more functions that
help validate message headers. Once the message header is fully validated, you
must also validate the request or response mojom struct following the message
header using the generated `*_Validate(..)` function.

Note that validation is run on encoded messages and structs on the wire --
decoding a struct without validating it first is dangerous.

## Enums and Constants

Example mojom code:
``` mojom
module example;

enum MyEnum { Zero, One, Four = 4, Five };
const uint64 kMyConst = 34;
```

Generated C code:
``` C
typedef uint32_t example_MyEnum;
enum example_MyEnum_Enum {
  examples_MyEnum_Zero = 0,
  examples_MyEnum_One = 1,
  examples_MyEnum_Four = 4,
  examples_MyEnum_Five = 5,
};

#define example_kMyConst ((uint64_t)34)
```

## Tagged Unions

Example mojom code:
``` mojom
module example;

union MyUnion {
  int8 f0;
  string f1;
  MyUnion f2;
};

struct StructWithUnion {
  MyUnion u;
}
```
Generated C code:
```C
// Generated code for the Tags enum for |MyUnion|.
typedef uint32_t example_MyUnion_Tag;
enum example_MyUnion_Tag_Enum {
  example_MyUnion_Tag_f0 = 0,
  example_MyUnion_Tag_f1 = 1,
  example_MyUnion_Tag_f2 = 2,
  example_MyUnion_Tag__UNKNOWN__ = 0xFFFFFFFF,
};

// Generated code for |MyUnion|.
union example_MyUnionPtr {
  struct example_MyUnion* ptr;
  uint64_t offset;
};
struct example_MyUnion {
  uint32_t size;
  example_MyUnion_Tag tag;
  union {
	int8_t f_f0;
	union MojomStringHeaderPtr f_f1;
	union example_MyUnionPtr f_f2;
    uint64_t unknown;
  } data;
};

// Snippet of generated code for |StructWithUnion|.
struct example_StructWithUnion {
  struct MojomStructHeader header_;
  struct example_MyUnion u;
};
```

Note that the `MyUnion` inside the `MyUnion` is a pointer object, whereas the
`MyUnion` inside `StructWithUnion` is inlined. The only case when unions are
pointer objects are when they are inside another union, otherwise they are
inlined. Unions are initialized by setting their size and their tag. The size is
always 16 bytes if the union is not null (4 for the size field, 4 for the tag,
and 8 for the data). The tag must be set to one defined in the generated enum of
tags. The unknown tag isn't meant to be encoded over the wire, and exists as an
initial value for a union's tag, but the tag should be set to a valid tag
before being written to wire. A union whose size is 0 is considered null. Unlike
for structs, there are no functions generated for unions, since unions are never
encoded as a top-level object type on the wire.

## Arrays and Strings

Arrays and strings (which are just arrays of characters) are not top-level data
types; they can only be defined within a struct, union or interface message.
Arrays inside structs are pointers to an array object. The array object's byte
layout is as follow:
 1. `struct MojomArrayHeader`.  This contains:
   - Number of bytes in the array (this includes the header and the data
     following the array header; see `2.`)
   - Number of elements in the array.
   - (see `bindings/array.h` for more details)
 2. The contents of the array (the size of this is accounted for in the number
    of bytes specified in the array header).

Note that if the array contains pointer objects (structs, arrays, maps), the
array contains only the 8-byte pointers (or offsets in its encoded form) -- the
objects' data follow the array contents, and their size is not accounted for in
the array header.

Example of how to allocate and initialize a new array of 5 int32s, and set each
one:
```C
...
struct MojomArrayHeader* int32_array = MojomArray_New(&buf, 5, sizeof(int32_t));
*MOJOM_ARRAY_INDEX(int32_array, int32_t, 0) = 10;
*MOJOM_ARRAY_INDEX(int32_array, int32_t, 1) = 20;
*MOJOM_ARRAY_INDEX(int32_array, int32_t, 2) = 30;
*MOJOM_ARRAY_INDEX(int32_array, int32_t, 3) = 40;
*MOJOM_ARRAY_INDEX(int32_array, int32_t, 4) = 50;
```

Here, `MojomArray_New(..)` allocates space for the buffer and initializes the
header, while the `MOJOM_ARRAY_INDEX(.., i)` macro returns the address of the
`i`th element.

TODO(vardhan): Explain how to make an array of bools.

Since a mojom string is an array of UTF-8 encoded characters, you can use
`MojomArray_New(&buf, NUM_CHARACTERS, sizeof(uint8_t))` if they are ASCII
characters. Otherwise, since UTF-8 characters may be variable-sized, you must be
careful to set the number of characters appropriately, as it may not be the same
as the number of bytes (minus the header). By convention, mojom strings are not
null-terminated.

## Maps

Maps on the wire are mojom structs with two arrays; one for the keys, and one
for the values. The `i`th element in the keys array corresponds to the `i`th
element in the values array. As such, both arrays must have the same number
of elements, and neither may be null.

# Additional Readings
* [Internals of the generated bindings](INTERNALS.md)
