#define CAVL_PARAM_NAME FPAFramesTree
#define CAVL_PARAM_FEATURE_COUNTS 0
#define CAVL_PARAM_FEATURE_KEYS_ARE_INDICES 0
#define CAVL_PARAM_FEATURE_NOKEYS 0
#define CAVL_PARAM_TYPE_ENTRY FPAFramesTree_entry
#define CAVL_PARAM_TYPE_LINK FPAFramesTree_link
#define CAVL_PARAM_TYPE_KEY fragmentproto_frameid
#define CAVL_PARAM_TYPE_ARG int
#define CAVL_PARAM_VALUE_NULL ((FPAFramesTree_link)NULL)
#define CAVL_PARAM_FUN_DEREF(arg, link) (link)
#define CAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) B_COMPARE((entry1).ptr->id, (entry2).ptr->id)
#define CAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) B_COMPARE((key1), (entry2).ptr->id)
#define CAVL_PARAM_MEMBER_CHILD tree_child
#define CAVL_PARAM_MEMBER_BALANCE tree_balance
#define CAVL_PARAM_MEMBER_PARENT tree_parent
