`TypeIsolationDbg` is a little WinDbg extension to help in dumping/examining the state of the Win32k **Type Isolation** structures.

The WinDbg extension provides the following commands:

    !gptypeisolation [address] : prints the top-level CTypeIsolation structure (default address: win32kbase!gpTypeIsolation)
    !typeisolation [address] : prints a NSInstrumentation::CTypeIsolation structure
    !sectionentry [address] : prints a NSInstrumentation::CSectionEntry structure
    !sectionbitmapallocator [address] : prints a NSInstrumentation::CSectionBitmapAllocator structure
    !rtlbitmap [address] : prints a RTL_BITMAP structure


The output of the extension includes some clickable links to help you follow the Type Isolation data structures. It also decodes XORed pointers to save you a step.

The following snippet shows the output of `TypeIsolationDbg` when dumping the global `CTypeIsolation` object and following the data structures for a single `CSectionEntry`, all the way down to the map of bits representing the busy/free state of the `CSectionEntry`'s slots:

    kd> !gptypeisolation
    win32kbase!gpTypeIsolation is at address 0xffffe6cf95138a98.
    Pointer [1] stored at win32kbase!gpTypeIsolation: 0xffffe6a4400006b0.
    Pointer [2]: 0xffffe6a440000680.
    NSInstrumentation::CTypeIsolation
          +0x000 next                                : 0xffffe6a440000620
          +0x008 previous                            : 0xffffe6a441d8ca20
          +0x010 pushlock                            : 0xffffe6a440000660
          +0x018 size                                : 0xF00 [number of section entries: 0x10]

    kd> !sectionentry ffffe6a440000620
    NSInstrumentation::CSectionEntry
          +0x000 next                                : 0xffffe6a441ca2470
          +0x008 previous                            : 0xffffe6a440000680
          +0x010 section                             : 0xffff86855f09f260
          +0x018 view                                : 0xffffe6a4403a0000
          +0x020 bitmap_allocator                    : 0xffffe6a4400005e0

    kd> !sectionbitmapallocator ffffe6a4400005e0
    NSInstrumentation::CSectionBitmapAllocator
          +0x000 pushlock                            : 0xffffe6a4400005c0
          +0x008 xored_view                          : 0xa410b31c3f332f4c [decoded: 0xffffe6a4403a0000]
          +0x010 xor_key                             : 0x5bef55b87f092f4c
          +0x018 xored_rtl_bitmap                    : 0xa410b31c3f092acc [decoded: 0xffffe6a440000580]
          +0x020 bitmap_hint_index                   : 0xC0
          +0x024 num_commited_views                  : 0x27

    kd> !rtlbitmap ffffe6a440000580
    RTL_BITMAP
          +0x000 size                                : 0xF0
          +0x008 bitmap_buffer                       : 0xffffe6a440000590

    kd> dyb ffffe6a440000590 L20
                       76543210 76543210 76543210 76543210
                       -------- -------- -------- --------
    ffffe6a4`40000590  00000101 00000000 00000110 10110000  05 00 06 b0
    ffffe6a4`40000594  00011100 10000000 11011011 11110110  1c 80 db f6
    ffffe6a4`40000598  01111101 11111111 11111111 11111111  7d ff ff ff
    ffffe6a4`4000059c  11111111 11011111 11110111 01111111  ff df f7 7f
    ffffe6a4`400005a0  11111111 11111111 11111111 01111111  ff ff ff 7f
    ffffe6a4`400005a4  11111101 11111001 11111111 01101111  fd f9 ff 6f
    ffffe6a4`400005a8  11111110 11111111 11111111 11111111  fe ff ff ff
    ffffe6a4`400005ac  11111111 00000011 00000000 00000000  ff 03 00 00

