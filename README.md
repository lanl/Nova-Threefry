Nova Threefry
=============

**Nova** is a language and runtime compiler that produces code for [Singular Computing](https://www.singularcomputing.com/)'s forthcoming [SIMD](https://en.wikipedia.org/wiki/SIMD) computer, which is based in part on approximate arithmetic.  For example, the assignment `A = B + C` would be written in Nova as `Set(A, Add(B, C))`, yielding a hardware `Add` instruction and possibly some data movement instructions such as `Set`, `Load`, and `Store`.

**Threefry** is a counter-based random-number generator from [D. E. Shaw Research](https://www.deshawresearch.com/)'s [Random123](https://github.com/DEShawResearch/random123) suite.

**Nova Threefry** is a port of Threefry to Nova.

Rationale
---------

Singular Computing's hardware is architected for massive scalability.  Because the compute cores are designed to be extremely simple, they require very little space and power.  Consequently, huge numbers of them can be packed into a single chip.  Just a few compute boards or racks can provide phenomenal peak performance.

The Threefry algorithm is an excellent match to the Singular Computing hardware in that it requires only very simple integer operations (adds, exclusive ORs, and rotates) and virtually no per-core state yet can produce a large number of independent streams of pseudo-random numbers that exhibit a reasonably long period and pass [TestU01](http://simul.iro.umontreal.ca/testu01/tu01.html)'s SmallCrush, Crush, and BigCrush statistical tests.

Caveats
-------

Nova Threefry relies on Singular Computing's proprietary Nova library, for which the source code is not currently generally available.  Consequently, for most people, this repository serves primarily as an example of what's involved in porting a state-of-the-art random-number generator to a contemporary SIMD computer.  However, [Singular Computing](https://www.singularcomputing.com/) welcomes inquiries from parties interested in exploring its currently available hardware systems.

Acknowledgments
---------------

Thanks to John Salmon and Mark Moraes at D. E. Shaw Research for encouragement and Threefry advice and to Joe Bates at Singular Computing for help and guidance with the Nova Threefry implementation.

Legal statement
---------------

Copyright © 2021 Triad National Security, LLC.
All rights reserved.

This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S.  Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.

This program is open source under the [BSD-3 License](LICENSE.md).
Its LANL-internal identifier is C21041, and it is a component of the Billion-Core Monte Carlo project.

Author
------

Scott Pakin, *pakin@lanl.gov*
