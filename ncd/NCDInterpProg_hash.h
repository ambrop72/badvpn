#define CHASH_PARAM_NAME NCDInterpProg__Hash
#define CHASH_PARAM_ENTRY NCDInterpProg__hashentry
#define CHASH_PARAM_LINK int
#define CHASH_PARAM_KEY NCDInterpProg__hashkey
#define CHASH_PARAM_ARG NCDInterpProg__hasharg
#define CHASH_PARAM_NULL ((int)-1)
#define CHASH_PARAM_DEREF(arg, link) (&(arg)[(link)])
#define CHASH_PARAM_ENTRYHASH(arg, entry) (badvpn_djb2_hash((const uint8_t *)(entry).ptr->name))
#define CHASH_PARAM_KEYHASH(arg, key) (badvpn_djb2_hash((const uint8_t *)(key)))
#define CHASH_PARAM_COMPARE_ENTRIES(arg, entry1, entry2) (!strcmp((entry1).ptr->name, (entry2).ptr->name))
#define CHASH_PARAM_COMPARE_KEY_ENTRY(arg, key1, entry2) (!strcmp((key1), (entry2).ptr->name))
#define CHASH_PARAM_ENTRY_NEXT hash_next
