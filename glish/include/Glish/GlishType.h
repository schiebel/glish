/* $Header$ */

#ifndef glishtype_h
#define glishtype_h


typedef enum {
	/* If you change the order here or add new types, be sure to
	 * update the definition of type_names[] in Value.cc.
	 */
	TYPE_ERROR,
	TYPE_REF, TYPE_CONST,
	TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_DOUBLE,
	TYPE_STRING,
	TYPE_AGENT,
	TYPE_FUNC,
	TYPE_RECORD,
	TYPE_OPAQUE
#define NUM_GLISH_TYPES (((int) TYPE_OPAQUE) + 1)
	} glish_type;

extern const char* type_names[NUM_GLISH_TYPES];

#endif /* glishtype_h */
