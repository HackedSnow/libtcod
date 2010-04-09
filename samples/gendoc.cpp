/*
* libtcod 1.5.1
* Copyright (c) 2008,2009,2010 Jice & Mingos
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * The name of Jice or Mingos may not be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY JICE AND MINGOS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL JICE OR MINGOS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// This is libtcod doc generator
// it parses .hpp files and use javadoc-like comments to build the doc
// work-in-progress!!
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include "libtcod.hpp"

// index.html categories
static const char *categs[] = {"", "Core","Base toolkits","Roguelike toolkits", NULL };

// a function parameter
struct ParamData {
	char *name;
	char *desc;	
}; 

// data about a libtcod function
struct FuncData {
	char *title;// function name
	char *desc; // general description
	char *cpp;  // C++ function
	char *c;    // C function
	char *cs;    // C# function
	char *py;   // python function
	TCODList<ParamData *> params; // parameters table
	char *cppEx; // C++ example
	char *cEx;   // C example	
	char *csEx;    // C# example
	char *pyEx;  // python example
	FuncData() : title(NULL),desc(NULL),cpp(NULL),c(NULL),cs(NULL),py(NULL),
		cppEx(NULL),cEx(NULL),csEx(NULL),pyEx(NULL) {}	
};

// data about a documentation page
struct PageData {
	// parsed data
	char *name; // page symbolic name
	char *title;// page title
	char *desc; // description on top of page
	char *fatherName; // symbolic name of father page if any
	char *filename; // .hpp file from which it comes
	char *categoryName; // category for level 0 pages
	TCODList<FuncData *>funcs; // functions in this page
	PageData() : name(NULL),title(NULL),desc(NULL), fatherName(NULL),
		filename(NULL),categoryName((char *)""),father(NULL),next(NULL),prev(NULL),
		fileOrder(0),order(0),numKids(0),pageNum(NULL),
		url(NULL), breadCrumb(NULL), prevLink((char *)""),nextLink((char*)"") {}
	// computed data
	PageData *father;
	PageData *next;
	PageData *prev;
	int fileOrder; // page number in its hpp file
	int order; // page number in it's father
	int numKids; // number of sub pages inside this one
	char *pageNum; // 1.2 and so on...
	char *url;  // page URL (ex.: 1.2.2.html)
	char *breadCrumb;
	char *prevLink; // link to prev page in breadcrumb
	char *nextLink; // link to next page in breadcrumb
};

TCODList<PageData *> pages;
// root page corresponding to index.html
PageData *root=NULL;
// page currently parsed
PageData *curPage=NULL;
// function currently parsed
FuncData *curFunc=NULL;

// get an identifier at txt pos and put a strdup copy in result
// returns the position after the identifier
char *getIdentifier(char *txt, char **result) {
	while (isspace(*txt)) txt++;
	char *end=txt;
	while (!isspace(*end)) end++;
	*end=0;
	*result=strdup(txt);
	return end+1;	
}

// get the end of line from txt and put a strdup copy in result
// returns the position after the current line
char *getLineEnd(char *txt, char **result) {
	while (isspace(*txt)) txt++;
	char *end=txt;
	while (*end && *end != '\n') end++;
	bool fileEnd = (*end == 0);
	*end=0;
	*result=strdup(txt);
	return fileEnd ? end : end+1;	
}

// get the data from txt up to the next @ directive and put a strdup copy in result
// returns the position at the next @ (or end of file)
char *getParagraph(char *txt, char **result) {
	while (isspace(*txt)) txt++;
	char *end=txt;
	while (*end && *end != '@') end++;
	bool fileEnd = (*end == 0);
	*end=0;
	*result=strdup(txt);
	if ( ! fileEnd ) *end='@';
	return end;	
}

// check if the string starts with keyword
bool startsWith(const char *txt, const char *keyword) {
	return strncmp(txt,keyword,strlen(keyword)) == 0;
}

// print in the file, replace \n by <br>
void printHtml(FILE *f, const char *txt) {
	while (*txt) {
		if ( *txt == '\n' ) fprintf(f,"<br />");
		else if ( *txt == '\r' ) {} // ignore
		else {
			fputc(*txt,f);
		}
		txt++;
	}
}

// print in the file, coloring syntax using the provided lexer
void printSyntaxColored(FILE *f, TCODLex *lex) {
	char *pos=lex->getPos();
	int tok=lex->parse();
	while (tok != TCOD_LEX_ERROR && tok != TCOD_LEX_EOF ) {
		const char *spanClass=NULL;
		switch (tok) {
			case TCOD_LEX_SYMBOL : spanClass = "code-symbol"; break;
			case TCOD_LEX_KEYWORD : spanClass = "code-keyword"; break;
			case TCOD_LEX_STRING : spanClass = "code-string"; break;
			case TCOD_LEX_INTEGER : 
			case TCOD_LEX_FLOAT : 
			case TCOD_LEX_CHAR : 
				spanClass = "code-value"; 
			break;
			case TCOD_LEX_IDEN : 
				if ( strncasecmp(lex->getToken(),"tcod",4) == 0 ) {
					spanClass = "code-tcod"; 
				}
			break;
			default : break;
		}
		if ( spanClass ) {
			fprintf(f, "<span class=\"%s\">",spanClass);
		} 
		while ( pos != lex->getPos() ) {
			if ( *pos == '\r' ) {}
			else if (*pos == '\n' ) fprintf(f,"<br />");
			else fputc(*pos,f);
			pos++;
		}
		if ( spanClass ) {
			fprintf(f, "</span>");
		} 
		tok=lex->parse();
	}
	if ( tok == TCOD_LEX_ERROR ) {
		printf ("ERROR while coloring syntax : %s\n",TCOD_lex_get_last_error());
	}
}

// print in the file, coloring syntax using C++ rules
void printCppCode(FILE *f, const char *txt) {
	static const char *symbols[] = {
	"::","->","++","--","->*",".*","<<",">>","<=",">=","==","!=","&&","||","+=","-=","*=","/=","%=","&=","^=","|=","<<=",">>=","...",
	"(",")","[","]",".","+","-","!","~","*","&","|","%","/","<",">","=","^","?",":",";","{","}",",",
	NULL		
	};
	static const char *keywords[] = {
	"and","and_eq","asm","auto","bitand","bitor","bool","break","case","catch","char","class","compl","const","const_cast","continue",
	"default","delete","do","double","dynamic_cast","else","enum","explicit","export","extern","false","float","for","friend","goto",
	"if","inline","int","long","mutable","namespace","new","not","not_eq","operator","or","or_eq","private","protected","public",
	"register","reinterpret_cast","return","short","signed","sizeof","static","static_cast","struct","switch","template","this",
	"throw","true","try","typedef","typeid","typename","union","unsigned","using","virtual","void","volatile","wchar_t","while",
	"xor","xor_eq",
	NULL
	};
	TCODLex lex(symbols,keywords);
	lex.setDataBuffer((char *)txt);
	printSyntaxColored(f,&lex);	
}

// print in the file, coloring syntax using C rules
void printCCode(FILE *f, const char *txt) {
	static const char *symbols[] = {
		"->","++","--","<<",">>","<=",">=","==","!=","&&","||","*=","/=","+=","-=","%=","<<=",">>=","&=","^=","|=","...",
		"{","}","(",")","[","]",".","&","*","+","-","~","!","/","%","<",">","^","|","?",":","=",",",";",
	};
	static const char *keywords[] = {
	"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","int",
	"long","register","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile",
	"while", 
	NULL
	};
	TCODLex lex(symbols,keywords);
	lex.setDataBuffer((char *)txt);
	printSyntaxColored(f,&lex);	
}

// print in the file, coloring syntax using python rules
void printPyCode(FILE *f, const char *txt) {
	static const char *symbols[] = {
		"**","&&","||","!=","<>","==","<=",">=","+=","-=","**=","*=","//=","/=","%=","|=","^=","<<=",">>=",
		"+","-","^","*","/","%","&","|","^","<",">","(",")","[","]","{","}",",",":",".","`","=",";","@",
		NULL
	};
	static const char *keywords[] = {
	"and","as","assert","break","class","continue","def","del","elif","else","except","exec","finally","for",
	"from","global","if","import","in","is","lambda","not","or","pass","print","raise","return","try","while",
	"with","yield",
	NULL
	};
	TCODLex lex(symbols,keywords,"#","\"\"\"","\"\"\"",NULL,"\"\'");
	lex.setDataBuffer((char *)txt);
	printSyntaxColored(f,&lex);	
}

// print in the file, coloring syntax using C# rules
void printCSCode(FILE *f, const char *txt) {
	static const char *symbols[] = {
		"++","--","->","<<",">>","<=",">=","==","!=","&&","||","+=","-=","*=","/=","%=","&=","|=","^=","<<=",">>=","??",
		".","(",")","[","]","{","}","+","-","!","~","&","*","/","%","<",">","^","|","?",":",
		NULL
	};
	static const char *keywords[] = {
	"abstract","as","base","bool","break","byte","case","catch","char","checked","class","const","continue","decimal","default",
	"delegate","do","double","else","enum","event","explicit","extern","false","finally","fixed","float","for","foreach","goto",
	"implicit","in","int","interface","internal","is","lock","long","namespace","new","null","object","operator","out","override",
	"params","private","protected","public","readonly","ref","return","sbyte","sealed","short","sizeof","stackalloc","static",
	"string","struct","switch","this","throw","true","try","typeof","uint","ulong","unchecked","unsafe","ushort","using","virtual",
	"volatile","void","while",
	"get","partial","set","value","where","yield",
	NULL
	};
	TCODLex lex(symbols,keywords);
	lex.setDataBuffer((char *)txt);
	printSyntaxColored(f,&lex);	
}

// get a page by its name or NULL if it doesn't exist
PageData *getPage(const char *name) {
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		if (strcmp((*it)->name,name)==0) return *it;
	}
	return NULL;
}

// find the previous page of same level (NULL if none)
PageData *getPrev(PageData *page) {
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		if ( (*it)->father == page->father && (*it)->order == page->order-1 ) return *it;
	}
	return NULL;	
}

// find the next page of same level (NULL if none)
PageData *getNext(PageData *page) {
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		if ( (*it)->father == page->father && (*it)->order == page->order+1 ) return *it;
	}
	return NULL;	
}

// parse a .hpp file and generate corresponding PageData
void parseFile(char *filename) {
	// load the file into memory (should probably use mmap instead that crap...)
	struct stat _st;
	printf ("INFO : parsing file %s\n",filename);
    FILE *f = fopen( filename, "r" );
    if ( f == NULL ) {
		printf("WARN : cannot open '%s'\n", filename);
		return ;
    }
    if ( stat( filename, &_st ) == -1 ) {
		fclose(f);
		printf("WARN : cannot stat '%s'\n", filename );
		return ;
    }
    char *buf = (char*)calloc(sizeof(char),(_st.st_size + 1));
	char *ptr=buf;
	// can't rely on size to read because of MS/DOS dumb CR/LF handling
	while ( fgets(ptr, _st.st_size,f ) ) {
		ptr += strlen(ptr);
	}
    fclose(f);
	// remove \r
	ptr = buf;
	while (*ptr) {
		if ( *ptr == '\r') {
			char *ptr2=ptr;
			while ( *ptr2 ) {
				*ptr2 = *(ptr2+1);
				ptr2++;
			}
		}
		ptr++;
	}
    
    // now scan javadocs
    int fileOrder=1;
    ptr = strstr(buf,"/**");
    while (ptr) {
    	char *end = strstr(ptr,"*/");
    	if ( end ) {
	    	// parse the javadoc
	    	*end=0;
	    	char *directive = strchr(ptr,'@');
	    	while ( directive ) {
	    		if ( startsWith(directive,"@PageName") ) {
	    			char *pageName=NULL;
	    			directive = getIdentifier(directive+sizeof("@PageName"),&pageName);
	    			curPage=getPage(pageName);
					curFunc=NULL;
	    			if(!curPage) {
						// non existing page. create a new one
						curPage=new PageData();
						pages.push(curPage);
						curPage->filename = strdup(filename);
						curPage->fileOrder=fileOrder++;
						curPage->name=pageName;
						curFunc=NULL;
					}
	    		} else if ( startsWith(directive,"@PageTitle") ) {
	    			directive = getLineEnd(directive+sizeof("@PageTitle"),&curPage->title);
	    		} else if ( startsWith(directive,"@PageDesc") ) {
	    			directive = getParagraph(directive+sizeof("@PageDesc"),&curPage->desc);
	    		} else if ( startsWith(directive,"@PageFather") ) {
	    			directive = getIdentifier(directive+sizeof("@PageFather"),&curPage->fatherName);
	    		} else if ( startsWith(directive,"@PageCategory") ) {
	    			directive = getLineEnd(directive+sizeof("@PageCategory"),&curPage->categoryName);
	    		} else if ( startsWith(directive,"@FuncTitle") ) {
					curFunc=new FuncData();
					curPage->funcs.push(curFunc);
	    			directive = getLineEnd(directive+sizeof("@FuncTitle"),&curFunc->title);
	    		} else if ( startsWith(directive,"@FuncDesc") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@FuncDesc"),&curFunc->desc);
	    		} else if ( startsWith(directive,"@CppEx") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@CppEx"),&curFunc->cppEx);
	    		} else if ( startsWith(directive,"@C#Ex") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@C#Ex"),&curFunc->csEx);
	    		} else if ( startsWith(directive,"@CEx") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@CEx"),&curFunc->cEx);
	    		} else if ( startsWith(directive,"@PyEx") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@PyEx"),&curFunc->pyEx);
	    		} else if ( startsWith(directive,"@Cpp") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@Cpp"),&curFunc->cpp);
	    		} else if ( startsWith(directive,"@C#") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@C#"),&curFunc->cs);
	    		} else if ( startsWith(directive,"@C") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@C"),&curFunc->c);
	    		} else if ( startsWith(directive,"@Py") ) {
					if (! curFunc ) {
						curFunc=new FuncData();
						curPage->funcs.push(curFunc);
					}
	    			directive = getParagraph(directive+sizeof("@Py"),&curFunc->py);
	    		} else if ( startsWith(directive,"@Param") ) {
					ParamData *param=new ParamData();
					curFunc->params.push(param);
	    			directive = getIdentifier(directive+sizeof("@Param"),&param->name);
	    			directive = getParagraph(directive,&param->desc);
				} else {
					char *tmp;
					directive = getIdentifier(directive,&tmp);
					printf ("WARN unknown directive  '%s'\n",tmp);
					free(tmp);
				}
				directive = strchr(directive,'@');
			}
	    	ptr=strstr(end+2,"/**");
	    } else ptr=NULL;
	}
}

// computes the page tree and auto-numbers pages
void buildTree() {
	// get the list of root (level 0) pages
	TCODList<PageData *>rootPages;
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		// page requires at least a @PageName and @PageTitle
		if (! (*it)->name ) {
			printf ("ERROR : page #%d (%s) in %s has no @PageName\n",
				(*it)->fileOrder,(*it)->name ? (*it)->name : "null", (*it)->filename);
			it=pages.remove(it);
			continue;
		}
		if (! (*it)->title ) {
			printf ("ERROR : page #%d (%s) in %s has no @PageTitle\n",
				(*it)->fileOrder,(*it)->name ? (*it)->name : "null",(*it)->filename);
			it=pages.remove(it);
			continue;
		}
		if ( (*it)->fatherName == NULL ) rootPages.push(*it);
	}
	// first, order the level 0 pages according to their category
	int categId=0;
	while ( categs[categId] ) {	
		for (PageData **it=pages.begin();it!=pages.end();it++) {
			if ( (*it)->fatherName == NULL && strcmp((*it)->categoryName,categs[categId]) == 0 ) {
				// new root page
				root->numKids++;
				(*it)->father=root;
				(*it)->order=root->numKids;
				char tmp[1024];
				sprintf(tmp,"%d",(*it)->order);
				(*it)->pageNum=strdup(tmp);
				sprintf(tmp,"doc/html2/%s.html",(*it)->name);
				(*it)->url=strdup(tmp);
				sprintf(tmp,"<a href=\"../index2.html\">Index</a> &gt; <a href=\"%s.html\">%s. %s</a>",
					(*it)->name,(*it)->pageNum,(*it)->title);
				(*it)->breadCrumb=strdup(tmp);						
				rootPages.remove(*it);
			}
		}
		categId++;
	}
	// pages with unknown categories
	for ( PageData **it=rootPages.begin(); it != rootPages.end(); it++) {
		printf ("ERROR : unknown category '%s' in page '%s'\n",(*it)->categoryName,(*it)->name);
		pages.remove(*it);
	}
	// build the subpages tree
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		if ( (*it)->fatherName != NULL ) {
			// sub-page. find its daddy and order
			(*it)->father=getPage((*it)->fatherName);
			if ( ! (*it)->father ) {
				printf ("ERROR : unknown father '%s' for page '%s'\n",
					(*it)->fatherName,(*it)->name);
				it=pages.remove(it);
				continue;
			}
			(*it)->father->numKids++;
			(*it)->order=(*it)->father->numKids;
		}
	}
	// now compute sub-page numbers 
	TCODList<PageData *> hierarchy;
	bool missing=true;
	while ( missing ) {
		missing=false;
		for (PageData **it=pages.begin();it!=pages.end();it++) {
			if ((*it)->pageNum == NULL ) {
				PageData *page=*it;
				if ( page->father->pageNum == NULL ) {
					missing=true;
				} else {                
					char tmp[256];
					sprintf(tmp,"%s.%d", page->father->pageNum,page->order);
					page->pageNum = strdup(tmp);
					sprintf(tmp,"doc/html2/%s.html",page->name);
					page->url=strdup(tmp);
				}
			}
		}
	}
	// now compute prev/next links and breadcrumbs for sub pages
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		PageData *page=*it;
		page->prev=getPrev(page);
		// prev link
		if ( page->prev ) {
			char tmp[1024];
			sprintf (tmp,
				"<a class=\"prev\" href=\"%s.html\">%s. %s</a>",
				page->prev->name,page->prev->pageNum,page->prev->title);
			page->prevLink=strdup(tmp);
		}
		// next link
		page->next=getNext(page);
		if ( page->next ) {
			char tmp[1024];
			sprintf (tmp,
				"%s<a class=\"next\" href=\"%s.html\">%s. %s</a>",
				page->prev ? "| " : "",
				page->next->name,page->next->pageNum,page->next->title);
			page->nextLink=strdup(tmp);
		}
		// breadCrumb
		if (! page->breadCrumb ) {
			char tmp[1024];
			TCODList<PageData *> hierarchy;
			PageData *curPage=page;
			while ( curPage ) {
				hierarchy.push(curPage);
				curPage=curPage->father;
			}
			char *ptr=tmp;
			ptr[0]=0;
			while ( ! hierarchy.isEmpty() ) {
				curPage=hierarchy.pop();
				if ( curPage == root ) {
					sprintf(ptr, "<a href=\"../%s.html\">%s</a>", 
						curPage->name,curPage->title);
				} else {
					sprintf(ptr, " &gt; <a href=\"%s.html\">%s. %s</a>", 
						curPage->name,curPage->pageNum,curPage->title);
				}
				ptr += strlen(ptr);
			}
			page->breadCrumb =strdup(tmp);
		}
	}
}

// return the subpage # kidNum
PageData *getKidByNum(PageData *father, int kidNum) {
	for (PageData **it=pages.begin(); it != pages.end(); it++) {
		if ( (*it)->father == father && (*it)->order == kidNum ) {
			return *it;
		}
	}
	return NULL;
}

// print subpage TOC for a standard (not index.htlm) page
void printStandardPageToc(FILE *f,PageData *page) {
	if ( page->numKids > 0 ) {
		fprintf(f,"<div id=\"toc\"><ul>");
		for (int kidId=1;kidId <= page->numKids; kidId++) {
			// find the kid # kidId
			PageData *kid = getKidByNum(page,kidId);
			if ( kid ) {
				if ( kid->numKids > 0 ) {
					// level 2
					fprintf (f,"<li class=\"haschildren\"><a href=\"%s%s.html\">%s. %s</a><ul>\n",
						page==root ? "html2/":"",
						kid->name,kid->pageNum,kid->title);
					int kidKidId=1;
					while (kidKidId <= kid->numKids ) {
						PageData *kidKid=NULL;
						// find the kid # kidKidId of kidId
						kidKid=getKidByNum(kid,kidKidId);
						if ( kidKid ) {
							fprintf(f,"<li><a href=\"%s%s.html\">%s. %s</a></li>\n",
							page==root ? "html2/":"",
								kidKid->name,kidKid->pageNum,kidKid->title);

						}
						kidKidId++;
					}
					fprintf(f,"</ul></li>\n");
				} else {
					fprintf(f,"<li><a href=\"%s%s.html\">%s. %s</a></li>\n",
						page==root ? "html2/":"",
						kid->name,kid->pageNum,kid->title);
				}
			}
		}
		fprintf(f,"</ul></div>\n");
	}
}

// print index.html TOC
void printRootPageToc(FILE *f,PageData *page) {
	int categId=0;
	fprintf(f,"<div id=\"toc\"><ul>");
	while ( categs[categId] ) {	
		if ( categId > 0 ) fprintf(f, "<li class=\"cat\">%s</li>\n",categs[categId]);
		for ( int kidId=1;kidId <= page->numKids; kidId++ ) {
			PageData *kid = getKidByNum(page,kidId);
			if ( kid && strcmp(kid->categoryName,categs[categId]) ==0 ) {
				if ( kid->numKids > 0 ) {
					// level 2
					fprintf (f,"<li class=\"haschildren\"><a href=\"%s%s.html\">%s. %s</a><ul>\n",
						page==root ? "html2/":"",
						kid->name,kid->pageNum,kid->title);
					int kidKidId=1;
					while (kidKidId <= kid->numKids ) {
						PageData *kidKid=getKidByNum(kid,kidKidId);
						if ( kidKid ) {
							fprintf(f,"<li><a href=\"%s%s.html\">%s. %s</a></li>\n",
							page==root ? "html2/":"",
								kidKid->name,kidKid->pageNum,kidKid->title);

						}
						kidKidId++;
					}
					fprintf(f,"</ul></li>\n");
				} else {
					fprintf(f,"<li><a href=\"%s%s.html\">%s. %s</a></li>\n",
						page==root ? "html2/":"",
						kid->name,kid->pageNum,kid->title);
				}
			}
		}
		categId++;
	}
	fprintf(f,"</ul></div>\n");
}

// generate html file for one page
void genPageDoc(PageData *page) {
	FILE *f = fopen(page->url,"wt");
	static const char *header=
"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\n"
"<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=ISO-8859-1\">\n"
"<title>libtcod documentation | %s</title>\n"
"<link href=\"%s\" rel=\"stylesheet\" type=\"text/css\"></head>\n"
"<body><div class=\"header\">\n"
"<p><span class=\"title1\">libtcod</span>&nbsp;<span class=\"title2\">documentation</span></p>\n"
"</div>\n"
"<div class=\"breadcrumb\"><div class=\"breadcrumbtext\"><p>\n"
"you are here: %s<br>\n"
"%s %s\n"
"</p></div></div><div class=\"main\"><div class=\"maintext\">\n";

	static const char *footer="</div></div></body></html>";
	// page header with breadcrumb
	fprintf(f,header, page->title, page == root ? "css/style.css" : "../css/style.css", page->breadCrumb,
		page->prevLink, page->nextLink
		);
	// page title
	if ( page == root ) fprintf(f,"<h1>%s</h1>\n",page->desc);
	else fprintf(f,"<h1>%s. %s</h1>\n",page->pageNum, page->title);
	// page description
	if ( page != root && page->desc ) {
		fprintf(f,"<p>");
		printHtml(f,page->desc);
		fprintf(f,"</p>\n");
	}
	// sub pages toc
	if ( page == root ) printRootPageToc(f,page);
	else printStandardPageToc(f,page);
	// functions toc
	if ( page->funcs.size() > 1 ) {
		fprintf(f,"<div id=\"toc\"><ul>\n");
		int i=0;
		for ( FuncData **fit=page->funcs.begin(); fit != page->funcs.end(); fit++,i++) {
			FuncData *funcData=*fit;
			if ( funcData->title ) {
				fprintf(f, "<li><a href=\"#%d\">%s</a></li>",
					i,funcData->title);
			}
		}		
		fprintf(f,"</ul></div>\n");
	}
	// functions
	int i=0;
	for ( FuncData **fit=page->funcs.begin(); fit != page->funcs.end(); fit++,i++) {
		FuncData *funcData=*fit;
		// title and description
		fprintf(f,"<a name=\"%d\"></a>",i);
		if (funcData->title) fprintf(f,"<h3>%s</h3>\n",funcData->title);
		if (funcData->desc) {
			fprintf(f,"<p>");
			printHtml(f,funcData->desc);
			fprintf(f,"</p>\n");
		}
		// functions for each language
		fprintf(f,"<div class=\"code\">");
		if (funcData->cpp) {
			fprintf(f,"<p class=\"cpp\">");
			printCppCode(f,funcData->cpp);
			fprintf(f,"</p>\n");
		}
		if (funcData->c) {
			fprintf(f,"<p class=\"c\">");
			printCCode(f,funcData->c);
			fprintf(f,"</p>\n");
		}
		if (funcData->cs) {
			fprintf(f,"<p class=\"cs\">");
			printCSCode(f,funcData->cs);
			fprintf(f,"</p>\n");
		}
		if (funcData->py) {
			fprintf(f,"<p class=\"py\">");
			printPyCode(f,funcData->py);
			fprintf(f,"</p>\n");
		}
		fprintf(f,"</div>\n");
		// parameters table
		if ( !funcData->params.isEmpty()) {
			fprintf(f,"<table class=\"param\"><tbody><tr><th>Parameter</th><th>Description</th></tr>");
			bool hilite=true;
			for ( ParamData **pit = funcData->params.begin(); pit != funcData->params.end(); pit++) {
				if ( hilite ) fprintf(f,"<tr class=\"hilite\">");
				else fprintf(f,"<tr>");
				fprintf(f,"<td>%s</td><td>",(*pit)->name);
				printHtml(f,(*pit)->desc);
				fprintf(f,"</td></tr>\n");
				hilite=!hilite;
			}
			fprintf(f,"</tbody></table>");
		}
		// examples
		if ( funcData->cppEx || funcData->cEx || funcData->pyEx ) {
			fprintf(f,"<h6>Example:</h6><div class=\"code\">\n");
			if (funcData->cppEx) {
				fprintf(f,"<p class=\"cpp\">");
				printCppCode(f,funcData->cppEx);
				fprintf(f,"</p>\n");
			}
			if (funcData->cEx) {
				fprintf(f,"<p class=\"c\">");
				printCCode(f,funcData->cEx);
				fprintf(f,"</p>\n");
			}
			if (funcData->csEx) {
				fprintf(f,"<p class=\"cs\">");
				printCSCode(f,funcData->csEx);
				fprintf(f,"</p>\n");
			}
			if (funcData->pyEx) {
				fprintf(f,"<p class=\"py\">");
				printPyCode(f,funcData->pyEx);
				fprintf(f,"</p>\n");
			}
			fprintf(f,"</div><hr>\n");
		}
	}
	fprintf(f,footer);
	fclose(f);	
}

// export to HTML
void genDoc() {		
	// generates the doc for each page
	for (PageData **it=pages.begin();it!=pages.end();it++) {
		printf ("Generating %s - %s...\n",(*it)->pageNum,(*it)->title);
		genPageDoc(*it);
	}
	genPageDoc(root);
}

// main func
int main(int argc, char *argv[]) {
	TCODList<char *> files=TCODSystem::getDirectoryContent("include", "*.hpp");
	char tmp[128];
	root = new PageData();
	root->name=(char *)"index2";
	root->title=(char *)"Index";
	root->pageNum=(char *)"";
	root->breadCrumb=(char *)"<a href=\"index2.html\">Index</a>";
	root->url=(char *)"doc/index2.html";
	sprintf(tmp,"The Doryen Library v%s - table of contents",TCOD_STRVERSION);
	root->desc=strdup(tmp);
	// parse the *.hpp files
	for ( char **it=files.begin(); it != files.end(); it++) {
		char tmp[128];
		sprintf(tmp,"include/%s",*it);
		parseFile(tmp);
	} 	
	// computations
	buildTree();
	// html export
	genDoc();
}

