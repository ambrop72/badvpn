#define SAVL_PARAM_NAME BReactor__TimersTree
#define SAVL_PARAM_FEATURE_COUNTS 0
#define SAVL_PARAM_FEATURE_NOKEYS 1
#define SAVL_PARAM_TYPE_ENTRY struct BTimer_t
#define SAVL_PARAM_TYPE_ARG int
#define SAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) compare_timers((entry1), (entry2))
#define SAVL_PARAM_MEMBER_NODE tree_node
