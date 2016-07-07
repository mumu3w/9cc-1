### 7cc
---------
*a standard C compiler*

The compiler is designed to be able to compile itself, so it is written in C. And it is intended to support all C99 language features while keeping the code as simple and small as possible.


### Source Roadmap

	Documentation   Documentation.
	include			Header files provided by this compiler.
	sys				System relative utilities.
	utils			Common utilities (system independent).
	test			Test suite.


### HOWTO build

Make sure you have Linux installed. (any distribution is fine)

To build the compiler, run command:

   	make

To build the bootstrap compiler, run command:

   	make bootstrap

To run the test suite:

   	make test


For Mac OS X users:

Nowadays OS X is shipped with Apple's C library headers, which does _NOT_ support a standard compiler. You can still build the compiler like Linux, but cannot include the standard headers.

### Caveats

1. Adding test cases.


### License (GPLv3)

7cc - a standard C compiler

Copyright (c) 2015 Guiyang Huang

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/.


### Reference

1. C Standard Draft: [C99](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf), [C11](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)

2. [C: A Reference Manual](http://careferencemanual.com)

3. X86-64 ABI: [v0.90(Dec 2, 2003)](http://people.freebsd.org/~obrien/amd64-elf-abi.pdf), [v0.99.6 (Oct 7, 2013)](http://www.x86-64.org/documentation/abi.pdf)

4. [DWARF Standard](http://dwarfstd.org/)

5. [《Compilers: Principles, Techniques, and Tools》](http://dragonbook.stanford.edu/)

6. [Intel 64 and IA-32 Architecture Software Developer Manuals](
http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)

7. [Using as](https://sourceware.org/binutils/docs/as/)

8. [GCC](https://github.com/gcc-mirror/gcc)

9. [lcc](https://github.com/drh/lcc)