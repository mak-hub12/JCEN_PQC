#ifndef PQCLEAN_ALIAS_H
#define PQCLEAN_ALIAS_H

/* Kyber namespaces */
#include "api.h" /* included from each kyberXX_clean dir during compile with include path */
                 /* BUT we’ll include *one* variant per build/test or include-path order. */

/* Recommended: include per-variant headers explicitly in the task files instead of here,
   e.g. for kyber512:
   #include "PQCLEAN_KYBER512_CLEAN/api.h"
   For dilithium2:
   #include "PQCLEAN_DILITHIUM2_CLEAN/api.h"
*/

#endif
