#define CAVL_PARAM_NAME FDMacsTree
#define CAVL_PARAM_FEATURE_COUNTS 0
#define CAVL_PARAM_FEATURE_KEYS_ARE_INDICES 0
#define CAVL_PARAM_FEATURE_NOKEYS 0
#define CAVL_PARAM_TYPE_ENTRY FDMacsTree_entry
#define CAVL_PARAM_TYPE_LINK FDMacsTree_link
#define CAVL_PARAM_TYPE_KEY FDMacsTree_key
#define CAVL_PARAM_TYPE_ARG int
#define CAVL_PARAM_VALUE_NULL ((FDMacsTree_link)NULL)
#define CAVL_PARAM_FUN_DEREF(arg, link) (link)
#define CAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) compare_macs((entry1).ptr->mac, (entry2).ptr->mac)
#define CAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) compare_macs((key1), (entry2).ptr->mac)
#define CAVL_PARAM_MEMBER_CHILD tree_child
#define CAVL_PARAM_MEMBER_BALANCE tree_balance
#define CAVL_PARAM_MEMBER_PARENT tree_parent
