This document describes some of the internals of the C bindings, along with some
numbers.

# Generated code

One of the goals of the C bindings is to generate minimal code and use little
memory. The C bindings achieve this by generating type descriptors for each
mojom type that contain information required for encoding, decoding and
validation. For instance, type descriptions for mojom structs describe where the
pointer and handle fields in a struct are located, which of them are nullable,
the sizes of different versions of the struct, etc. Using this information, we
can avoid generating separate encoding, decoding and validation code for each
struct, and instead rely on a common encoding function shared amongst all mojom
structs which consume these generated type descriptions. Simiarly, type
descriptions are generated for every instance of arrays, maps, and unions. These
type descriptions live in the `.rodata` (or similar) section of a binary.

# Sizes of type descriptors

The following subsections describe the space cost of type descriptors generated
for different mojom types on a 64-bit system. The C data types for type
descriptors are located in `bindings/type_descriptor.h`, which is the
canonical place for up-to-date documentation of type description tables, how
they are structured and used.

## Structs
A type descriptor for a mojom struct contains:
- A version table (pointer, 8 bytes):
 - Each version is described by a `version` <=> `size of struct` (8 bytes)
 - Number of elements in the version table (4 bytes)
- Pointer/handle location table (pointer, 8 bytes):
 - Array of `struct MojomTypeDescriptorStructEntry` (roughly 20 bytes per
   field), each entry describing a pointer or handle field.
 - Number of entries in the table (4 bytes)

Example:
```mojom
struct Rect {
  int32 x;
  int32 y;
  int32 w;
  int32 h;
};
```

This struct has just 1 version, and no pointer or handle fields. It takes up `8
bytes + 8 bytes + 4 bytes` for the version table, another `8 bytes + 4 bytes`
for the location table, for a total of **32 bytes**. Adding any additional
non-handle/pointer fields will not grow the generated type descriptior for this
mojom struct.

Another example:
```mojom
struct RectPair {
  Rect a;
  Rect b;
};
```

RectPair similarly has 1 version (accounting for `8 + 8 + 4 bytes`), but has 2
pointer types, which puts its location table to `8 + 4 + 20*2 bytes`, bringing
its type descriptor to **72 bytes** in size.

## Arrays and Strings

A type descriptor for a mojom array contains:
- The type of its entries, and a pointer to its type descriptor (roughly 1 + 8
  bytes)
- Number of elements in the array, only applicable if defined in the mojom IDL
  (4 bytes)
- Size (in number of bits) of a single element. (4 bytes)
- If its elements can be nullable (1 byte).

A single array of any type (e.g., `array<int8>`) will therefore take roughly **18
bytes**. However, an array of other composite types can end up forming a chain of
type descriptors. For example, consider `array<array<int8>>`: 18 bytes for the
outer-array, and another 18 bytes for `array<int8>`, totalling **36 bytes**.

Strings are arrays of UTF-8 encoded characters; A type descriptor for a string
is always the same (it is composed of a variable number of characters). For this
reason, the bindings library comes with a single type descriptor for an array of
characters, so a new type descriptor is not generated for every instance of a
string.

## Maps

A map is just a mojom struct with 2 mojom arrays, so a type descriptor for a simple map (mapping a POD
to a POD) could take `32 bytes + 2 * (18 bytes)` = **68 bytes**. A new type
descriptor is generated for every occurance of a mojom map. A possible optimization in
the future is to deduplicate identical type descriptors and share them.

## Unions

TODO(vardhan)

## Interfaces

Interfaces are composed of messages, each of which is composed of a request
parameters, and possibly response parameters. For example:

```mojom
struct Person { ... };

interface Population {
  GetPerson(string name) => (Person p);
};
```

Consits of two mojom structs, equivalent to:
```mojom
struct Population_GetPerson_Request {
  string name;
}
struct Population_GetPerson_Response {
  Person p;
};
```

The request struct takes up **32 bytes** (taken from the example in the
**Structs** section above) **+ 8 bytes** (for the string entry). Similarly, the
response struct takes up another **32+8 bytes** bytes (this is not accounting
for the space taken up by the `struct Person` type descriptor).

## Other types

TODO(vardhan)

# Compiled code size

A lot of these generated type descriptors can be compacted further, if required.
This kind of optimization could be done later when things are more stable. There
are also opportunities to de-duplicate generated type descriptors, notable for
arrays and maps (e.g., we only need one type descriptor for array<int8> that
could be shared across all mojom structs).

Comparing the sizes of an example mojo echo client in C and C++ (which can be
found in the domokit/mojo github repo), built for Linux:

C++ echo client:
```bash
$ size out/Release/echo_client.mojo
  text    data     bss     dec     hex filename
134194    4528     436  139158   21f96 out/Release/echo_client.mojo
```

C echo client:
```bash
$ size out/Release/c_echo_client.mojo
text    data     bss     dec     hex filename
9834    1328     264   11426    2ca2 out/Release/c_echo_client.mojo
```
