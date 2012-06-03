#define CHASH_PARAM_NAME NCDInterpProg__Hash
#define CHASH_PARAM_ENTRY NCDInterpProg__hashentry
#define CHASH_PARAM_LINK int
#define CHASH_PARAM_KEY NCDInterpProg__hashkey
#define CHASH_PARAM_ARG NCDInterpProg__hasharg
#define CHASH_PARAM_NULL ((int)-1)
#define CHASH_PARAM_DEREF(arg, link) (&(arg)[(link)])
#define CHASH_PARAM_HASHFUN(arg, key) (badvpn_djb2_hash((const uint8_t *)(key)))
#define CHASH_PARAM_KEYSEQUAL(arg, key1, key2) (!strcmp((key1), (key2)))
#define CHASH_PARAM_GETKEY(arg, entry) ((entry).ptr->name)
#define CHASH_PARAM_ENTRY_NEXT hash_next
