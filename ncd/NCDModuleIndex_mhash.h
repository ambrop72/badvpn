#define CHASH_PARAM_NAME NCDModuleIndex__MHash
#define CHASH_PARAM_ENTRY NCDModuleIndex__mhash_entry
#define CHASH_PARAM_LINK int
#define CHASH_PARAM_KEY NCDModuleIndex__mhash_key
#define CHASH_PARAM_ARG NCDModuleIndex__mhash_arg
#define CHASH_PARAM_NULL ((int)-1)
#define CHASH_PARAM_DEREF(arg, link) (&(arg)[(link)])
#define CHASH_PARAM_ENTRYHASH(arg, entry) (badvpn_djb2_hash((const uint8_t *)(entry).ptr->type))
#define CHASH_PARAM_KEYHASH(arg, key) (badvpn_djb2_hash((const uint8_t *)(key)))
#define CHASH_PARAM_COMPARE_ENTRIES(arg, entry1, entry2) (!strcmp((entry1).ptr->type, (entry2).ptr->type))
#define CHASH_PARAM_COMPARE_KEY_ENTRY(arg, key1, entry2) (!strcmp((key1), (entry2).ptr->type))
#define CHASH_PARAM_ENTRY_NEXT hash_next
