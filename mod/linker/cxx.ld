/* Symbols for running global constructors and destructors */
_ctors_start = ADDR(.ctors);
_ctors_end = _ctors_start + SIZEOF(.ctors);
_dtors_start = ADDR(.dtors);
_dtors_end = _dtors_start + SIZEOF(.dtors);
