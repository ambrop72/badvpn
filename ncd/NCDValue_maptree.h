#define CAVL_PARAM_NAME NCDValue__MapTree
#define CAVL_PARAM_FEATURE_COUNTS 0
#define CAVL_PARAM_FEATURE_KEYS_ARE_INDICES 0
#define CAVL_PARAM_FEATURE_NOKEYS 0
#define CAVL_PARAM_TYPE_ENTRY NCDMapElement
#define CAVL_PARAM_TYPE_LINK NCDValue__maptree_link
#define CAVL_PARAM_TYPE_KEY NCDValue__maptree_key
#define CAVL_PARAM_TYPE_ARG int
#define CAVL_PARAM_VALUE_NULL ((NCDValue__maptree_link)NULL)
#define CAVL_PARAM_FUN_DEREF(arg, link) (link)
#define CAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) NCDValue_Compare(&(entry1).ptr->key, &(entry2).ptr->key)
#define CAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) NCDValue_Compare((key1), &(entry2).ptr->key)
#define CAVL_PARAM_MEMBER_CHILD tree_child
#define CAVL_PARAM_MEMBER_BALANCE tree_balance
#define CAVL_PARAM_MEMBER_PARENT tree_parent
