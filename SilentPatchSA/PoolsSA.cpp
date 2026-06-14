#include "StdAfxSA.h"
#include "PoolsSA.h"

ExternalRef<CObjectPool*> CPools::ms_pObjectPool(AddressByVersion<CObjectPool***>(0x5A18B2 + 2, Memory::PatternAndOffset("8B 48 08 57 33 FF 89 45 F8 89 4D FC", -4) ));