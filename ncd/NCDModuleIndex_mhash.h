#define CHASH_PARAM_NAME NCDModuleIndex__MHash
#define CHASH_PARAM_ENTRY NCDModuleIndex__mhash_entry
#define CHASH_PARAM_LINK int
#define CHASH_PARAM_KEY NCDModuleIndex__mhash_key
#define CHASH_PARAM_ARG NCDModuleIndex__mhash_arg
#define CHASH_PARAM_NULL ((int)-1)
#define CHASH_PARAM_DEREF(arg, link) (&(arg)[(link)])
#define CHASH_PARAM_HASHFUN(arg, key) (badvpn_djb2_hash((const uint8_t *)(key)))
#define CHASH_PARAM_KEYSEQUAL(arg, key1, key2) (!strcmp((key1), (key2)))
#define CHASH_PARAM_GETKEY(arg, entry) ((const char *)(entry).ptr->type)
#define CHASH_PARAM_ENTRY_NEXT hash_next
