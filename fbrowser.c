/*====================================================================*/
/* +ListFiles with double linked list and selection menu in C.
   +Scroll function added.
   Last modified : 22/7/2018
   Coded by Velorek.
   Target OS: Linux.                                                  */
/*====================================================================*/

/*====================================================================*/
/* COMPILER DIRECTIVES AND INCLUDES */
/*====================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
/*====================================================================*/
/* CONSTANTS */
/*====================================================================*/
//Scroll Control values.
#define SCROLL_ACTIVE 1
#define SCROLL_INACTIVE 0
#define CONTINUE_SCROLL -1
#define DOWN_SCROLL 1
#define UP_SCROLL 0
#define SELECT_ITEM 1
#define UNSELECT_ITEM 0
#define CIRCULAR_ACTIVE 1
#define CIRCULAR_INACTIVE 0

// Colors used.                                                                         
#define B_BLACK 40
#define B_WHITE 47
#define B_BLUE 44
#define F_BLACK 30
#define F_WHITE 37
#define F_BLUE 34
#define FH_WHITE 97
#define FILL_CHAR ' '
//Keys used.
#define K_ENTER 10
#define K_ESCAPE 27
#define K_UP_ARROW 'A'		// K_ESCAPE + 'A' -> UP_ARROW
#define K_DOWN_ARROW 'B'	// K_ESCAPE + 'B' -> DOWN_ARROW
//Directories
#define CURRENTDIR "."
#define CHANGEDIR ".."
#define MAX_ITEM_LENGTH 15
#define DIRECTORY 1
#define FILEITEM 0
#define MAX 1024

/*====================================================================*/
/* TYPEDEF STRUCTS DEFINITIONS */
/*====================================================================*/
typedef struct _listchoice {
  unsigned index;		// Item number
  char   *item;			// Item string
  char   *path;			// Item path
  unsigned isDirectory;		// Kind of item
  struct _listchoice *next;	// Pointer to next item
  struct _listchoice *back;	// Pointer to previous item
} LISTCHOICE;

typedef struct _scrolldata {
  unsigned scrollActive;	//To know whether scroll is active or not.
  unsigned scrollLimit;		//Last index for scroll.
  unsigned listLength;		//Total no. of items in the list
  unsigned currentListIndex;	//Pointer to new sublist of items when scrolling.
  unsigned displayLimit;	//No. of elements to be displayed.
  unsigned scrollDirection;	//To keep track of scrolling Direction.
  unsigned selector;		//Y++
  unsigned wherex;		
  unsigned wherey;		
  unsigned backColor0;		//0 unselected; 1 selected
  unsigned foreColor0;
  unsigned backColor1;
  unsigned foreColor1;
  unsigned isDirectory;		// Kind of item
  char   *item;
  char   *path;
  unsigned itemIndex;
} SCROLLDATA;

/*====================================================================*/
/* GLOBAL VARIABLES */
/*====================================================================*/

static struct termios old, new;
LISTCHOICE *listBox1 = NULL;	//Head pointer.

/*====================================================================*/
/* PROTOTYPES OF FUNCTIONS                                            */
/*====================================================================*/

//CONSOLE DISPLAY FUNCTIONS 
void    gotoxy(int x, int y);
void    clear();
void    cleanLine(int line, int backcolor, int forecolor);
void    outputcolor(int foreground, int background);
void    initTermios(int echo);
void    resetTermios(void);
char    getch();
void    draw_window(int x1, int y1, int x2, int y2, int backcolor);

//DYNAMIC LINKED LIST FUNCTIONS
void    deleteList(LISTCHOICE ** head);
LISTCHOICE *addend(LISTCHOICE * head, LISTCHOICE * newp);
LISTCHOICE *newelement(char *text, char *itemPath, unsigned itemType);

//LISTBOX FUNCTIONS
char    listBox(LISTCHOICE * selector, unsigned whereX, unsigned whereY,
		SCROLLDATA * scrollData, unsigned bColor0,
		unsigned fColor0, unsigned bColor1, unsigned fColor1,
		unsigned displayLimit);
void    loadlist(LISTCHOICE * head, SCROLLDATA * scrollData,
		 unsigned indexAt);

void    gotoIndex(LISTCHOICE ** aux, SCROLLDATA * scrollData,
		  unsigned indexAt);
int     query_length(LISTCHOICE ** head);
int     move_selector(LISTCHOICE ** head, SCROLLDATA * scrollData);
char    selectorMenu(LISTCHOICE * aux, SCROLLDATA * scrollData);
void    displayItem(LISTCHOICE * aux, SCROLLDATA * scrollData, int select);

//LISTFILES FUNCTIONS
int     listFiles(LISTCHOICE ** listBox1, char *directory);
int     addSpaces(char temp[MAX_ITEM_LENGTH]);
void    cleanString(char *string, int max);
void    changeDir(SCROLLDATA * scrollData, char fullPath[MAX],
		  char newDir[MAX]);

  /*====================================================================*/
/* CODE */
/*====================================================================*/

/* ------------------------------ */
/* Terminal manipulation routines */
/* ------------------------------ */
void clear() {
  printf("\033[2J\033[1;1H");
}

void gotoxy(int x, int y)
//Sets the cursor at the desired position.
{
  printf("%c[%d;%df", 0x1B, y, x);
}

void outputcolor(int foreground, int background)
//Changes format foreground and background colors of display.
{
  printf("%c[%d;%dm", 0x1b, foreground, background);
}

/* Initialize new terminal i/o settings */
void initTermios(int echo) {
  tcgetattr(0, &old);		/* grab old terminal i/o settings */
  new = old;			/* make new settings same as old settings */
  new.c_lflag &= ~ICANON;	/* disable buffered i/o */
  new.c_lflag &= echo ? ECHO : ~ECHO;	/* set echo mode */
  tcsetattr(0, TCSANOW, &new);	/* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) {
  tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - no echo */
char getch() {
  char    ch;
  initTermios(0);
  ch = getchar();
  resetTermios();
  return ch;
}

//draw window area with shadow

void draw_window(int x1, int y1, int x2, int y2, int backcolor) {
  int     i, j;
  i = x1;
  j = y1;

  //window
  for(j = y1; j <= y2; j++)
    for(i = x1; i <= x2; i++) {
      gotoxy(i, j);
      outputcolor(F_WHITE, backcolor);
      printf("%c", FILL_CHAR);
    }
}

void cleanLine(int line, int backcolor, int forecolor) {
//Cleans line of console.
  int     i;
  for(i = 0; i < 80; i++) {
    //clean line where path is displayed.
    outputcolor(forecolor, backcolor);
    gotoxy(i, line);
    printf("%c", FILL_CHAR);	//space
  }
}

/* --------------------- */
/* Dynamic List routines */
/* --------------------- */

// create new list element of type LISTCHOICE from the supplied text string
LISTCHOICE *newelement(char *text, char *itemPath, unsigned itemType) {
  LISTCHOICE *newp;
  newp = (LISTCHOICE *) malloc(sizeof(LISTCHOICE));
  newp->item = (char *)malloc(strlen(text) + 1);
  newp->path = (char *)malloc(strlen(itemPath) + 1);
  strcpy(newp->item, text);
  strcpy(newp->path, itemPath);
  newp->isDirectory = itemType;
  newp->next = NULL;
  newp->back = NULL;
  return newp;
}

// deleleteList: remove list from memory
/* Function to delete the entire linked list */
void deleteList(LISTCHOICE **head) 
{ 
   /* deref head_ref to get the real head */
   LISTCHOICE *current = *head; 
   LISTCHOICE *next = NULL; 
  
   while (current != NULL)  
   { 
       next = current->next; 
       free(current->item);
       free(current->path);
       free(current);
       current = next; 
   } 
    
   /* deref head_ref to affect the real head back 
      in the caller. */
   *head = NULL; 
} 

/* addend: add new LISTCHOICE to the end of a list  */
/* usage example: listBox1 = (addend(listBox1, newelement("Item")); */
LISTCHOICE *addend(LISTCHOICE * head, LISTCHOICE * newp) {
  LISTCHOICE *p2;
  if(head == NULL) {
    newp->index = 0;
    newp->back = NULL;
    return newp;
  }
// now find the end of list
  for(p2 = head; p2->next != NULL; p2 = p2->next) ;
  p2->next = newp;
  newp->back = p2;
  newp->index = newp->back->index + 1;
  return head;
}

/* ---------------- */
/* Listbox routines */
/* ---------------- */

void gotoIndex(LISTCHOICE ** aux, SCROLLDATA * scrollData,
	       unsigned indexAt)
//Go to a specific location on the list.
{
  LISTCHOICE *aux2;
  unsigned counter = 0;
  *aux = listBox1;
  aux2 = *aux;
  while(counter != indexAt) {
    aux2 = aux2->next;
    counter++;
  }
  //Highlight current item

  displayItem(aux2, scrollData, SELECT_ITEM);

  //Update pointer
  *aux = aux2;
}

void loadlist(LISTCHOICE * head, SCROLLDATA * scrollData, unsigned indexAt) {
/*
Displays the items contained in the list with the properties specified
in scrollData.
*/

  LISTCHOICE *aux;
  unsigned wherey, counter = 0;

  aux = head;
  gotoIndex(&aux, scrollData, indexAt);
  /* Save values */
  //wherex = scrollData->wherex;
  wherey = scrollData->wherey;
  do {
    displayItem(aux, scrollData, UNSELECT_ITEM);
    aux = aux->next;
    counter++;
    scrollData->selector++;	// wherey++
  } while(counter != scrollData->displayLimit);
  scrollData->selector = wherey;	//restore value
}

int query_length(LISTCHOICE ** head) {
//Return no. items in a list.
  {
    LISTCHOICE *aux;

    unsigned itemCount = 0;
    aux = *head;
    while(aux->next != NULL) {
      aux = aux->next;
      itemCount++;
    }
    return itemCount;
  }

}

void displayItem(LISTCHOICE * aux, SCROLLDATA * scrollData, int select)
//Select or unselect item animation
{
  switch (select) {

    case SELECT_ITEM:
      gotoxy(scrollData->wherex, scrollData->selector);
      outputcolor(scrollData->foreColor1, scrollData->backColor1);
      printf("%s\n", aux->item);
      break;

    case UNSELECT_ITEM:
      gotoxy(scrollData->wherex, scrollData->selector);
      outputcolor(scrollData->foreColor0, scrollData->backColor0);
      printf("%s\n", aux->item);
      break;
  }
}
int move_selector(LISTCHOICE ** selector, SCROLLDATA * scrollData) {
/* 
Creates animation by moving a selector highlighting next item and
unselecting previous item
*/

  LISTCHOICE *aux;
  unsigned scrollControl = 0, continueScroll = 0, circular =
      CIRCULAR_INACTIVE;
  //Auxiliary pointer points to selector.
  aux = *selector;

  //Circular list animation when not scrolling.
  if(aux->index == scrollData->listLength - 1
     && scrollData->scrollActive == SCROLL_INACTIVE
     && scrollData->scrollDirection == DOWN_SCROLL) {
    //After last item go back to the top.
    displayItem(aux, scrollData, UNSELECT_ITEM);
    scrollData->selector = scrollData->wherey;
    gotoIndex(&aux, scrollData, 0);
    *selector = aux;
    circular = CIRCULAR_ACTIVE;
  }

  if(aux->index == 0 && scrollData->scrollActive == SCROLL_INACTIVE
     && scrollData->scrollDirection == UP_SCROLL) {
    //Before first item go back to the bottom.
    displayItem(aux, scrollData, UNSELECT_ITEM);
    scrollData->selector = scrollData->wherey + scrollData->listLength - 1;
    gotoIndex(&aux, scrollData, scrollData->listLength - 1);
    *selector = aux;
    circular = CIRCULAR_ACTIVE;
  }
  //Check if we do the circular list animation.
  //If active, we skip the code one time.

  if(circular == CIRCULAR_INACTIVE) {

    //Check if we are within boundaries.
    if((aux->next != NULL && scrollData->scrollDirection == DOWN_SCROLL)
       || (aux->back != NULL && scrollData->scrollDirection == UP_SCROLL)) {

      //Unselect previous item
      displayItem(aux, scrollData, UNSELECT_ITEM);

      //Check whether we move UP or Down
      switch (scrollData->scrollDirection) {

	case UP_SCROLL:
	  //Calculate new top index if scroll is active 
	  //otherwise it defaults to 0 (top)
	  if(scrollData->scrollActive == SCROLL_ACTIVE)
	    scrollControl = scrollData->currentListIndex;
	  else
	    scrollControl = 0;

	  //Move selector
	  if(aux->back->index >= scrollControl) {
	    scrollData->selector--;	//whereY--
	    aux = aux->back;	//Go to previous item
	  } else {
	    if(scrollData->scrollActive == SCROLL_ACTIVE)
	      continueScroll = 1;
	    else
	      continueScroll = 0;
	  }
	  break;

	case DOWN_SCROLL:
	  //Calculate bottom index limit if scroll is ACTIVE
	  //Otherwise it defaults to scrollData->ListLength-1

	  if(scrollData->scrollActive == SCROLL_ACTIVE)
	    scrollControl =
		scrollData->currentListIndex + (scrollData->displayLimit -
						1);
	  else
	    scrollControl = scrollData->listLength - 1;

	  //Move selector
	  if(aux->next->index <= scrollControl) {
	    aux = aux->next;	//Go to next item
	    scrollData->selector++;	//whereY++;
	  } else {
	    if(scrollData->scrollActive == SCROLL_ACTIVE)
	      continueScroll = 1;
	    else
	      continueScroll = 0;
	  }
	  break;
      }

      //Metrics
      cleanLine(4, B_BLUE, F_BLUE);
      outputcolor(F_WHITE, B_BLUE);
      gotoxy(6, 3);
      printf("Index:%d/%d|Memory addr:%p", aux->index,
	     scrollData->listLength - 1, aux);
      gotoxy(6, 4);
      printf("Scroll Limit: %d|IsScActive?:%d|Path: %s",
	     scrollControl, scrollData->scrollActive, aux->path);

      //Highlight new item
      displayItem(aux, scrollData, SELECT_ITEM);

      //Update selector pointer
      *selector = aux;
    }
  }
  circular = CIRCULAR_INACTIVE;
  return continueScroll;
}

char selectorMenu(LISTCHOICE * aux, SCROLLDATA * scrollData) {
  char    ch=0;
  unsigned control = 0;
  unsigned continueScroll=0;
  unsigned counter = 0;

  //Go to and select expected item at the beginning

  gotoIndex(&aux, scrollData, scrollData->currentListIndex);

  if(scrollData->scrollDirection == DOWN_SCROLL
     && scrollData->currentListIndex != 0) {
    //If we are going down we'll select the last item 
    //to create a better scrolling transition (animation)
    for(counter = 0; counter < scrollData->displayLimit; counter++) {
      scrollData->scrollDirection = DOWN_SCROLL;
      move_selector(&aux, scrollData);
    }

  } else {
    //Do nothing if we are going up. Selector is always at the top item.
  }

  //It break the loop everytime the boundaries are reached.
  //to reload a new list to show the scroll animation.
  while(control != CONTINUE_SCROLL) {
    ch = getch();

    //if enter key pressed - break loop
    if(ch == K_ENTER)
      control = CONTINUE_SCROLL;	//Break the loop

    //Check arrow keys
    if(ch == K_ESCAPE)		// escape key
    {
      getch();			// read key again for arrow key combinations
      switch (getch()) {
	case K_UP_ARROW:	// escape key + A => arrow key up
	  //Move selector up
	  scrollData->scrollDirection = UP_SCROLL;
	  continueScroll = move_selector(&aux, scrollData);
	  //Break the loop if we are scrolling
	  if(scrollData->scrollActive == SCROLL_ACTIVE
	     && continueScroll == 1) {
	    control = CONTINUE_SCROLL;
	    //Update data
	    scrollData->currentListIndex =
		scrollData->currentListIndex - 1;
	    scrollData->selector = scrollData->wherey;
	    scrollData->itemIndex = aux->index;
	    //Return value
	    ch = control;
	  }
	  break;
	case K_DOWN_ARROW:	// escape key + B => arrow key down
	  //Move selector down
	  scrollData->scrollDirection = DOWN_SCROLL;
	  continueScroll = move_selector(&aux, scrollData);
	  //Break the loop if we are scrolling
	  if(scrollData->scrollActive == SCROLL_ACTIVE
	     && continueScroll == 1) {
	    control = CONTINUE_SCROLL;
	    //Update data  
	    scrollData->currentListIndex =
		scrollData->currentListIndex + 1;
	    scrollData->selector = scrollData->wherey;
	    scrollData->itemIndex = aux->index;
	    scrollData->scrollDirection = DOWN_SCROLL;
	  }
	  //Return value  
	  ch = control;
	  break;
      }
    }
  }
  if(ch == K_ENTER)		// enter key
  {
    //Pass data of last item selected.
    scrollData->item = aux->item;
    scrollData->itemIndex = aux->index;
    scrollData->path = aux->path;
    scrollData->isDirectory = aux->isDirectory;
  }
  return ch;
}

char listBox(LISTCHOICE * head,
	     unsigned whereX, unsigned whereY,
	     SCROLLDATA * scrollData, unsigned bColor0,
	     unsigned fColor0, unsigned bColor1, unsigned fColor1,
	     unsigned displayLimit) {

  unsigned list_length = 0;
  //unsigned currentIndex = 0;
  int     scrollLimit = 0;
  unsigned currentListIndex = 0;
  char    ch=0;
  LISTCHOICE *aux=NULL;

  // Query size of the list
  list_length = query_length(&head) + 1;

  //Save calculations for SCROLL and store DATA
  scrollData->displayLimit = displayLimit;
  scrollLimit = list_length - scrollData->displayLimit;	//Careful with negative integers

  if(scrollLimit < 0)
    scrollData->displayLimit = list_length;	//Failsafe for overboard values

  scrollData->scrollLimit = scrollLimit;
  scrollData->listLength = list_length;
  scrollData->wherex = whereX;
  scrollData->wherey = whereY;
  scrollData->selector = whereY;
  scrollData->backColor0 = bColor0;
  scrollData->backColor1 = bColor1;
  scrollData->foreColor0 = fColor0;
  scrollData->foreColor1 = fColor1;

  //Check whether we have to activate scroll or not 
  //and if we are within bounds. [1,list_length)

  if(list_length > scrollData->displayLimit && scrollLimit > 0
     && displayLimit > 0) {
    //Scroll is possible  

    scrollData->scrollActive = SCROLL_ACTIVE;
    aux = head;

    currentListIndex = 0;	//We listBox1 the scroll at the top index.
    scrollData->currentListIndex = currentListIndex;

    //Scroll loop animation. Finish with ENTER.
    do {
      currentListIndex = scrollData->currentListIndex;
      loadlist(aux, scrollData, currentListIndex);
      gotoIndex(&aux, scrollData, currentListIndex);
      ch = selectorMenu(aux, scrollData);
    } while(ch != K_ENTER);

  } else {
    //Scroll is not possible.
    //Display all the elements and create selector.
    scrollData->scrollActive = SCROLL_INACTIVE;
    scrollData->currentListIndex = 0;
    scrollData->displayLimit = list_length;	//Default to list_length
    loadlist(head, scrollData, 0);
    ch = selectorMenu(head, scrollData);
  }
  return ch;
}

/* ---------------- */
/* List files       */
/* ---------------- */

int addSpaces(char temp[MAX_ITEM_LENGTH]) {
  int     i;
  for(i = strlen(temp); i < MAX_ITEM_LENGTH; i++) {
    strcat(temp, " ");
  }
  return 0;
}

void cleanString(char *string, int max) {
  int     i;
  for(i = 0; i < max; i++) {
    string[i] = ' ';
  }
}
int listFiles(LISTCHOICE ** listBox1, char *directory) {
  DIR    *d=NULL;
  struct dirent *dir=NULL;
  int     i;
  char    temp[MAX_ITEM_LENGTH];
  int     lenDir;		//length of directory

  //Add elements to switch directory at the beginning for convenience.
  strcpy(temp, CURRENTDIR);
  //Add spaces
  addSpaces(temp);
  *listBox1 = addend(*listBox1, newelement(temp, CURRENTDIR, DIRECTORY));	// "."
  strcpy(temp, CHANGEDIR);
  //Add spaces
  addSpaces(temp);
  *listBox1 = addend(*listBox1, newelement(temp, CHANGEDIR, DIRECTORY));	// ".."

  //Start at current directory
  d = opendir(directory);
  //Find directories and add them to list first
  if(d) {
    while((dir = readdir(d)) != NULL) {
      if(dir->d_type == DT_DIR) {

	lenDir = strlen(dir->d_name);

	//Check length of directory
	//Directories are displayed between brackets [directory]
	if(lenDir > MAX_ITEM_LENGTH - 2) {
	  //Directory name is long. CROP
	  cleanString(temp, MAX_ITEM_LENGTH);
	  strcpy(temp, "[");
	  for(i = 1; i < MAX_ITEM_LENGTH - 1; i++) {
	    temp[i] = dir->d_name[i - 1];
	  }
	  temp[MAX_ITEM_LENGTH - 1] = ']';
	} else {
	  //Directory's name is shorter than display
	  //Add spaces to item string.
	  cleanString(temp, MAX_ITEM_LENGTH);
	  strcpy(temp, "[");
	  for(i = 1; i < lenDir + 1; i++) {
	    temp[i] = dir->d_name[i - 1];
	  }
	  temp[lenDir + 1] = ']';
	  addSpaces(temp);
	}
	//Add all directories except CURRENTDIR and CHANGEDIR
	if(strcmp(dir->d_name, CURRENTDIR) != 0
	   && strcmp(dir->d_name, CHANGEDIR) != 0)
	  *listBox1 =
	      addend(*listBox1, newelement(temp, dir->d_name, DIRECTORY));
      }
    }
  }
  closedir(d);

  //Find files and add them to list after directories
  d = opendir(directory);
  if(d) {
    while((dir = readdir(d)) != NULL) {
      if(dir->d_type == DT_REG) {
	//only list valid fiels
	if(strlen(dir->d_name) > MAX_ITEM_LENGTH) {
	  for(i = 0; i < MAX_ITEM_LENGTH; i++) {
	    temp[i] = dir->d_name[i];
	  }
	} else {
	  strcpy(temp, dir->d_name);
	  //Add spaces
	  addSpaces(temp);
	}
	*listBox1 =
	    addend(*listBox1, newelement(temp, dir->d_name, FILEITEM));
      }
    }
    closedir(d);
  }
  return 0;
}

void changeDir(SCROLLDATA * scrollData, char fullPath[MAX],
	       char newDir[MAX]) {
//Change dir
  char    oldPath[MAX];
  if(scrollData->isDirectory == DIRECTORY) {
    if(scrollData->itemIndex == 1) {
      //cd ..
      cleanString(fullPath, MAX);
      cleanString(oldPath, MAX);
      cleanString(newDir, MAX);
      chdir("..");
      getcwd(oldPath, sizeof(oldPath));
      strcpy(newDir, oldPath);
      strcpy(fullPath, oldPath);
    } else {
      //cd newDir
      cleanString(fullPath, MAX);
      cleanString(newDir, MAX);
      cleanString(oldPath, MAX);
      getcwd(oldPath, sizeof(oldPath));
      strcat(oldPath, "/");
      strcat(oldPath, scrollData->path);
      chdir(oldPath);
      strcpy(newDir, oldPath);
      strcpy(fullPath, oldPath);
    }
  }
}

/* ---------------- */
/* Main             */
/* ---------------- */

/*========================================================================*/
/*  
  ListBox with Scroll: 
  ____________________
  
  Usage:
   
  listBox(headpointer, whereX, whereY, scrollData, backColor0, foreColor0,
backcolor1, forecolor1, displayLimit); */

/*========================================================================*/

int main() {
  SCROLLDATA scrollData;
  char    ch;
  char    fullPath[MAX];
  char    newDir[MAX];
  //Change background color
  outputcolor(F_WHITE, B_BLUE);
  clear();

  strcpy(newDir, ".");		//We start at current dir
  getcwd(fullPath, sizeof(fullPath));	//Get path
  scrollData.scrollActive=0;	//To know whether scroll is active or not.
  scrollData.scrollLimit=0;		//Last index for scroll.
  scrollData.listLength=0;		//Total no. of items in the list
  scrollData.currentListIndex=0;	//Pointer to new sublist of items when scrolling.
  scrollData.displayLimit=0;	//No. of elements to be displayed.
  scrollData.scrollDirection=0;	//To keep track of scrolling Direction.
  scrollData.selector=0;		//Y++
  scrollData.wherex=0;		
  scrollData.wherey=0;		
  scrollData.backColor0=0;		//0 unselected; 1 selected
  scrollData.foreColor0=0;
  scrollData.backColor1=0;
  scrollData.foreColor1=0;
  scrollData.isDirectory=0;		// Kind of item
  scrollData.item =NULL;
  scrollData.path =NULL;
  scrollData.itemIndex=0;
  //LISTCHOICE *head;		//store head of the list

  //Directories loop
  do {
    draw_window(9, 7, 31, 19, B_BLACK);	//shadow
    draw_window(8, 6, 30, 18, B_WHITE);	//window

    //Add items to list
    if(listBox1 == NULL) 
      listFiles(&listBox1, newDir);
    ch = listBox(listBox1, 10, 7, &scrollData, B_WHITE, F_BLACK, B_BLUE,
		 FH_WHITE, 10);

    //Change Dir. New directory is copied in newDir
    if (scrollData.itemIndex!=0) changeDir(&scrollData, fullPath, newDir);

    //Display current path
    cleanLine(22, B_BLUE, F_BLUE);
    outputcolor(F_WHITE, B_BLUE);
    gotoxy(1, 22);
    printf("Current Path: %s", fullPath);

    //Info Item selected.
    cleanLine(21, B_BLUE, F_BLUE);
    gotoxy(1, 21);
    outputcolor(FH_WHITE, B_BLUE);
    printf("Item selected: %s | Index: %d | Key : %d\n",
	   scrollData.path, scrollData.itemIndex, ch);

    if(listBox1 != NULL) {
		deleteList(&listBox1);
		listBox1 = NULL;
    }
  } while(scrollData.itemIndex != 0);
 //Restore colors.
  outputcolor(F_WHITE, B_BLACK);
  clear();
  printf("\n");
  return 0;

}
