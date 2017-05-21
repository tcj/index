#include <dos/stdio.h>
#include <stdlib.h>
#include <string.h>
#undef strcmp
#include <ctype.h>
#include <dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <clib/alib_stdio_protos.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#define MAX_TAG_NAME_LENGTH 34
#define INPUT_BUFFER_SIZE	255

/* Exec List node IDs */

#define HTMLTAGNODE_ID 		100
#define HTMLCONTENT_ID		101
#define HTMLTAGCLOSE_ID		102

#define HTMLTAGATTR_LIST	150
#define HTMLTAG_LIST			151

#define ATTRTYPE_INT			200
#define ATTRTYPE_FLOAT		201
#define ATTRTYPE_CHAR		202

#define PRIORITY_VALUE		0

const char SINGLE_QUOTE_CHAR 	= '\'';
const char DOUBLE_QUOTE_CHAR 	= '"';
const char SPACE_CHAR 			= ' ';
const char EQUALS_CHAR 			= '=';

const char *HTMLCOMMENT_TAG 	= "!--";

struct List *tagList = NULL;

struct TagNode {
	struct Node tn_Node;
	char *tn_Name;
	struct List *tn_AttrList;
};

struct TagAttrNode {
	struct Node ta_Node;
	char *ta_Name;
	void *ta_Value;
	UBYTE *ta_Data;
};

BPTR stdin;
BPTR stdout;

BPTR inspector;


BOOL openInspector ( void )
{
	inspector = Open("CON:420/10/140/250/Inspector/CLOSE/INACTIVE", MODE_OLDFILE);
	if (inspector)
		return TRUE;
	else 
		return FALSE;

}

void closeInspector ( void )
{
	Close( inspector );
}

void inspectTag ( struct TagNode *tag )
{
	if ( inspector )
	{
		FPrintf( inspector, "c" );
		switch (tag->tn_Node.ln_Type)
		{
			case HTMLTAGNODE_ID:
				FPrintf( inspector, "Tag Name:\n %s\n", tag->tn_Name );
				break;
			case HTMLCONTENT_ID:
				FPrintf( inspector, "Content:\n%s\n", tag->tn_Name );
				break;
			case HTMLTAGCLOSE_ID:
				FPrintf( inspector, "Close tag:\n %s\n", tag->tn_Name );
				break;
			default:
				break;
		}
	}
}

/* str_to_upper()
**
**		change a string IN PLACE to upper-case
*/

char *str_to_upper(char *s)
{
	int i;
	char c;
	
	for (i = 0; i < strlen(s); i++)
	{
		c = *(s+i);
		*(s+i) = toupper(c);		
	}
	return s;
}

struct TagNode *buildContentNodeAndInsertAfter
			( char *content, struct Node *afterNode, struct List *list )
{
	struct TagNode *tn;
	
	tn = AllocVec( sizeof(struct TagNode), MEMF_PUBLIC | MEMF_CLEAR );
	if (tn)
	{
		tn->tn_Name = content;
		tn->tn_Node.ln_Name = tn->tn_Name;
		tn->tn_Node.ln_Type = HTMLCONTENT_ID;
		tn->tn_Node.ln_Pri  = PRIORITY_VALUE;
		Insert( list, (struct Node *)tn, afterNode);
	}
	return tn;
}

BOOL listIsEmpty( struct List *list )
{
	BOOL empty = FALSE;
	
	if (list->lh_TailPred == (struct Node *)list )
		empty = TRUE;
		
	return empty;
}

void printTagWithAttrs( BPTR myConsole, struct TagNode *tn )
{	
	struct Node *attrNode;
	struct List *tagAttrList;
	
	FPrintf( myConsole, "<%s", tn->tn_Name);
	if (listIsEmpty ( (struct List *)tn->tn_AttrList ) )
//		printf("\t[no attrs\n");
		;
	else
	{
		tagAttrList = tn->tn_AttrList;
		for ( attrNode = tagAttrList->lh_Head; attrNode->ln_Succ ; 
									attrNode = attrNode->ln_Succ )
		{
			FPrintf( myConsole, " [%s] ", ((struct TagAttrNode *)attrNode)->ta_Name);
			if ( ((struct TagAttrNode *)attrNode)->ta_Value) 
				FPrintf( myConsole, "= {%s}", ((struct TagAttrNode *)attrNode)->ta_Value);
		}
	}
	FPrintf( myConsole, ">\n");
}

void printTagWithAttrs2( struct TagNode *tn )
{	
	struct Node *attrNode;
	struct List *tagAttrList;
	
	printf("tag <%s>:\n", tn->tn_Name);
	if (listIsEmpty ( (struct List *)tn->tn_AttrList ) )
		printf("\tno attrs\n");
	else
	{
		tagAttrList = tn->tn_AttrList;
		for ( attrNode = tagAttrList->lh_Head; attrNode->ln_Succ ; 
									attrNode = attrNode->ln_Succ )
		{
			printf("\tattr: [%s]", ((struct TagAttrNode *)attrNode)->ta_Name);
			if ( ((struct TagAttrNode *)attrNode)->ta_Value) 
				printf(" = {%s}", ((struct TagAttrNode *)attrNode)->ta_Value);
			printf("\n");
		}
	}	
}

struct TagAttrNode *addTagAttrNode (struct List *al, char *attr)
{
	struct TagAttrNode *tan;
	
	tan = AllocVec( sizeof(struct TagAttrNode), MEMF_PUBLIC | MEMF_CLEAR );
	if (tan)
	{
		tan->ta_Name = AllocVec( strlen(attr) + 1, MEMF_PUBLIC | MEMF_CLEAR );
		if (tan->ta_Name)
		{
			strcpy(tan->ta_Name, attr);
			tan->ta_Node.ln_Name = tan->ta_Name;
		}
		tan->ta_Node.ln_Type = ATTRTYPE_CHAR;
		tan->ta_Node.ln_Pri = PRIORITY_VALUE;
		AddTail( (struct List *)al, (struct Node *)tan);
//		printf("  attr added: [%s]\n", tan->ta_Name);
	}
	return tan;
}

int addValueToAttrNode ( struct TagAttrNode *tan, char *value)
{
	BOOL success = FALSE;
	
	if ( tan->ta_Value )
	{
		printf("\x07h\t\g\bAttempt to add a second value!\n");
	} 
	else if ( tan->ta_Value = AllocVec( strlen(value) + 1, MEMF_PUBLIC|MEMF_CLEAR) )
	{
		strcpy( (char *)tan->ta_Value, value);
//		printf("   value added: {%s}\n", value);
		success = TRUE;
	};
	
	return success;
}		

void buildAttrList ( struct TagNode *tn, char *buf )
{
	char *c = NULL;
	char *attrs = NULL;
	
	BOOL inSingleQuotes = FALSE;
	BOOL inDoubleQuotes = FALSE;

	BOOL inValue = FALSE;
	BOOL inAttr = TRUE;
	
	char *strStart;
	char *value;
	char *attr;

	struct TagAttrNode *tagattrnode;
	
	attrs = AllocVec( strlen(buf) + 1, MEMF_PUBLIC|MEMF_CLEAR );
	memcpy(attrs, buf, strlen(buf));
//	*(attrs + strlen(buf)) = '\0';

	strStart = attrs;
	attr = strStart;

	tn->tn_AttrList = AllocVec( sizeof(struct List), MEMF_PUBLIC|MEMF_CLEAR );
	NewList( tn->tn_AttrList );
	tn->tn_AttrList->lh_Type = HTMLTAGATTR_LIST;
//	printf("for tag: <%s>\n", tn->tn_Name);

	if( strcmp( tn->tn_Name, "!--" ) == 0 )
	{
/*    printf("we will not parse attrs from comments.\n");   */
		tagattrnode = addTagAttrNode( tn->tn_AttrList, "comment" );
		addValueToAttrNode (tagattrnode, attrs);
	} 
	else if (tn->tn_AttrList)
	{
		for (c = attrs; (*c) != '\0'; c++)
		{
			if (*c == EQUALS_CHAR && (!inSingleQuotes) && (!inDoubleQuotes) ) 
			{
/*				future chars will be a value, not an attribute			*/
				inAttr = FALSE;
				inValue = TRUE;
				*c = '\0';
/*				set the characters we've looked at so far to an Attr 	*/
				attr = strStart;
//				printf(" attr: [%s]", attr);
				tagattrnode = addTagAttrNode( tn->tn_AttrList, attr );
/*				and future characters to a Value								*/
				strStart = c + 1;
			} 
			else if ( (*c == SINGLE_QUOTE_CHAR) && (!inDoubleQuotes) )
			{
/*				take out the first/last quote mark from value		 	*/
				if (!inSingleQuotes)
					strStart++;
				else 
					*c = '\0';

				inSingleQuotes = !inSingleQuotes;				
			}
			else if ( (*c == DOUBLE_QUOTE_CHAR) && (!inSingleQuotes) )
			{
/*				take out the first/last quote mark from value		 	*/
				if (!inDoubleQuotes)
					strStart++;
				else 
					*c = '\0';
				inDoubleQuotes = !inDoubleQuotes;
			}
			else if ( (*c == SPACE_CHAR) && 
									(!inSingleQuotes) && (!inDoubleQuotes) &&
									( *(c+1) != SPACE_CHAR) )
			{
/*				future chars will be an attribute, not a value			*/
				inValue = FALSE;
				inAttr = TRUE;
				*c = '\0';
				value = strStart;
//				printf(" value: {%s}", value);
				addValueToAttrNode( tagattrnode, value );
				strStart = c + 1;	
			} 
		}
		if (strStart < c)
		{
			if (inAttr)
			{
				attr = strStart;
//				printf(" lastattr: [%s] ", attr);
				tagattrnode = addTagAttrNode( tn->tn_AttrList, attr );
			} else {
				value = strStart;
//				printf(" lastvalue: {%s} ", value);
				addValueToAttrNode( tagattrnode, value );
			}			
		}
//		printf("\n");	
	}
	if (attrs)
		FreeVec(attrs);
}

void freeAttrList( struct List *attrList )
{
	struct TagAttrNode *worknode;
	struct TagAttrNode *nextnode;

	worknode = (struct TagAttrNode *)(attrList->lh_Head);	/* first node */
	while (nextnode = (struct TagAttrNode *)(worknode->ta_Node.ln_Succ)) 
	{
		FreeVec( worknode->ta_Name );
		if ( worknode->ta_Value )
			FreeVec( worknode->ta_Value );
//		if ( worknode->ta_Data )
//			FreeVec( worknode->ta_Data );
		FreeVec( worknode );
		worknode = nextnode;
	}
}

void freeTagNodes( struct List *list )
{
	struct TagNode *worknode;
	struct TagNode *nextnode;

	worknode = (struct TagNode *)(list->lh_Head);   /* first node */
	while (nextnode = (struct TagNode *)(worknode->tn_Node.ln_Succ)) {
		FreeVec( worknode->tn_Name );
		if ( !(listIsEmpty( worknode->tn_AttrList )) ) {
			freeAttrList ( worknode->tn_AttrList );
			FreeVec( worknode->tn_AttrList );
		}
		FreeVec( worknode );
		worknode = nextnode;
	}
}	


struct TagNode *addTagToList ( struct List *list, char *tagName )
{
	struct TagNode *tn;
	
	tn = AllocVec( sizeof(struct TagNode), MEMF_CLEAR) ;
	if (tn)
	{
		tn->tn_Name = ( AllocVec( MAX_TAG_NAME_LENGTH, MEMF_CLEAR ) );
		strncpy( tn->tn_Name, tagName, MAX_TAG_NAME_LENGTH );
		tn->tn_Node.ln_Name = ( tn->tn_Name );
		tn->tn_Node.ln_Type = ( *(tagName) == '/') ? HTMLTAGCLOSE_ID : HTMLTAGNODE_ID;
		tn->tn_Node.ln_Pri  = PRIORITY_VALUE;
		
		AddTail( list, (struct Node *)tn );
	} else {
		printf("running out of memory!\n");
	}
	return tn;
}	


struct TagNode *scanTagTok( char *tag, int tagSize )
{
	char *token;
	char *tagBuf;
	
	struct TagNode *tn;
	
	int tokenCount = 0;

/*	Maybe we don't need tagBuf?									*/
	
	tagBuf = AllocVec( tagSize + 1, MEMF_CLEAR );
	if (tagBuf)
	{
		memcpy( tagBuf, tag, tagSize );
//		printf("tagBuf: [%s]\n", tagBuf);

		token = strtok( tagBuf, "< >" );
		while ( token != NULL )
		{
			tokenCount++;
			if (tokenCount == 1)
			{
/*				We have found tag name.  Add to list. 			*/
				tn = addTagToList( tagList, token );
//				printf("tagname: %s. ", token);
			} else {
/*				Continue on down the tag...						*/
				buildAttrList( tn, token );
			}
			token = strtok( NULL, ">" );
		}
//		printf("\n");
		FreeVec( tagBuf );
	}
	return tn;
}

/* scanBufferRecursively()
**
**		requires a large stack!  as large as bufSize, it seems...
*/

void scanBufferRecursively ( char *buf, ULONG bufSize, struct TagNode *lastTag )
{
	char *leftTagPoint = NULL;
	char *rightTagPoint = NULL;
	
	char *content;
	char *contentLoc;
	char *contentEnd;
	int contentSize;

	struct TagNode *tn;

	int whatsLeft = 0;

	leftTagPoint = memchr( (const void *)buf, '<', bufSize );
	if (leftTagPoint)
	{
		contentSize = ( leftTagPoint - buf );
		if (contentSize)
		{	
/*			Advance past control characters and spaces		*/			
			for (contentLoc = buf ; 
					(contentLoc < leftTagPoint) && 
						( iscntrl(*contentLoc) || isspace(*contentLoc) ); 
					contentLoc++, contentSize--)
				;
			if (contentLoc < leftTagPoint)
			{
/*				Remove trailing control chars and spaces		*/			
				for (contentEnd = leftTagPoint - 1;
						(contentEnd > contentLoc) &&
							( iscntrl(*contentEnd) || isspace(*contentEnd) );
						contentEnd--, contentSize--)
					;
				if ( !(contentLoc == contentEnd) )
/*				if anything is left after stripping, save it	*/	
				{
					content = AllocVec( contentSize + 1, MEMF_PUBLIC | MEMF_CLEAR );
					if (content)
					{
						memcpy( content, contentLoc, contentSize );
//						printf("content = |%s|\n", content);
						if ( lastTag != NULL )
							buildContentNodeAndInsertAfter
								( content, (struct Node *)lastTag, tagList );
					}
				}
			}
		}
		rightTagPoint = memchr( (const void *)leftTagPoint, 
			'>', (bufSize - (leftTagPoint - buf)) );
		if (rightTagPoint)
		{
//			scanTag(leftTagPoint + 1, (rightTagPoint - leftTagPoint) - 2);
			tn = scanTagTok( leftTagPoint, (rightTagPoint - leftTagPoint) + 1);
				
			whatsLeft = ( bufSize - ( (rightTagPoint + 1) - buf) );
			if ( whatsLeft < bufSize ) 
				scanBufferRecursively( rightTagPoint + 1, whatsLeft, tn);
		}
	}
}

char *lastTagAttr( struct TagNode *tag )
{
	struct List *attrList = tag->tn_AttrList;

	char *tagName = NULL;
	
	if (attrList)
	{
		if ( !(listIsEmpty( attrList ) ) )
		{
			tagName = attrList->lh_TailPred->ln_Name;
		}
	}
	return tagName;
}	

void showNodes ( struct List *list )
{
	struct Node *node;
	int totalNodes = 0;
	int indentLevel = 0;
	int i;
	
	if ( list )
	{
		for (node = list->lh_Head ; node->ln_Succ ; node = node->ln_Succ)
		{
			totalNodes++;
				
//			printf("tagName: %s, %s\n", node->ln_Name, 
//				(node->ln_Type == 100) ? "HTMLTAGNODE_ID" : "HTMLTAGCLOSE_ID");
//			printAttrs( ( (struct TagNode *)node )->tn_Attrs );
			switch (node->ln_Type)
			{
				case HTMLTAGNODE_ID:
					for (i = 0 ; i < indentLevel ; i++)
						printf(" ");						
					printTagWithAttrs2( (struct TagNode *)node );

					if ( strcmp(lastTagAttr( (struct TagNode *)node ), "/") == 0)
					{ ; } 
					else if ( strcmp(node->ln_Name, "!--") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "IMG") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "img") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "BR") == 0 )
					{ ; }
					else
						indentLevel++;

					break;
				case HTMLTAGCLOSE_ID:
					indentLevel--;

					for (i = 0 ; i < indentLevel ; i++)
						printf(" ");

					printTagWithAttrs2( (struct TagNode *)node );
					break;
				case HTMLCONTENT_ID:
					printf("%s\n", node->ln_Name);
					break;
				default:
					break;
			}
		}
		printf("total nodes: %i.\n", totalNodes);
		printf("leftover indent level: %i\n", indentLevel);
	}
}

void showAllContent( struct List *list )
{
	struct Node *node;
	
	if( list )
	{
		for( node = list->lh_Head ; node->ln_Succ ; node = node->ln_Succ )
		{
			switch( node->ln_Type )
			{
				case HTMLCONTENT_ID:
				/*
					if( ( node->ln_Pred->ln_Type = HTMLTAGCLOSE_ID )
						&& ( strnicmp( node->ln_Pred->ln_Name, "/script", 7 ) ) )
						Printf( "[script]\n" );
					else 
				*/
						Printf( "%s\n", node->ln_Name );
					break;
				default:
					break;
			};
		}
	}
}				

void showNodesCon ( struct List *list )
{
	struct Node *node;
	long totalNodes = 0;
	long indentLevel = 0;
	long i;

	BPTR myConsole;
	
	myConsole = Open("CON:10/30/500/200/Output Window/CLOSE/WAIT", MODE_OLDFILE);
	if ( !myConsole )
		printf("Error %i opening console\n", IoErr() );
	
//	Write( myConsole, "hello.\n", 7 );
	
	if ( list )
	{
		for (node = list->lh_Head ; node->ln_Succ ; node = node->ln_Succ)
		{
			totalNodes++;
				
//			printf("tagName: %s, %s\n", node->ln_Name, 
//				(node->ln_Type == 100) ? "HTMLTAGNODE_ID" : "HTMLTAGCLOSE_ID");
//			printAttrs( ( (struct TagNode *)node )->tn_Attrs );
			switch (node->ln_Type)
			{
				case HTMLTAGNODE_ID:
					for (i = 0 ; i < indentLevel ; i++)
						FPrintf( myConsole, " ", 1);						
					printTagWithAttrs( myConsole, (struct TagNode *)node );

					if ( strcmp(lastTagAttr( (struct TagNode *)node ), "/") == 0)
					{ ; } 
					else if ( strcmp(node->ln_Name, "!--") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "IMG") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "img") == 0 )
					{ ; }
					else if ( strcmp(node->ln_Name, "BR") == 0 )
					{ ; }
					else
						indentLevel++;

					break;
				case HTMLTAGCLOSE_ID:
					indentLevel--;

					for (i = 0 ; i < indentLevel ; i++)
						FPrintf( myConsole, " ", 1 );

					printTagWithAttrs( myConsole, (struct TagNode *)node );
					break;
				case HTMLCONTENT_ID:
					FPrintf( myConsole, "%s\n", node->ln_Name );
					break;
				default:
					break;
			}
		}
		FPrintf( myConsole, "total nodes: %ld.\n", totalNodes);
		FPrintf( myConsole, "leftover indent level: %ld\n", indentLevel);
	}
	Close(myConsole);
}



ULONG getFileSize( char *fName )
{
	struct FileLock *lock;
	struct FileInfoBlock *fib_ptr;
	ULONG fSize = 0;
	
	fib_ptr = (struct FileInfoBlock *)
				AllocVec(sizeof(struct FileInfoBlock),
							MEMF_PUBLIC | MEMF_CLEAR);
	if (!fib_ptr)
		printf("not enough memory (fib_ptr)\n");
	else {
		lock = (struct FileLock *) Lock( fName, SHARED_LOCK );
		if (!lock) {
			printf("cannot lock file %s\n", fName);
			FreeVec( fib_ptr );
		} else {
			if (Examine( (BPTR)lock, fib_ptr) == NULL)
			{
				printf("Couldn't examine file\n");
				FreeVec(fib_ptr);
				UnLock( (BPTR)lock );
			} else {
				fSize = fib_ptr->fib_Size;

				UnLock( (BPTR)lock );
				FreeVec( fib_ptr );
			}
		}
	}
	return fSize;
}

/* readHtmlFileIntoBuffer()
**		
**		Allocates buffer, reads file into it, returns pointer to buffer
**		Note:  pointer to bufSize passed as parameter
*/

char *readHtmlFileIntoBuffer( char *fname, ULONG *bufSize )
{
	ULONG a;

	BPTR fp;
	
	char *buf = NULL;

	*bufSize = getFileSize(fname);
	if (*bufSize == 0)
		printf("cannot get size of %s or it is zero bytes\n", fname);
	else {
		buf = AllocVec( (*bufSize) + 1, MEMF_PUBLIC | MEMF_CLEAR);
		if (!buf)
			printf("not enough memory for buffer\n");
		else {
			if ( !( fp = Open( fname, MODE_OLDFILE ) ) )
				printf("cannot open file '%s'\n", fname);
			else {
/*				Read HTML file into buffer        */
				a = Read(fp, (void *)buf, *bufSize);
				Close(fp);
			}
		}
	}
	return buf;
}

int main(int argc, char *argv[])
{
 	ULONG bufSize = 2 << 8; 
	char *buf = NULL;
	unsigned long stackSize;
	char *inputBuf;
	BOOL quitRequested = FALSE;
	struct TagNode *curTag;

/*	We need one argument: the name of file to parse 		*/
	if (argc < 2)
	{
		printf(": %s html-file-to-parse\n", argv[0]);
		exit(1);
	};

/* Insert stack-check here											*/
/* Stack must be at least as large as bufSize, I guess	*/

	stackSize = stacksize();
	if (stackSize < 32000)
	{
		printf("stack size must be >32000.  currently %lu.  sorry.\n", stackSize);
		exit(1);
	}

	stdin = Input();
	stdout = Output();

/* Read given file into buffer, exit on failure 			*/
	buf = readHtmlFileIntoBuffer( argv[1], &bufSize );
	if ( !buf ) 
	{
		printf("no buffer -- exiting\n");
		exit(1);
	}

/*	Allocate our initial Exec List to store tags 			*/
	if ( !(tagList = AllocVec(sizeof(struct List), MEMF_CLEAR)) )
		printf("Out of memory (tagList)\n");
	else 
	{
/*		amiga.lib call to initialize Exec list					*/
		NewList( tagList );
		tagList->lh_Type = HTMLTAG_LIST;
					
/*		Scan buffer for tags 										*/
		scanBufferRecursively( buf, bufSize, NULL );

/*		Let's see what we got 										*/
//		showNodesCon( tagList );

		if ( inputBuf = AllocVec( INPUT_BUFFER_SIZE, MEMF_PUBLIC | MEMF_CLEAR ) )
		{		
			if ( openInspector() )
			{
				if ( !( listIsEmpty( tagList ) ) )
				{
					curTag = (struct TagNode *)tagList->lh_Head;
					inspectTag( curTag );
					Flush(stdin);
					while ( !quitRequested )
					{
						FPrintf(stdout, "html> ");
						Flush(stdout);
						inputBuf = FGets( stdin, (STRPTR)inputBuf, INPUT_BUFFER_SIZE );
			
						if ( inputBuf == NULL || ( strnicmp( inputBuf, "quit", 4 ) == 0 ) )
							quitRequested = TRUE;
						else
						if ( strnicmp( inputBuf, "next", 4 ) == 0)
						{
							if ( curTag == (struct TagNode *)tagList->lh_TailPred )
								printf("already at tail\n");
							else
							{
								curTag = (struct TagNode *)curTag->tn_Node.ln_Succ;
								inspectTag( curTag );
							}
						}
						else
						if ( strnicmp( inputBuf, "prev", 4 ) == 0)
						{
							if ( curTag == (struct TagNode *)tagList->lh_Head )
								printf("already at head\n");
							else
							{
								curTag = (struct TagNode *)curTag->tn_Node.ln_Pred;
								inspectTag( curTag );
							}
						}
						else
						if( strnicmp( inputBuf, "showallcontent", 14 ) == 0 )
						{
							showAllContent( tagList);
						}
						else
						if ( strnicmp( inputBuf, "tail", 4) == 0 )
						{
							curTag = (struct TagNode *)tagList->lh_TailPred;
							inspectTag( curTag );
						} 
						else
						if ( strnicmp( inputBuf, "head", 4) == 0 )
						{
							curTag = (struct TagNode *)tagList->lh_Head;
							inspectTag( curTag );
						}	
						else
							printf( "unknown command: %s", inputBuf );
					}
					printf("Quitting...\n"); 
				}
				closeInspector();
			}
			FreeVec(inputBuf);
		}	

/*		Clean up after ourselves (free memory) 				*/
		freeTagNodes( tagList );
		
		FreeVec( tagList );
	}
	FreeVec( buf );
	return 0;	 
}
