#ifndef _CVMX_GLOBAL_RESOURCES_T_
#define _CVMX_GLOBAL_RESOURCES_T_

#define CVMX_GLOBAL_RESOURCES_DATA_NAME "cvmx-global-resources"
#define ULL unsigned long long

/*In macros below abbreviation GR stands for global resources. */
/*Tag for pko que table range. */
#define CVMX_GR_TAG_PKO_QUEUES cvmx_get_gr_tag('c','v','m','_','p','k','o','_','q','u','e','u','s','.','.','.')
/*Tag for a pko internal ports range */
#define CVMX_GR_TAG_PKO_IPORTS cvmx_get_gr_tag('c','v','m','_','p','k','o','_','i','p','o','r','t','.','.','.')
#define CVMX_GR_TAG_FPA        cvmx_get_gr_tag('c','v','m','_','f','p','a','.','.','.','.','.','.','.','.','.')
#define CVMX_GR_TAG_FAU        cvmx_get_gr_tag('c','v','m','_','f','a','u','.','.','.','.','.','.','.','.','.')

#define TAG_INIT_PART(A,B,C,D,E,F,G,H) \
	((((ULL)A & 0xff) << 56) | (((ULL)B & 0xff) << 48) | (((ULL)C & 0xff) << 40)  | (((ULL)D & 0xff) << 32) | \
	 (((ULL)E & 0xff) << 24) | (((ULL)F & 0xff) << 16) | (((ULL)G & 0xff) << 8)   | (((ULL)H & 0xff)))

struct global_resource_tag
{
	uint64_t lo;
	uint64_t hi;
};

/*
 * @INTERNAL
 * Creates a tag from the specified characters.
 */
static inline struct global_resource_tag
cvmx_get_gr_tag(char a, char b, char c, char d, char e, char f, char g, char h,
		char i, char j, char k, char l, char m, char n, char o, char p)
{
	struct global_resource_tag tag;
	tag.lo =  TAG_INIT_PART(a,b,c,d,e,f,g,h);
	tag.hi =  TAG_INIT_PART(i,j,k,l,m,n,o,p);
	return tag;
}

/*
 * @INTERNAL
 * Creates a global resource range that can hold the specified number of
 * elements
 * @param tag is the tag of the range. The taga is created using the method
 * cvmx_get_gr_tag()
 * @param nelements is the number of elements to be held in the resource range.
 */
int cvmx_create_global_resource_range(struct global_resource_tag tag,int nelements);

/*
 * @INTERNAL
 * Allocate nelements in the global resource range with the specified tag. It
 * is assumed that prior
 * to calling this the global resource range has already been created using
 * cvmx_create_global_resource_range().
 * @param tag is the tag of the global resource range.
 * @param nelements is the number of elements to be allocated.
 * @param owner is a 64 bit number that identifes the owner of this range.
 * @aligment specifes the required alignment of the returned base number.
 * @return returns the base of the allocated range. -1 return value indicates
 * failure.
 */
int cvmx_allocate_global_resource_range(struct global_resource_tag tag,
					uint64_t owner, int nelements,
					int alignment);
/*
 * @INTERNAL
 * Reserve nelements starting from base in the global resource range with the
 * specified tag.
 * It is assumed that prior to calling this the global resource range has
 * already been created using cvmx_create_global_resource_range().
 * @param tag is the tag of the global resource range.
 * @param nelements is the number of elements to be allocated.
 * @param owner is a 64 bit number that identifes the owner of this range.
 * @base specifies the base start of nelements.
 * @return returns the base of the allocated range. -1 return value indicates
 * failure.
 */
int cvmx_reserve_global_resource_range(struct global_resource_tag tag,
				       uint64_t owner, int base, int nelements);
/*
 * @INTERNAL
 * Free nelements starting at base in the global resource range with the
 * specified tag.
 * @param tag is the tag of the global resource range.
 * @param base is the base number
 * @param nelements is the number of elements that are to be freed.
 * @return returns 0 if successful and -1 on failure.
 */
int cvmx_free_global_resource_range_with_base(struct global_resource_tag tag,
						     int base, int nelements);
/*
 * @INTERNAL
 * Frees all the global resources that have been created.
 * For use only from the bootloader, when it shutdown and boots up the
 * application or kernel.
 */
int free_global_resources(void);

/*
 * @INTERNAL
 * Shows the global resource range with the specified tag. Use mainly for debug.
 */
void  cvmx_show_global_resource_range(struct global_resource_tag tag);

/*
 * @INTERNAL
 * Shows all the global resources. Used mainly for debug.
 */
void cvmx_global_resources_show(void);

#endif

