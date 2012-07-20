#define CAVL_PARAM_NAME MapTree
#define CAVL_PARAM_FEATURE_COUNTS 1
#define CAVL_PARAM_FEATURE_KEYS_ARE_INDICES 0
#define CAVL_PARAM_FEATURE_NOKEYS 0
#define CAVL_PARAM_TYPE_ENTRY MapTree_entry
#define CAVL_PARAM_TYPE_LINK MapTree_link
#define CAVL_PARAM_TYPE_KEY NCDValRef
#define CAVL_PARAM_TYPE_ARG int
#define CAVL_PARAM_TYPE_COUNT size_t
#define CAVL_PARAM_VALUE_COUNT_MAX SIZE_MAX
#define CAVL_PARAM_VALUE_NULL ((MapTree_link)NULL)
#define CAVL_PARAM_FUN_DEREF(arg, link) (link)
#define CAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) NCDVal_Compare((entry1).ptr->map_parent.key, (entry2).ptr->map_parent.key)
#define CAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) NCDVal_Compare((key1), (entry2).ptr->map_parent.key)
#define CAVL_PARAM_MEMBER_CHILD map_parent.maptree_child
#define CAVL_PARAM_MEMBER_BALANCE map_parent.maptree_balance
#define CAVL_PARAM_MEMBER_PARENT map_parent.maptree_parent
#define CAVL_PARAM_MEMBER_COUNT map_parent.maptree_count
