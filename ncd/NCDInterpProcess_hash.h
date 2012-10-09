#define CHASH_PARAM_NAME NCDInterpProcess__Hash
#define CHASH_PARAM_ENTRY struct NCDInterpProcess__stmt
#define CHASH_PARAM_LINK int
#define CHASH_PARAM_KEY NCD_string_id_t
#define CHASH_PARAM_ARG NCDInterpProcess_hash_arg
#define CHASH_PARAM_NULL ((int)-1)
#define CHASH_PARAM_DEREF(arg, link) (&(arg)[(link)])
#define CHASH_PARAM_ENTRYHASH(arg, entry) ((size_t)(entry).ptr->name)
#define CHASH_PARAM_KEYHASH(arg, key) ((size_t)(key))
#define CHASH_PARAM_ENTRYHASH_IS_CHEAP 0
#define CHASH_PARAM_COMPARE_ENTRIES(arg, entry1, entry2) ((entry1).ptr->name == (entry2).ptr->name)
#define CHASH_PARAM_COMPARE_KEY_ENTRY(arg, key1, entry2) ((key1) == (entry2).ptr->name)
#define CHASH_PARAM_ENTRY_NEXT hash_next
