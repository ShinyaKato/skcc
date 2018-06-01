#define def

#ifdef def
if_group
#else
else_group
#endif

#ifndef ndef
if_group
#else
else_group
#endif


#define macro1 defined(xxx)
#define macro2 xxx

#if macro1
if1
#else
else1
#endif

#if defined(macro2)
if2
#else
else2
#endif

#if defined(macro1)
if3
#else
else3
#endif

#define defed defined

#if defed macro1
if4
#else
else4
#endif

#define macro5
#if defined macro5
if5
#else
else5
#endif

#if defined(macro1) && !defined(macro2)
if6
#else
else6
#endif
