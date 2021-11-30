# bsdiff

bsdiff is a library for building and applying patches to binary files.

The original algorithm and implementation was developed by Colin Percival. The algorithm is detailed in his doctoral thesis: <http://www.daemonology.net/papers/thesis.pdf>. For more information visit his website at <http://www.daemonology.net/bsdiff/>.

I maintain this project separately from Colin's work, with the following goals:
* Ability to easily embed the routines as a library instead of an external binary.
* Self-contained 3rd-party libraries, build on Windows/Linux/OSX.

There is a branch "bsdiff_win32" which is based on Andreas John 's [bsdiff Win32-Port](http://sites.inka.de/tesla/others.html#bsdiff).
