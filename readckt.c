/*=======================================================================
  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 IPT    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines




                                    Author: Chihang Chen
                                    Date: 9/16/94

=======================================================================*/

/*=======================================================================
  - Write your program as a subroutine under main().
    The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
int cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#define MAXLINE 10000              /* Input buffer size */
#define MAXNAME 5000               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT, LOGICSIM, RFL, DFS, PFS};  // Command list. removed lev
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */ // Node type.
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */

struct cmdstruc {
   char name[MAXNAME];        /* command syntax */
   int (*fptr)();             /* function pointer of the commands */
   enum e_state state;        /* execution state sequence */
};

struct fault{
   int node_num;
   int fault_type;
};


int W = (int)(CHAR_BIT * sizeof(void *));

typedef struct n_struc {
   unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
   unsigned num;              /* line number(May be different from indx */
   enum e_gtype type;         /* gate type */
   unsigned fin;              /* number of fanins */
   unsigned fout;             /* number of fanouts */
   struct n_struc **unodes;   /* pointer to array of up nodes */
   struct n_struc **dnodes;   /* pointer to array of down nodes */
   int level;                 /* level of the gate output */
   int logical_value;         /* For storing the logical value of every node.*/
   int fault_list[(int)(CHAR_BIT * sizeof(void *))];
   //dfs
   int num_fl[10000];
   int type_fl[10000];
   int count_fl;
   //
} NSTRUC;  





/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 9
int cread(), pc(), help(), quit(), logicsim(), rfl(), dfs(), pfs(), rtg();
int all_levels_assigned(), all_logics_aasigned(), get_max_level();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   //{"LEV", lev, CKTLD},
   {"LOGICSIM", logicsim, CKTLD },
   {"RFL",rfl, CKTLD },
   {"DFS", dfs, CKTLD },
   {"PFS", pfs, CKTLD },
   {"RTG",rtg,CKTLD}
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */
/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
//OUTPUT FILE COMPONENTS:
char print_file_name[MAXLINE];
char output_file_name[MAXLINE];
char *user_file_input[MAXLINE];
int final_gate_count = 0;


//cread():
char current_read_file[MAXLINE];
//logicsim():
char input_file_logicsim[MAXLINE];
char output_file_logicsim[MAXLINE];
//dfs():
char input_file_dfs[MAXLINE];
char output_file_dfs[MAXLINE];
// No. of nodes is Nnodes(declared on line 102).
// No. of primary inputs is Npi(declared on line 103).
// No. of primary outputs is Npo(declared on line 104).

//pfs():
//struct fault map[iterations];
//struct fault current_input_vector[Npi];// Store PI node_num and its value.


/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
int temp_max_level,final_max,found_uninitialized_input=0; // Used in lev->Gate loop.


/*------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
  This is the main program of the simulator. It displays the prompt, reads
  and parses the user command, and calls the corresponding routines.
  Commands not reconized by the parser are passed along to the shell.
  The command is executed according to some pre-determined sequence.
  For example, we have to read in the circuit description file before any
  action commands.  The code uses "Gstate" to check the execution
  sequence.
  Pointers to functions are used to make function calls which makes the
  code short and clean.
-----------------------------------------------------------------------*/
int main()
{
   enum e_com com;
   char cline[MAXLINE], wstr[MAXLINE], *cp;
   int i,j,k; // For iteration.
   
   while(!Done) {
      
      printf("\nCommand>");
      fgets(cline, MAXLINE, stdin);
      final_gate_count = 0; // reset final_gate_count.
      i=j=k=0;
      if((cline[0] == 'r') || (cline[0] == 'R')){ // If its a read command.
            //printf("Entered file copy\n");
            while(cline[i] != ' ') i+=1;
            j = i + 1;
   
            while(cline[j] != '\n')
            {
               print_file_name[k] = cline[j];
               //printf("%c\n",cline[j]);
               j+=1;
               k+=1;
            }

            //printf("print_file_name in main: %s\n",print_file_name);
            //strcpy(output_file_name,print_file_name);
            //strcat(output_file_name,"_golden_output_vikrant.log");
            //printf("output_file_name in main: %s\n",output_file_name);
      }
      
      if(sscanf(cline, "%s", wstr) != 1) continue;
      cp = wstr;

      while(*cp){ // Converts the input command to Uppercase.
	*cp= Upcase(*cp);
	cp++;
      }

      cp = cline + strlen(wstr); // What is this for?
      com = READ;
      while(com < NUMFUNCS && strcmp(wstr, command[com].name)) com++;
      if(com < NUMFUNCS) {
         if(command[com].state <= Gstate) (*command[com].fptr)(cp);
         else printf("Execution out of sequence!\n");
      }
      else system(cline); // Executes the listed command.
   }

   return 0;
}

/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
  This routine reads in the circuit description file and set up all the
  required data structure. It first checks if the file exists, then it
  sets up a mapping table, determines the number of nodes, PI's and PO's,
  allocates dynamic data arrays, and fills in the structural information
  of the circuit. In the ISCAS circuit description format, only upstream
  nodes are specified. Downstream nodes are implied. However, to facilitate
  forward implication, they are also built up in the data structure.
  To have the maximal flexibility, three passes through the circuit file
  are required: the first pass to determine the size of the mapping table
  , the second to fill in the mapping table, and the third to actually
  set up the circuit information. These procedures may be simplified in
  the future.
-----------------------------------------------------------------------*/
cread(cp)
char *cp;
{
   char output_lev_file_name[1000]; //used to call lev function with output name
   int a; // used for output_lev_name
   char buf[MAXLINE];
   int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
   FILE *fd;
   NSTRUC *np;
   
   sscanf(cp, "%s", buf);
   for(a=0;a<strlen(buf)-4;a++) 
   {
      current_read_file[a] = buf[a];
      output_lev_file_name[a]=buf[a];
   }
   strcat(output_lev_file_name,"_lev.txt");
   //printf("%s",output_lev_file_name);
   if((fd = fopen(buf,"r")) == NULL) {
      printf("File %s does not exist!\n", buf);
      return;
   }

   if(Gstate >= CKTLD) clear();

   // Restting all data and file name variables:
   Nnodes = Npi = Npo = ntbl = 0;
   final_gate_count = 0;
   
   while(fgets(buf, MAXLINE, fd) != NULL) // This is the first pass.
   {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) // sscanf: On success, the function returns the number of variables filled.
      {
         //printf("tp, nd: %d , %d\n",tp,nd);
         //if((tp == GATE) || (tp == PO)) final_gate_count +=1;
         //if((tp == 0) || (tp == 3)) final_gate_count +=1;
         // tp is basically column1 and nd is basically column2.
         if(ntbl < nd) ntbl = nd;
         Nnodes ++;
         if(tp == PI) Npi++; // How can we compare an integer(tp) to an enum type?
         else if(tp == PO) Npo++;
      }
   }

   //printf("Total no. of nodes: %d\n",Nnodes);
   //printf("Total no. of primary inputs: %d\n",Npi);
   //printf("Total no. of primary outputs: %d\n",Npo);
   //printf("final_gate_count: %d\n",final_gate_count);

   tbl = (int *) malloc(++ntbl * sizeof(int));

   fseek(fd, 0L, 0); // Going back to top of file. Basically, this is the second pass.
   i = 0;
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
      // printf("tbl[%d]: %d\n",nd,tbl[nd]);
      // tp is basically column1 and nd is basically column2.
      // In the sscanf we basically give every node a unique number from 0 - (Nnodes-1).
   }
   allocate();
   

   fseek(fd, 0L, 0); // This is the third pass. Now we actually set-up the circuit.
   
   while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
      //printf("tp: %d   nd: %d\n",tp,nd);
      np = &Node[tbl[nd]];
      np->num = nd;
      if(tp == PI) Pinput[ni++] = np;
      else if(tp == PO) Poutput[no++] = np;

      switch(tp) 
      {
            case PI:
            case PO:
            case GATE:
               fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
               np->logical_value = -1;
               //printf("Inside gate tp: %d    nd: %d\n",tp,nd);
               //printf("type: %d  fout: %u   fin: %u\n\n\n",np->type,np->fout,np->fin);
               break;
            
            case FB:
               //tp == 2
               //printf("Inside FB tp: %d\n",tp);
               np->fout = np->fin = 1; // Why are fin and fout same? And why are they 1?
               fscanf(fd, "%d", &np->type);
               np->logical_value = -1;
               break;

            default:
               //printf("tp: %d   nd: %d\n",tp,nd);
               printf(" Unknown node type!\n");
               exit(-1);
      }


      np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
      np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
      //printf("\ncurrent fin: %u \n",np->fin);
      for(i = 0; i < np->fin; i++) 
      {
         fscanf(fd, "%d", &nd);
         np->unodes[i] = &Node[tbl[nd]];
         
      }

      for(i = 0; i < np->fout; np->dnodes[i++] = NULL);
      
      }


      for(i = 0; i < Nnodes; i++) {
         for(j = 0; j < Node[i].fin; j++) {
            np = Node[i].unodes[j];
            k = 0;
            while(np->dnodes[k] != NULL) k++;
            np->dnodes[k] = &Node[i];
            }
         }

      fclose(fd);
      Gstate = CKTLD;
      printf("==> OK\n");
      lev(output_lev_file_name);
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
  The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
pc(cp)
char *cp;
{
   int i, j;
   NSTRUC *np;
   char *gname();

   // Original print labels:
   printf(" Node   Type \tIn     \t\t\tOut    \n");
   printf("------ ------\t-------\t\t\t-------\n");

   // Test labels:
   //printf(" Node   Type \tIn     \t\t\tOut         logical_value\n");
   //printf("------ ------\t-------\t\t\t-------     -------------\n");
   for(i = 0; i<Nnodes; i++) {
      np = &Node[i];
      printf("\t\t\t\t\t");
      for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
      //printf("                      %d",np->logical_value); // Added for testing.
      printf("\r%5d  %s\t", np->num, gname(np->type));     // Previously \r%5d
      for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
      printf("\n");
   }
   printf("Primary inputs:  ");
   for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
   printf("\n");
   printf("Primary outputs: ");
   for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
   printf("\n\n");
   printf("Number of nodes = %d\n", Nnodes);
   printf("Number of primary inputs = %d\n", Npi);
   printf("Number of primary outputs = %d\n", Npo);
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
help()
{
   printf("READ filename - ");
   printf("read in circuit file and creat all data structures\n");
   printf("PC - ");
   printf("print circuit information\n");
   printf("HELP - ");
   printf("print this help information\n");
   //printf("LEV filename - ");
   //printf(" Performs levelization of the circuit that was read and outputs the circuit data into the given file\n");
   printf("LOGICSIM input_filename output_filename - ");
   printf("Performs the logic simulation of the given circuit and generates the output vector\n");
   printf("RFL output_filename - ");
   printf(" Generates a reduced list of faults required for the ATPG of the circuit\n");
   printf("DFS filename - ");
   printf("Reports all the detectable (so not just the RFL) faults using the input test patterns\n");
   printf("PFS filename - ");
   printf("Reports which one of the faults can be detected with the input test patterns using the PFS method\n");
   printf("RTG #random_test_patterns  report_frequency   test_pattern_report_filename   FC_report_filename-\n"); //UPDATE THIS.
   printf("QUIT - ");
   printf("stop and exit\n");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
quit()
{
   Done = 1;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
Function: all_levels_assigned
input: Name of output file
output: If all the levels have been assigned a level successfully.
-----------------------------------------------------------------------*/
all_levels_assigned()
{
   NSTRUC *np;
   int i;
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      if(np->level == -1) return 0;
   }

   return 1;
}

/*-----------------------------------------------------------------------
Function: get_node
input: num value of a node
output: Returns the node that has the given num value.
-----------------------------------------------------------------------*/
NSTRUC* get_node(int val)
{
   int i;
   NSTRUC *np;
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      if(np->num == val) return np;
   }

   return NULL; // If that node doesn't exist.
}

/*-----------------------------------------------------------------------
Function: print_level
input: nothing
output: nothing
-----------------------------------------------------------------------*/
void print_level()
{
   NSTRUC *np;
   int i;
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      printf("%d ",np->level);
   }

}
/*-----------------------------------------------------------------------
Function: LEV

input: Name of output file
output: Nothing
-----------------------------------------------------------------------*/
lev(cp)
char *cp;
{
   /*
   enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format  // Node type.
   enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types 
   */
   //char cline[MAXLINE];
   //fgets(cline, MAXLINE, stdin);
   //printf("Inside lev\n\n");
   int i,j,k,assigned_all=0,loop_count=1;
   NSTRUC *np;
   NSTRUC *temp;
   temp = np;
   char *gname();
   FILE *file_ptr;
   sscanf(cp, "%s", output_file_name);
   //printf("Initial level array:\n");
   for(i=0;i<Nnodes;i++) // Initialized level of every node to 0;
   {
      np = &Node[i];
      np->level = -1;

      // If node type is not PI and not BRANCH, its a gate.
      if((np->type != 0) && (np->type != 1)) final_gate_count+=1; 
      //printf("%d ",np->level);
   }
   
   // LEVELIZATION STARTS HERE:

   while(!(assigned_all))
      {
         for(i=0;i<Nnodes;i++) 
         {
            np = &Node[i];
            //printf("%s        %d \n",gname(np->type),np->num);
            if(gname(np->type) == "PI") 
            {
               //printf("Node %d is a PI\n",np->num);
               np->level = 0;
               //printf("Node #: %d is level %d\n",np->num,np->level);
            }

            else if(gname(np->type) == "BRANCH")
            {
               temp = get_node(np->unodes[0]->num);
               //printf("Stem of BRANCH %d is node: %d\n",np->num,temp->num);
               if(temp->level != -1) 
               {
                  np->level = temp->level + 1;
                  //printf("Level of BRANCH %d is:%d \n\n\n",np->num,np->level);
               }

               else{
                  //printf("Level of stem is still -1\n\n\n");
               }

            }

            else  // For all gates including PO.
            {
               temp_max_level = np->unodes[0]->level; // Every gate will have atleast one input so this statement will never be illegal.

               if(temp_max_level == -1) found_uninitialized_input = 1;

               else // Even if 1 input is uninitialized, we don't have to enter here.
               {
                  for(j=0;j<np->fin;j++)
                  {
                     if((np->unodes[j]->level > temp_max_level) && (np->unodes[j]->level != -1) && (temp_max_level != -1)) 
                        temp_max_level = np->unodes[j]->level ;

                     else if((np->unodes[j]->level == -1) || (temp_max_level == -1))  found_uninitialized_input = 1;
                  }
               }


               if(found_uninitialized_input == 0) 
               {
                  np->level = temp_max_level + 1;
                  //printf("Level of GateNode %d is: %d\n\n\n",np->num,np->level);
               }

               else found_uninitialized_input = 0; // Re-setting variable for next gate.
            }
         
            //loop_count +=1;
            
            /*if(loop_count > 6294)
            {
               printf("LEVELIZTION OPERATED 6294 LINES. EXITING\n");
               exit(-1);
            }*/
         }
         assigned_all = all_levels_assigned();
         //print_level();
         //printf("\n");
         //printf("assigned_all: %d\n",assigned_all);
      }

      loop_count = 1;
      //assigned_all = 0;


   // FILE WRITING STARTS HERE:
   //printf("Output file name: %s\n",output_file_name);
   //printf("print_file_name: %s\n",print_file_name);
   file_ptr = fopen(output_file_name,"w");
   fprintf(file_ptr,"%s",print_file_name);
   fprintf(file_ptr,"\n#PI: %d",Npi);
   fprintf(file_ptr,"\n#PO: %d",Npo);
   fprintf(file_ptr,"\n#Nodes: %d",Nnodes);
   fprintf(file_ptr,"\n#Gates: %d",final_gate_count);
   
   // Nnodes, Npi, Npo, final_gate_count are all reset to 0 inside cread().

  

   for(k=0;k<Nnodes;k++)
   {
      np = &Node[k];
      fprintf(file_ptr,"\n%d %d",np->num,np->level);
   }

   fclose(file_ptr);
   //printf("File write complete\n");
   memset(print_file_name,0,sizeof(print_file_name));
   // Re-setting file variables:
   //strcpy(print_file_name,"");
   //strcpy(output_file_name,"");

}

all_logics_assigned()
{
   NSTRUC *np;
   int i;
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      if(np->logical_value == -1) return 0;
   }

   return 1;
}


/*all_input_logics_assigned()
{
   NSTRUC *np;
   int i;
   for(i=0;i<Npi;i++)
   {
      np = &Pinput[i];
      if(np->logical_value == -1) return 0;
   }

   return 1;
}*/

get_max_level()
{
   NSTRUC *np;
   int i,max_level=0;
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      if(np->level > max_level) max_level = np->level;
   }
   return max_level;
}
/*-----------------------------------------------------------------------
Function: LOGICSIM

input: Name of input and output files.
output: Nothing
-----------------------------------------------------------------------*/

logicsim(ip)
char *ip;
{
   sscanf(ip, "%s %s", input_file_logicsim,output_file_logicsim);
   //printf("Entered LOGICSIM\n");
   //printf("input file: %s\n",input_file_logicsim);
   //printf("output file: %s\n",output_file_logicsim);
   char line[2000];
   int i,j,k,current_level,node_number,logic_val;
   NSTRUC *np,tmp;
   int max_level = get_max_level();
   
   FILE *file_ptr;
   file_ptr = fopen(input_file_logicsim,"r");
   FILE *file_ptr2;
   file_ptr2 = fopen(output_file_logicsim,"w");
   char *pt;
   const char *comp_x = "x";
   const char *comp_X = "X";
   int first_line =0;
   int node_no[Npi];
   char logicvalue[10000][Npi];
   for(i=0; i<1000; i++) memset(logicvalue[i],0,strlen(logicvalue[i]));
   int a=0, b=0; //a is # input patterns, b used internally which is basically Npi
   int x,y,c;

   //printf("Npi=%d\n",Npi);
   // Assigning logical value to the Primary inputs:
   while(!feof(file_ptr))
   {
      memset(line,0,strlen(line));
      fgets(line,10000,file_ptr);
      //printf("%d\n", strlen(line));
      //printf("%s",line);
      if(line[strlen(line)-1]=='\n') line[strlen(line)-1]='\0';
      if(strlen(line)==0) break;
      //printf("%s",line);
      if(first_line==0){
         i=0;
         pt = strtok (line,",");
         while (pt != NULL) {
            node_no[i] = atoi(pt);
            //printf("%d", node_no[i]);
            pt = strtok (NULL, ",");
            i++;
         }
         first_line++;
         for(c=0; c<Npo;c++){
            if(c!=Npo-1) fprintf(file_ptr2,"%d,",Poutput[c]->num);
            else fprintf(file_ptr2,"%d",Poutput[c]->num);
         }
      }
      else{
         pt = strtok (line,",");
         while (pt != NULL) {
            //printf("%s",pt);
            if((strcmp(pt, comp_x)==0)||(strcmp(pt, comp_X)==0)){ 
            logicvalue[a][b] = -1;   
            //printf("%d", logicvalue[a][b]);
            }
            else{
                logicvalue[a][b] = atoi(pt);
            }    
            //printf("%d", logicvalue[a][b]);
            pt = strtok (NULL, ",");
            //printf("%s",pt);
            b++;
         }
         b=0;
         //printf("\n");
         a++;
      } 
      //sscanf(line,"%d,%d",&node_number,&logic_val);
      // Can we reduce time complexity by using Pinput[] and Npi?
       
   }
   fclose(file_ptr);
   //int x=0;
   for(x=0;x<(a);x++){
       for(y=0;y<Npi;y++) 
       {
         //printf("current node: %d\n",Pinput[i]->num);
          if(Pinput[y]->num == node_no[y])
          { Pinput[y]->logical_value = logicvalue[x][y];
            //printf("node-%d assigned value: %d\n",Pinput[i]->num,Pinput[i]->logical_value);
            //x++;
          }
       }      
   //printf("x=%d",x);
   /*int c,d;
      for( c=0;c<a-1;c++){
         for( d=0;d<Npi;d++) printf("%d",logicvalue[c][d]);
         printf("\n");
      }*/

   //----------------PIs have been assigned their logical value-------------------------

   //printf("Logic value assigned to all PIs\n");
   //printf("PI logic values: \n");
   //for(i=0;i<Npi;i++)
   //{
      
      //printf("node:%d   val:%d\n",Pinput[i]->num,Pinput[i]->logical_value);
   //}
   

   current_level = 1; // We have already assigned logical values to all the nodes in level-0 
   //                0     1    2   3    4    5    6     7
   // enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */
   //while(!all_logics_assigned() && current_level<=max_level)
   while(current_level<=max_level)
   {
      //printf("\n\nCurrent level: %d\n",current_level);
      for(i=0;i<Nnodes;i++)
      {
         np = &Node[i];
         if((np->level== current_level) && (np->type!=0)) // We assign values level-by-level. 
         {
            if(np->type == 1) // BRANCH: We assign it the value of its parent branch.
            { 
               //printf("Entered 1\n");
               np->logical_value = np->unodes[0]->logical_value; // Branches will always have just 1 upnode.
               //printf("node-%d is a Branch. value assigned: %d\n\n",np->num,np->logical_value);
            }
            


            if(np->type == 2) // XOR
            {
               //printf("Entered 2\n");
               np->logical_value = 0; // 0^x = x; so we initialize the xor result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value == -1)
                  {
                     np->logical_value=-1;
                     break;
                  }
                  else
                  {
                     if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value ^ np->unodes[j]->logical_value;
                     else np->logical_value = -1;
                  }
                  //if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value ^ np->unodes[j]->logical_value;
                  //else np->logical_value = -1;
               }
               //printf("node-%d is an XOR gate. value assigned: %d\n\n",np->num,np->logical_value);
            }

            if(np->type == 3) // OR
            {
               //printf("Entered 3\n");
               np->logical_value = 0; // 0|x = x; so we initialize the OR result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1)
                  { 
                     if(np->logical_value != -1) np->logical_value = np->logical_value | np->unodes[j]->logical_value;
                     else
                     {
                        if(np->unodes[j]->logical_value == 0) np->logical_value = -1;
                        if(np->unodes[j]->logical_value == 1) np->logical_value = 1;  
                     }
                  }   
                  else
                  { 
                     if(np->logical_value == 0) np->logical_value = -1;
                     if(np->logical_value == 1) np->logical_value = 1;
                  }   
               }
               //printf("node-%d is an OR gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 4) // NOR
            {
               //printf("Entered 4\n");
               np->logical_value = 0; // 0|x = x; so we initialize the OR result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1)
                  { 
                     if(np->logical_value != -1) np->logical_value = np->logical_value | np->unodes[j]->logical_value;
                     else
                     {
                        if(np->unodes[j]->logical_value == 0) np->logical_value = -1;
                        if(np->unodes[j]->logical_value == 1) np->logical_value = 1;  
                     }
                  }   
                  else
                  { 
                     if(np->logical_value == 0) np->logical_value = -1;
                     if(np->logical_value == 1) np->logical_value = 1;
                  }   
               }
               if(np->logical_value != -1) np->logical_value = 1-np->logical_value; // Invert once for NOR.
               //printf("node-%d is a NOR gate. value assigned: %d\n\n",np->num,np->logical_value);
            } 



            if(np->type == 5) // NOT
            {
               //printf("Entered 5\n");
               //printf("unode:%d    unode val:%d\n",np->unodes[0]->num,np->unodes[0]->logical_value);
               if(np->unodes[0]->logical_value == -1) np->logical_value = -1;
               else np->logical_value = 1 - np->unodes[0]->logical_value; // Invert.
               //printf("node-%d is a NOT gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 6) // NAND
            {
               //printf("Entered 6\n");
               np->logical_value = 1; // 1&x = x; so we initialize the NAND result to 1.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1)
                  { 
                     if(np->logical_value != -1) np->logical_value = np->logical_value & np->unodes[j]->logical_value;
                     else
                     {
                        if(np->unodes[j]->logical_value == 0) np->logical_value = 0;
                        if(np->unodes[j]->logical_value == 1) np->logical_value = -1;  
                     }
                  }   
                  else
                  { 
                     if(np->logical_value == 0) np->logical_value = 0;
                     if(np->logical_value == 1) np->logical_value = -1;
                  }   
               }
               if(np->logical_value != -1) np->logical_value = 1-np->logical_value; // Invert once for NAND.
               //printf("node-%d is a NAND gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 7) //AND
            {
               //printf("Entered 7\n");
               np->logical_value = 1; // 1&x = x; so we initialize the NAND result to 1.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1)
                  { 
                     if(np->logical_value != -1) np->logical_value = np->logical_value & np->unodes[j]->logical_value;
                     else
                     {
                        if(np->unodes[j]->logical_value == 0) np->logical_value = 0;
                        if(np->unodes[j]->logical_value == 1) np->logical_value = -1;  
                     }
                  }   
                  else
                  { 
                     if(np->logical_value == 0) np->logical_value = 0;
                     if(np->logical_value == 1) np->logical_value = -1;
                  }   
               }
               //printf("node-%d is an AND gate. value assigned: %d\n\n",np->num,np->logical_value);
            }
         }

         else
         {
            //printf("not entered for:: node:%d  level:%d\n",np->num,np->level);
         }
      }
      current_level+=1;
   }
   // At this point all the nodes have been assigned a logical value and we have traversed
   // all the levels. Hence, we now write the output logic values to the output files.
   
   
   for(j=0;j<Npo;j++)
   {
      if(Poutput[j]->logical_value != -1){
      //printf("%d=%d\n",Poutput[j]->num,Poutput[j]->logical_value);
      if(j==0) fprintf(file_ptr2,"\n%d,",Poutput[j]->logical_value);
      else if(j!=Npo-1) fprintf(file_ptr2,"%d,",Poutput[j]->logical_value);
      else fprintf(file_ptr2,"%d",Poutput[j]->logical_value);
      }
      else{
         if(j==0) fprintf(file_ptr2,"\nx,");
      else if(j!=Npo-1) fprintf(file_ptr2,"x,");
      else fprintf(file_ptr2,"x"); 
      }
   }
   //printf("\n\n\nLogical value assigned to all nodes\n");
   //printf("Logicsim finished\n");
   }  
   fclose(file_ptr2);   
}
/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine clears the memory space occupied by the previous circuit
  before reading in new one. It frees up the dynamic arrays Node.unodes,
  Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.
-----------------------------------------------------------------------*/
clear()
{
   int i;

   for(i = 0; i<Nnodes; i++) {
      free(Node[i].unodes);
      free(Node[i].dnodes);
   }
   free(Node);
   free(Pinput);
   free(Poutput);
   Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine allocatess the memory space required by the circuit
  description data structure. It allocates the dynamic arrays Node,
  Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
  tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
allocate()
{
   int i;

   Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));
   Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
   Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
   for(i = 0; i<Nnodes; i++) {
      Node[i].indx = i;
      Node[i].fin = Node[i].fout = 0;
   }
}

//rfl:
rfl(cp)
char *cp;
{
   FILE *f_ptr;
   char output_filename[MAXLINE];
   char line[MAXLINE];
   char current_fault[MAXLINE];
   char temp[MAXLINE];
   int i;
   NSTRUC *np;
   sscanf(cp, "%s", output_filename);
   f_ptr = fopen(output_filename,"w");

   
   for(i=0;i<Npi;i++)
   {
      sprintf(temp,"%d",Pinput[i]->num);
      strcat(temp,"@0");
      fprintf(f_ptr,"%s\n",temp);
      strcpy(temp,"");
      sprintf(temp,"%d",Pinput[i]->num);
      strcat(temp,"@1");
      fprintf(f_ptr,"%s\n",temp);
   }

   strcpy(temp,""); // Re-initialize temp.
   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      if(np->type == 1)
      {
         sprintf(temp,"%d",np->num);
         strcat(temp,"@0");
         fprintf(f_ptr,"%s\n",temp);
         strcpy(temp,"");
         sprintf(temp,"%d",np->num);
         strcat(temp,"@1");
         fprintf(f_ptr,"%s\n",temp);
      }
      
   }
   //printf("All RFL faults updated\n");
   fclose(f_ptr);
   
}
/*
int get_lines(char *filename): Returns the number of lines in a file.
*/
 int get_lines(char *filename)
 {
    FILE *f;
    int number_of_lines = 0;
    char input_filename[MAXLINE];
    sscanf(filename, "%s", input_filename);
    f = fopen(input_filename,"r");
    int ch;

    while (EOF != (ch=getc(f)))
    {
        if ('\n' == ch) ++number_of_lines;
    }

    return number_of_lines;
 }


/*
void reset_logic_values()
{
   NSTRUC *np;
   int i;

   for(i=0;i<Nnodes;i++)
   {
      np = &Node[i];
      np->logical_value = -1;
      np->fault[0] = -1;
   }
}



int logicsim_single_pass(int pinputs[Npi])
{
   reset_logic_values();
   //LOGICSIM SINGLE PASS LOGIC:
   Also set np->fault[0] = np->logical_val;
}

*/
/*struct union_op(struct dfs_faultlist a, int dfs_indx1, struct dfs_faultlist b, int dfs_indx2)
{
    int i = 0, j = 0, k=0;
    struct dfs_faultlist uni;

    while (i < dfs_indx1 && j < dfs_indx2) {
        if (a.num[i] < b.num[j]){
          uni.num[k++]=a.num[i++];
          uni.type[k++]=a.type[i++];  
          //printf(" %d ", a.num[i++]);  
        } 
        else if (b.num[j] < a.num[i]){ 
            uni.num[k++]=b.num[i++];
            uni.type[k++]=b.type[i++];
            //printf(" %d ", b.num[j++]);
         }   
        else {
            uni.num[k++]=b.num[i++];
            uni.type[k++]=b.type[i++];
            //printf(" %d ", b.num[j++]);
            i++;
        }
    }
 
    
    while (i < dfs_indx1){
        uni.num[k++]=a.num[i++];
        uni.type[k++]=a.type[i++];
        //printf(" %d ", a.num[i++]);
      }  
    while (j < dfs_indx2){
        uni.num[k++]=b.num[i++];
        uni.type[k++]=b.type[i++];
        //printf(" %d ", b.num[j++]);
      }  
}*/
int removerepeated(NSTRUC *np1);
void sort(NSTRUC *np1);
void union_op(NSTRUC *np1, NSTRUC *np2) //np1 is gate output
{
    int i,size1,size2,size,j=0,k;
    int a[size1],b[size2],uni[size1+size2];
    //printf("%d ",np2->count_fl); 
    //printf("%d ",np1->type_fl[0]); 
    //union start
    size=np2->count_fl;
    //printf("%d ",size); 
    for(i=0;i<(size);i++){
         //printf("inside loop");
        np1->num_fl[np1->count_fl]=np2->num_fl[i];
        np1->type_fl[np1->count_fl]=np2->type_fl[i];
        //uni[j]=b[i];
        //printf("%d ",np1->num_fl[i]);
        np1->count_fl++;
    }
    //printf("%d ",np1->type_fl[0]);
    //Sorting
    sort(np1);
    //printf("%d ",np1->type_fl[0]);
    //Remove repeated elements
    np1->count_fl=removerepeated(np1);
    //printf("%d ",np1->type_fl[0]);
    //printf("%d ",np1->count_fl);
    //printf("Array afetr Union \n");
    //printf("union res node=%d\n",np1->num);
    for(i=0;i<np1->count_fl;i++){
        //printf("%d@%d ",np1->num_fl[i],np1->type_fl[i]);
    }
    //printf("\n");
    //Sorting
}
int removerepeated(NSTRUC *np1){
    int i,j,k;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;){
            if((np1->num_fl[i]==np1->num_fl[j]) && (np1->type_fl[i]==np1->type_fl[j])){
                for(k=j;k<np1->count_fl;k++){
                    np1->num_fl[k]=np1->num_fl[k+1];
                    np1->type_fl[k]=np1->type_fl[k+1];
                }
                np1->count_fl--;
            }
            else{
                j++;
            }
        }
    }
    return(np1->count_fl);
}
void sort(NSTRUC *np1){
    int i,j,temp_num,temp_type;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;j++){
            if(np1->num_fl[i]>np1->num_fl[j]){
                temp_num=np1->num_fl[i];
                temp_type=np1->type_fl[i];
                np1->num_fl[i]=np1->num_fl[j];
                np1->type_fl[i]=np1->type_fl[j];
                np1->num_fl[j]=temp_num;
                np1->type_fl[j]=temp_type;
            }
        }
    }
}//end of union_op
int removerepeated_int(NSTRUC *np1);
void sort_int(NSTRUC *np1);
void interjection_op(NSTRUC *np1, NSTRUC *np2)
{
   int i,size1,size2,size,j=0,k;
    //int a[size1],b[size2],ini[size1+size2];
    size1=np1->count_fl;
    size2=np2->count_fl;
    int ini_num[size1];
    int ini_type[size1];
    k=0;
    //printf("interjection res node=%d, %d\n",np1->num,np2->num); 
    //printf("np1 array of faults\n");
    //for(i=0;i<size1;i++) printf("%d@%d ",np1->num_fl[i],np1->type_fl[i]);
    //printf("\n");
    //printf("np2 array of faults\n");
    //for(i=0;i<size2;i++) printf("%d@%d ",np2->num_fl[i],np2->type_fl[i]);
    //printf("\n");
    for(i=0;i<size1;i++)
    {
      for(j=0;j<size2;j++)
      {
         //printf("comparing %d %d\n",np1->num_fl[i],np2->num_fl[j]);
         if(np1->num_fl[i]==np2->num_fl[j])
         {
            //printf("Storing=%d by comparing %d %d\n",np2->num_fl[j], np1->num_fl[i],np2->num_fl[j]);
            ini_num[k]=np2->num_fl[j];
            ini_type[k]=np2->type_fl[j];
            k++;
         }
      }
    }
    np1->count_fl=k;
    for(i=0;i<k;i++)
    {
      np1->num_fl[i]=ini_num[i];
      np1->type_fl[i]=ini_type[i];
    }
    //Sorting
    sort_int(np1);
    //Remove repeated elements
    np1->count_fl=removerepeated_int(np1);
    //printf("interjection res node=%d, %d\n",np1->num,np2->num); 
    //printf("res count_fl=%d\n",np1->count_fl); 
    for(i=0;i<np1->count_fl;i++){
        //printf("%d@%d ",np1->num_fl[i],np1->type_fl[i]);
    }
    //printf("\n");
    //Sorting
}
int removerepeated_int(NSTRUC *np1){
    int i,j,k;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;){
            if((np1->num_fl[i]==np1->num_fl[j]) && (np1->type_fl[i]==np1->type_fl[j])){
                for(k=j;k<np1->count_fl;k++){
                    np1->num_fl[k]=np1->num_fl[k+1];
                    np1->type_fl[k]=np1->type_fl[k+1];
                }
                np1->count_fl--;
            }
            else{
                j++;
            }
        }
    }
    return(np1->count_fl);
}
void sort_int(NSTRUC *np1){
    int i,j,temp_num,temp_type;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;j++){
            if(np1->num_fl[i]>np1->num_fl[j]){
                temp_num=np1->num_fl[i];
                temp_type=np1->type_fl[i];
                np1->num_fl[i]=np1->num_fl[j];
                np1->type_fl[i]=np1->type_fl[j];
                np1->num_fl[j]=temp_num;
                np1->type_fl[j]=temp_type;
            }
        }
    }
}
int removerepeated_aminusb(NSTRUC *np1);
void sort_aminusb(NSTRUC *np1);
void AminusB(NSTRUC *np1, NSTRUC *np2)
{
   int i,size1,size2,size,j=0,k;
    //int a[size1],b[size2],ini[size1+size2];
    size1=np1->count_fl;
    size2=np2->count_fl;
    int ini_num[size1];
    int ini_type[size1];
    int found=0;
    k=0;
    //for(i=0;i<size1;i++) printf("%d ",np1->num_fl[i]);
    //printf("\n");
    //for(i=0;i<size2;i++) printf("%d ",np2->num_fl[i]);
    //printf("\n");
    for(i=0;i<size1;i++)
    {
      for(j=0;j<size2;j++)
      {
         if(np1->num_fl[i]==np2->num_fl[j]) found=1; 
      }
      if(found==0)
      {
         //if(np1->num_fl[i]!=np2->num_fl[j])
         {
            //printf("storing=%d\n",np1->num_fl[i]);
            ini_num[k]=np1->num_fl[i];
            ini_type[k]=np1->type_fl[i];
            k=k+1;
         }
      }
      //else printf("duplicate=%d\n",np1->num_fl[i]);
      found=0;
    }
    np1->count_fl=k;
    for(i=0;i<k;i++)
    {
      np1->num_fl[i]=ini_num[i];
      np1->type_fl[i]=ini_type[i];
    }
    //Sorting
    sort_aminusb(np1);
    //Remove repeated elements
    np1->count_fl=removerepeated_aminusb(np1);
   //printf("a-b res node=%d\n",np1->num);
    for(i=0;i<np1->count_fl;i++){
        //printf("%d@%d ",np1->num_fl[i],np1->type_fl[i]);
    }
    //printf("\n");
    //Sorting
}
int removerepeated_aminusb(NSTRUC *np1){
    int i,j,k;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;){
            if((np1->num_fl[i]==np1->num_fl[j]) && (np1->type_fl[i]==np1->type_fl[j]) ){
                for(k=j;k<np1->count_fl;k++){
                    np1->num_fl[k]=np1->num_fl[k+1];
                    np1->type_fl[k]=np1->type_fl[k+1];
                }
                np1->count_fl--;
            }
            else{
                j++;
            }
        }
    }
    return(np1->count_fl);
}
void sort_aminusb(NSTRUC *np1){
    int i,j,temp_num,temp_type;
    for(i=0;i<np1->count_fl;i++){
        for(j=i+1;j<np1->count_fl;j++){
            if(np1->num_fl[i]>np1->num_fl[j]){
                temp_num=np1->num_fl[i];
                temp_type=np1->type_fl[i];
                np1->num_fl[i]=np1->num_fl[j];
                np1->type_fl[i]=np1->type_fl[j];
                np1->num_fl[j]=temp_num;
                np1->type_fl[j]=temp_type;
            }
        }
    }
} 

//dfs:
dfs(cp)
char *cp;
{
   sscanf(cp, "%s %s", input_file_dfs,output_file_dfs);
   char line[10000];
   int i,j,k,current_level,node_number,logic_val;
   NSTRUC *np,tmp;
   int max_level = get_max_level();
   
   FILE *file_ptr;
   file_ptr = fopen(input_file_dfs,"r");
   FILE *file_ptr2;
   file_ptr2 = fopen(output_file_dfs,"w");
   char *pt;
   const char *comp_x = "x";
   const char *comp_X = "X";
   int first_line =0;
   int node_no[Npi];
   char logicvalue[10000][Npi];
   for(i=0; i<1000; i++) memset(logicvalue[i],0,strlen(logicvalue[i]));
   int a=0, b=0; //a is # input patterns, b used internally which is basically Npi
   int x,y,c;
   //**********************************************
   //************DFS input*************************
   //struct dfs_faultlist dfs_fl[Nnodes];
   //int dfs_indx=0;
   NSTRUC *temp_xor_union;
   NSTRUC *temp_xor_int;
   temp_xor_union = (NSTRUC *) malloc(sizeof(NSTRUC));
   temp_xor_int = (NSTRUC *) malloc(sizeof(NSTRUC));
   // Assigning logical value to the Primary inputs:
   int non_cont, cont;
   int non_cont_indx[100],cont_indx[100];
   int count;
   //************************************************
   while(!feof(file_ptr))
   {
      memset(line,0,strlen(line));
      fgets(line,10000,file_ptr);
      //printf("%d\n", strlen(line));
      //printf("%s",line);
      if(line[strlen(line)-1]=='\n') line[strlen(line)-1]='\0';
      if(strlen(line)==0) break;
      //printf("%s",line);
      if(first_line==0){
         i=0;
         pt = strtok (line,",");
         while (pt != NULL) {
            node_no[i] = atoi(pt);
            //printf("%d", node_no[i]);
            pt = strtok (NULL, ",");
            i++;
         }
         first_line++;
      }
      else{
         pt = strtok (line,",");
         while (pt != NULL) {
            //printf("%s",pt);
            if((strcmp(pt, comp_x)==0)||(strcmp(pt, comp_X)==0)){ 
            logicvalue[a][b] = -1;   
            //printf("%d", logicvalue[a][b]);
            }
            else{
                logicvalue[a][b] = atoi(pt);
            }    
            //printf("%d", logicvalue[a][b]);
            pt = strtok (NULL, ",");
            //printf("%s",pt);
            b++;
         }
         b=0;
         //printf("\n");
         a++;
      } 
      //sscanf(line,"%d,%d",&node_number,&logic_val);
      // Can we reduce time complexity by using Pinput[] and Npi?
       
   }
   fclose(file_ptr);
   //int x=0;
   for(x=0;x<(a);x++)
   {
      temp_xor_union->count_fl=0;
      temp_xor_int->count_fl=0;
      for(i=0;i<Nnodes;i++)
      {
         np=&Node[i];
         np->count_fl=0;
      }
       for(y=0;y<Npi;y++) 
       {
         //printf("current node: %d\n",Pinput[i]->num);
          if(Pinput[y]->num == node_no[y])
          { Pinput[y]->logical_value = logicvalue[x][y];
            //printf("node-%d assigned value: %d\n",Pinput[i]->num,Pinput[i]->logical_value);
            //x++;
          }
       }      
   //printf("x=%d",x);
   /*int c,d;
      for( c=0;c<a-1;c++){
         for( d=0;d<Npi;d++) printf("%d",logicvalue[c][d]);
         printf("\n");
      }*/

   //----------------PIs have been assigned their logical value-------------------------

   //printf("Logic value assigned to all PIs\n");
   //printf("PI logic values: \n");
   //for(i=0;i<Npi;i++)
   //{
      
      //printf("node:%d   val:%d\n",Pinput[i]->num,Pinput[i]->logical_value);
   //}
   

   current_level = 1; // We have already assigned logical values to all the nodes in level-0 
   //                0     1    2   3    4    5    6     7
   // enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */
   //while(!all_logics_assigned() && current_level<=max_level)
   while(current_level<=max_level)
   {
      //printf("\n\nCurrent level: %d\n",current_level);
      for(i=0;i<Nnodes;i++)
      {
         np = &Node[i];
         if((np->level== current_level) && (np->type!=0)) // We assign values level-by-level. 
         {
            if(np->type == 1) // BRANCH: We assign it the value of its parent branch.
            { 
               //printf("Entered 1\n");
               np->logical_value = np->unodes[0]->logical_value; // Branches will always have just 1 upnode.
               //printf("node-%d is a Branch. value assigned: %d\n\n",np->num,np->logical_value);
            }
            


            if(np->type == 2) // XOR
            {
               //printf("Entered 2\n");
               np->logical_value = 0; // 0^x = x; so we initialize the xor result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value ^ np->unodes[j]->logical_value;
                  else np->logical_value = -1;
               }
               //printf("node-%d is an XOR gate. value assigned: %d\n\n",np->num,np->logical_value);
            }

            if(np->type == 3) // OR
            {
               //printf("Entered 3\n");
               np->logical_value = 0; // 0|x = x; so we initialize the OR result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value | np->unodes[j]->logical_value;
                  else np->logical_value = -1;
               }
               //printf("node-%d is an OR gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 4) // NOR
            {
               //printf("Entered 4\n");
               np->logical_value = 0; // 0|x = x; so we initialize the OR result to 0.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value | np->unodes[j]->logical_value;
                  else np->logical_value = -1;
               }
               if(np->logical_value != -1) np->logical_value = 1-np->logical_value; // Invert once for NOR.
               //printf("node-%d is a NOR gate. value assigned: %d\n\n",np->num,np->logical_value);
            } 



            if(np->type == 5) // NOT
            {
               //printf("Entered 5\n");
               //printf("unode:%d    unode val:%d\n",np->unodes[0]->num,np->unodes[0]->logical_value);
               if(np->unodes[0]->logical_value == -1) np->logical_value = -1;
               else np->logical_value = 1 - np->unodes[0]->logical_value; // Invert.
               //printf("node-%d is a NOT gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 6) // NAND
            {
               //printf("Entered 6\n");
               np->logical_value = 1; // 1&x = x; so we initialize the NAND result to 1.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value & np->unodes[j]->logical_value;
                  else np->logical_value = -1;
               }
               if(np->logical_value != -1) np->logical_value = 1-np->logical_value; // Invert once for NAND.
               //printf("node-%d is a NAND gate. value assigned: %d\n\n",np->num,np->logical_value);
            }



            if(np->type == 7) //AND
            {
               //printf("Entered 7\n");
               np->logical_value = 1; // 1&x = x; so we initialize the NAND result to 1.
               for(j=0;j<np->fin;j++)
               {
                  if(np->unodes[j]->logical_value != -1) np->logical_value = np->logical_value & np->unodes[j]->logical_value;
                  else np->logical_value = -1;
               }
               //printf("node-%d is an AND gate. value assigned: %d\n\n",np->num,np->logical_value);
            }
         }

         else
         {
            //printf("not entered for:: node:%d  level:%d\n",np->num,np->level);
         }
      }
      current_level+=1;
   }
   // At this point all the nodes have been assigned a logical value and we have traversed
   // all the levels. Hence, we now write the output logic values to the output files.
   //******************************
   //DFS start:
   //******************************
   for(i=0;i<Npi;i++)
   {
      Pinput[i]->num_fl[Pinput[i]->count_fl]=Pinput[i]->num;
      Pinput[i]->type_fl[Pinput[i]->count_fl]=1-(Pinput[i]->logical_value);
      Pinput[i]->count_fl=Pinput[i]->count_fl+1;
      //union_op(Pinput[i],Pinput[i]);
      //interjection_op(Pinput[i],Pinput[i]);
      //AminusB(Pinput[i],Pinput[i]);
   }
   current_level = 1; // We have already assigned logical values to all the nodes in level-0 
   //                0     1    2   3    4    5    6     7
   // enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */
   //while(!all_logics_assigned() && current_level<=max_level)
   while(current_level<=max_level)
   {
      //printf("\n\nCurrent level: %d\n",current_level);
      for(i=0;i<Nnodes;i++)
      {
         np = &Node[i];
         
         
         if((np->level== current_level) && (np->type!=0)) // We assign values level-by-level. 
         {
            np->num_fl[np->count_fl]=np->num;
            np->type_fl[np->count_fl]=1-(np->logical_value);
            np->count_fl=np->count_fl+1;
            //printf("%d@%d ",np->num_fl,np->type_fl);
         
            if(np->type == 1) // BRANCH: We assign it the value of its parent branch.
            { 
               //printf("\nEntered branch for node=%d\n",np->num);
               union_op(np, np->unodes[0]);
               //printf("count_fl=%d np->num_fl[0]=%d np->type_fl[0]=%d\n",np->count_fl, np->num_fl[0], np->type_fl[0]);
               //printf("count_fl=%d np->num_fl[1]=%d np->type_fl[1]=%d\n\n",np->count_fl, np->num_fl[1], np->type_fl[1]); 
               //count=np->count_fl;
               //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
               //np->logical_value = np->unodes[0]->logical_value; // Branches will always have just 1 upnode.
               //printf("node-%d is a Branch. value assigned: %d\n\n",np->num,np->logical_value);
            }
            


            if(np->type == 2) // XOR
            {
               temp_xor_union->num=np->unodes[0]->num;
               temp_xor_int->num=np->unodes[0]->num;
               temp_xor_union->count_fl=np->unodes[0]->count_fl;
               temp_xor_int->count_fl=np->unodes[0]->count_fl;
               for(j=0;j<np->unodes[0]->count_fl;j++)
               {
                  temp_xor_union->num_fl[j]=np->unodes[0]->num_fl[j];
                  temp_xor_int->num_fl[j]=np->unodes[0]->num_fl[j];
                  temp_xor_union->type_fl[j]=np->unodes[0]->type_fl[j];
                  temp_xor_int->type_fl[j]=np->unodes[0]->type_fl[j];
               }
               /*printf("current node=%d\n",np->num);
               printf("unode list=%d\n",np->unodes[0]->num);
               for(j=0;j<np->unodes[0]->count_fl;j++)
                  printf("%d@%d ",np->unodes[0]->num_fl[j], np->unodes[0]->type_fl[j]);

               printf("\n\ntemp_xor_union");
               for(j=0;j<temp_xor_union->count_fl;j++)
                  printf("%d@%d ",temp_xor_union->num_fl[j], temp_xor_union->type_fl[j]);
               
               printf("\n\ntemp_xor_int");
               for(j=0;j<temp_xor_int->count_fl;j++)
                  printf("%d@%d ",temp_xor_int->num_fl[j], temp_xor_int->type_fl[j]);*/

               //printf("\n\n");   
               //exit(-1);
               //la U lb - la n lb
               //printf("Entered 2\n");
               /*printf("unode list=%d\n",np->unodes[0]->num);
               for(j=0;j<np->unodes[0]->count_fl;j++)
                  printf("%d@%d ",np->unodes[0]->num_fl[j], np->unodes[0]->type_fl[j]);
               printf("\n");   
               printf("unode list=%d\n",np->unodes[1]->num);
               for(j=0;j<np->unodes[1]->count_fl;j++)
                  printf("%d@%d ",np->unodes[1]->num_fl[j], np->unodes[1]->type_fl[j]);

               printf("\n");*/
               for(j=1;j<np->fin;j++)
               {
                  union_op(temp_xor_union,np->unodes[j]);
                  interjection_op(temp_xor_int, np->unodes[j]);
               }
               /*printf("count_fl of temp_xor_union=%d\n",temp_xor_union->count_fl);
               printf("union_op:\n");
               for(j=0;j<temp_xor_union->count_fl;j++)
               { 
                  printf("%d@%d ",temp_xor_union->num_fl[j], temp_xor_union->type_fl[j]);
               }

               printf("\n\ninterjection:\n");
               for(j=0;j<temp_xor_int->count_fl;j++) 
                  printf("%d@%d ",temp_xor_int->num_fl[j], temp_xor_int->type_fl[j]);*/   
               AminusB(temp_xor_union,temp_xor_int);
               union_op(np,temp_xor_union);
               
               /*printf("AminusB:\n");
               for(j=0;j<temp_xor_union->count_fl;j++) 
                  printf("%d@%d ",temp_xor_union->num_fl[j], temp_xor_union->type_fl[j]);
               
               printf("\n\nfinal xor fl\n");
               for(j=0;j<np->count_fl;j++) 
                  printf("%d@%d ",np->num_fl[j], np->type_fl[j]);   

               printf("existing xor\n");
               exit(-1);*/
               //printf("node-%d is an XOR gate. value assigned: %d\n\n",np->num,np->logical_value);
            }

            if(np->type == 3) // OR
            {
               //printf("current node in dfs=%d\n",np->num);
               //printf("current nand gate=%d\n",np->num);
               non_cont=0;
               cont=0;
               for(j=0;j<np->fin;j++)
               {
                  //printf("inside np->fin=%d\n",np->fin);
                  if(np->unodes[j]->logical_value==0)
                  {  
                     //printf("1.unodes[%d]=%d, %d\n",j,np->unodes[j]->num,np->unodes[j]->logical_value);
                     non_cont_indx[non_cont]=j;
                     non_cont++;
                     //printf("non controlling count for %d=%d\n",np->num,non_cont);
                  }
                  if(np->unodes[j]->logical_value==1){ 
                     //printf("2.unodes[%d]=%d\n",j,np->unodes[j]->num);
                     cont_indx[cont]=j;
                     cont++;
                  }   
               }
               //exit(-1);
               if(non_cont==np->fin)
               {
                  //printf("inside ");
                  for(j=0;j<(np->fin-1);j++)
                  {
                     union_op(np->unodes[j+1],np->unodes[j]);
                  }
                  union_op(np,np->unodes[np->fin-1]);
               }
               else if(cont>=1)
               {
                  //for(j=0;j<cont-1;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
                  if(cont==1)
                  {
                     //printf("cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<cont-1;j++)
                     {
                        interjection_op(np->unodes[cont_indx[j+1]],np->unodes[cont_indx[j]]);
                     }
                  }


                  if(non_cont==1)
                  {
                     //printf("non_cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<non_cont-1;j++)
                     {
                        union_op(np->unodes[non_cont_indx[j+1]],np->unodes[non_cont_indx[j]]);
                     }
                  }


                  if(cont>0 && non_cont>0)
                  { 
                     //printf("AminusB visted for node=%d\n",np->num);
                     AminusB(np->unodes[cont_indx[cont-1]],np->unodes[non_cont_indx[non_cont-1]]);
                     //printf("AMinusB is done\n");
                     //exit(-1);
                  }


                  //printf("union visted for node=%d\n",np->num);
                  union_op(np,np->unodes[cont_indx[cont-1]]); 
                  //printf("union visted for node=%d\n",np->num);
                  //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]); 
                  //printf("\n");
                  //printf("inside non_cont else\n");
                  //exit(-1);
               }
               //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
            }



            if(np->type == 4) // NOR
            {
               //printf("current node in dfs=%d\n",np->num);
               //printf("current nand gate=%d\n",np->num);
               non_cont=0;
               cont=0;
               for(j=0;j<np->fin;j++)
               {
                  //printf("inside np->fin=%d\n",np->fin);
                  if(np->unodes[j]->logical_value==0)
                  {  
                     //printf("1.unodes[%d]=%d, %d\n",j,np->unodes[j]->num,np->unodes[j]->logical_value);
                     non_cont_indx[non_cont]=j;
                     non_cont++;
                     //printf("non controlling count for %d=%d\n",np->num,non_cont);
                  }
                  if(np->unodes[j]->logical_value==1){ 
                     //printf("2.unodes[%d]=%d\n",j,np->unodes[j]->num);
                     cont_indx[cont]=j;
                     cont++;
                  }   
               }
               //exit(-1);
               if(non_cont==np->fin)
               {
                  //printf("inside ");
                  for(j=0;j<(np->fin-1);j++)
                  {
                     union_op(np->unodes[j+1],np->unodes[j]);
                  }
                  union_op(np,np->unodes[np->fin-1]);
               }
               else if(cont>=1)
               {
                  //for(j=0;j<cont-1;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
                  if(cont==1)
                  {
                     //printf("cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<cont-1;j++)
                     {
                        interjection_op(np->unodes[cont_indx[j+1]],np->unodes[cont_indx[j]]);
                     }
                  }


                  if(non_cont==1)
                  {
                     //printf("non_cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<non_cont-1;j++)
                     {
                        union_op(np->unodes[non_cont_indx[j+1]],np->unodes[non_cont_indx[j]]);
                     }
                  }


                  if(cont>0 && non_cont>0)
                  { 
                     //printf("AminusB visted for node=%d\n",np->num);
                     AminusB(np->unodes[cont_indx[cont-1]],np->unodes[non_cont_indx[non_cont-1]]);
                     //printf("AMinusB is done\n");
                     //exit(-1);
                  }


                  //printf("union visted for node=%d\n",np->num);
                  union_op(np,np->unodes[cont_indx[cont-1]]); 
                  //printf("union visted for node=%d\n",np->num);
                  //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]); 
                  //printf("\n");
                  //printf("inside non_cont else\n");
                  //exit(-1);
               }
               //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
            } 



            if(np->type == 5) // NOT
            {
               union_op(np,np->unodes[0]);
            }

            if(np->type == 6) // NAND
            {
               //printf("current node in dfs=%d\n",np->num);
               //printf("current nand gate=%d\n",np->num);
               non_cont=0;
               cont=0;
               for(j=0;j<np->fin;j++)
               {
                  //printf("inside np->fin=%d\n",np->fin);
                  if(np->unodes[j]->logical_value==1)
                  {  
                     //printf("1.unodes[%d]=%d, %d\n",j,np->unodes[j]->num,np->unodes[j]->logical_value);
                     non_cont_indx[non_cont]=j;
                     non_cont++;
                     //printf("non controlling count for %d=%d\n",np->num,non_cont);
                  }
                  if(np->unodes[j]->logical_value==0){ 
                     //printf("2.unodes[%d]=%d\n",j,np->unodes[j]->num);
                     cont_indx[cont]=j;
                     cont++;
                  }   
               }
               //exit(-1);
               if(non_cont==np->fin)
               {
                  //printf("inside ");
                  for(j=0;j<(np->fin-1);j++)
                  {
                     union_op(np->unodes[j+1],np->unodes[j]);
                  }
                  union_op(np,np->unodes[np->fin-1]);
               }
               else if(cont>=1)
               {
                  //for(j=0;j<cont-1;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
                  if(cont==1)
                  {
                     //printf("cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<cont-1;j++)
                     {
                        interjection_op(np->unodes[cont_indx[j+1]],np->unodes[cont_indx[j]]);
                     }
                  }


                  if(non_cont==1)
                  {
                     //printf("non_cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<non_cont-1;j++)
                     {
                        union_op(np->unodes[non_cont_indx[j+1]],np->unodes[non_cont_indx[j]]);
                     }
                  }


                  if(cont>0 && non_cont>0)
                  { 
                     //printf("AminusB visted for node=%d\n",np->num);
                     AminusB(np->unodes[cont_indx[cont-1]],np->unodes[non_cont_indx[non_cont-1]]);
                     //printf("AMinusB is done\n");
                     //exit(-1);
                  }


                  //printf("union visted for node=%d\n",np->num);
                  union_op(np,np->unodes[cont_indx[cont-1]]); 
                  //printf("union visted for node=%d\n",np->num);
                  //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]); 
                  //printf("\n");
                  //printf("inside non_cont else\n");
                  //exit(-1);
               }
               //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
            }


            if(np->type == 7) //AND
            {
               //printf("current node in dfs=%d\n",np->num);
               //printf("current nand gate=%d\n",np->num);
               non_cont=0;
               cont=0;
               for(j=0;j<np->fin;j++)
               {
                  //printf("inside np->fin=%d\n",np->fin);
                  if(np->unodes[j]->logical_value==1)
                  {  
                     //printf("1.unodes[%d]=%d, %d\n",j,np->unodes[j]->num,np->unodes[j]->logical_value);
                     non_cont_indx[non_cont]=j;
                     non_cont++;
                     //printf("non controlling count for %d=%d\n",np->num,non_cont);
                  }
                  if(np->unodes[j]->logical_value==0){ 
                     //printf("2.unodes[%d]=%d\n",j,np->unodes[j]->num);
                     cont_indx[cont]=j;
                     cont++;
                  }   
               }
               //exit(-1);
               if(non_cont==np->fin)
               {
                  //printf("inside ");
                  for(j=0;j<(np->fin-1);j++)
                  {
                     union_op(np->unodes[j+1],np->unodes[j]);
                  }
                  union_op(np,np->unodes[np->fin-1]);
               }
               else if(cont>=1)
               {
                  //for(j=0;j<cont-1;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
                  if(cont==1)
                  {
                     //printf("cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<cont-1;j++)
                     {
                        interjection_op(np->unodes[cont_indx[j+1]],np->unodes[cont_indx[j]]);
                     }
                  }


                  if(non_cont==1)
                  {
                     //printf("non_cont visted for node=%d\n",np->num);
                     //continue;
                  }
                  else {
                     for(j=0;j<non_cont-1;j++)
                     {
                        union_op(np->unodes[non_cont_indx[j+1]],np->unodes[non_cont_indx[j]]);
                     }
                  }


                  if(cont>0 && non_cont>0)
                  { 
                     //printf("AminusB visted for node=%d\n",np->num);
                     AminusB(np->unodes[cont_indx[cont-1]],np->unodes[non_cont_indx[non_cont-1]]);
                     //printf("AMinusB is done\n");
                     //exit(-1);
                  }


                  //printf("union visted for node=%d\n",np->num);
                  union_op(np,np->unodes[cont_indx[cont-1]]); 
                  //printf("union visted for node=%d\n",np->num);
                  //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]); 
                  //printf("\n");
                  //printf("inside non_cont else\n");
                  //exit(-1);
               }
               //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
            }
         }
         else
         {
            //printf("not entered for:: node:%d  level:%d\n",np->num,np->level);
         }
         //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);
      }
      current_level+=1;
   } 
   for(j=0;j<Npo-1;j++)
   {
      union_op(Poutput[j+1],Poutput[j]);
   } 
   //printf("final dfs list\n");
   //for(j=0;j<Poutput[Npo-1]->count_fl;j++) printf("%d@%d ",Poutput[Npo-1]->num_fl[j],Poutput[Npo-1]->type_fl[j]);
   //printf("\n");
   for(j=0;j<Poutput[Npo-1]->count_fl;j++) fprintf(file_ptr2,"%d@%d\n",Poutput[Npo-1]->num_fl[j],Poutput[Npo-1]->type_fl[j]); 
   //fclose(file_ptr2);
   //for(j=0;j<np->count_fl;j++) printf("%d@%d ",np->num_fl[j],np->type_fl[j]);   
   }// end of for loop 
   fclose(file_ptr2);    
}//dfs end







//pfs:
pfs(cp)
char *cp;
{
   printf("Entered pfs\n");
   FILE *f,*f1,*f2;
   char fault_list[MAXLINE];
   char input_vector_list[MAXLINE];
   char output_file[MAXLINE];
   char line[MAXLINE];
   
   sscanf(cp, "%s %s %s",input_vector_list,fault_list,output_file);
   printf("input_vector_list: %s\n\n",input_vector_list);
   printf("fault_list: %s\n\n",fault_list);
   printf("output file: %s\n",output_file);
   float q = get_lines(fault_list); // Must be float otherwise q/W-1 itself gets rounded.
   int iterations = ceil(q/(W-1)); // No. of iterations = ceil(q/(W-1)).
   int iteration_count=0,test_count=1;
   int cycles = (q >= W) ? W-1 : q;//No. of faults read in every iteration. 
   
   struct pfs_node map[cycles];
   int current_level=0;
   int current_fault_free_val;
   int max_level = get_max_level();
   printf("q:%f W:%d cycles: %d\n",q,W,cycles);
   int node_number,logic_number;
   struct pfs_node current_input_vector[Npi];// Store PI node_num and its value.

   int i,j,k,l,a,b,c;
   char *input_vector;
   int  logic_val;
   int iter=0; // Iteration number.
   NSTRUC *np;
   
   
   //Set fault map:
   
   f = fopen(fault_list,"r");
   //printf("W: %d\n",W);
   printf("q: %f\n",q);
   printf("cycles: %d\n",cycles);
   i=0; // For storing map list;
   
   while(!feof(f))
   {
      for(j=1;j<=cycles;j++) // Because map[0] has the fault-free value hence, faults will get stored starting from map[1].
      {
         
         if(!feof(f)) //if q=10 and w=5, cycles=3. In the last iteration, only 2 values will be left by cycles=3 so we don't won't to read garbage in that cycle.
         {
               fgets(line,MAXLINE,f);
               sscanf(line,"%d@%d", &node_number,&logic_number);
               map[j].node_num = node_number;
               map[j].fault_type = logic_number;
               printf("map[%d]: %d %d\n",j,map[j].node_num,map[j].fault_type);
         }  
         
      }
   }
   fclose(f); // Close fault list. 
   //FAULT MAP HAS BEEN INITIALIZED NOW. 
   printf("MAIN FAULT MAP HAS BEEN INITIALIZED NOW.\n\n");
   printf("Resetting found bit for all the \n");

   for(j=1;j<=cycles;j++)
   {
      map[j].found=0; //Reset found for all faults before storing.
   }


   f1 = fopen(input_vector_list,"r");
   fgets(line,MAXLINE,f1);// First line will give PI order.
   input_vector = strtok(line,",");
   //printf("strtok done\n\n");
   i=0;
   while(input_vector != NULL)
   {
      current_input_vector[i].node_num = atoi(input_vector);
      i+=1;
      input_vector = strtok(NULL,",");
   }

   //input vectors start from next line onwards i.e. line-2.

   while(!feof(f1)) // Perform pfs() for every input vector.
   {
        printf("\n\n\n Running for test-vector-%d\n");
        fgets(line,MAXLINE,f1);// From hereon line stores input vector.
        printf("line: %s\n\n",line);
        input_vector = strtok(line,",");
        i=0;
        while(input_vector != NULL)
        {
            current_input_vector[i].fault_type = atoi(input_vector);
            i+=1;
            input_vector = strtok(NULL,",");
        }

        for(i=0;i<Npi;i++)
        {
            //printf("Current input: %d\n",Pinput[i]->num);
            for(j=0;j<Npi;j++)
            {
                if(Pinput[i]->num == current_input_vector[j].node_num)
                {
                   Pinput[i]->logical_value = current_input_vector[j].fault_type;
                   Pinput[i]->fault_list[0] = current_input_vector[j].fault_type;
                   //printf("assigning input %d to %d\n",Pinput[i]->num,current_input_vector[j].fault_type);  
                }
            }
            
        }

       // At this point we have initialized all the PIs.
       logicsim_single_pass();
        


      //UPDATE FAULT LIST OF ALL THE NODES LEVEL-BY-LEVEL:
      current_level = 0;
      while(current_level <= max_level)
      {
         printf("\ncurrent_level: %d\n",current_level);
         for(i=0;i<Nnodes;i++)
         {
            np = &Node[i];
            if(np->level == current_level)
            {
               if(np->type == 0) //PI
               {
                  for(j=1;j<=cycles;j++)
                  {
                        np->fault_list[j] = np->logical_value;
                  }
         
                  printf("\n\nFinal fault_list of PI %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }     
               }



               if(np->type == 1) //BRANCH
               {
                  //Just copy the fault_list of unodes[0].
                  for(j=1;j<=cycles;j++)
                  {
                     np->fault_list[j] = np->unodes[0]->fault_list[j];
                  }

                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }

                  printf("\n\nFinal fault_list of BRANCH Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
                  

               }

               if(np->type == 2) //XOR
               {
                  //INITIALIZE THE FAULT LIST:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 0; //0^x = x; so we initialize the fault_list to 0.
                  }

                  //XOR the fault_lists of all the unodes
                  for(j=0;j<np->fin;j++)
                  {
                     for(k=1;k<=cycles;k++)
                     {
                        np->fault_list[k] = np->fault_list[k] ^ np->unodes[j]->fault_list[k]; 
                     }
                  }

                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }

                  printf("\n\nFinal fault_list of XOR Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
               }



               if(np->type == 3) //OR
               {
                  //INITIALIZE THE FAULT LIST:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 0; //0|x = x; so we initialize the fault_list to 0.
                  }


                  //OR the fault_lists of all the unodes
                  for(j=0;j<np->fin;j++)
                  {
                     for(k=1;k<=cycles;k++)
                     {
                        np->fault_list[k] = np->fault_list[k] | np->unodes[j]->fault_list[k]; 
                     }
                  }

                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }

                  printf("\n\nFinal fault_list of OR Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
               }


               if(np->type == 4) //NOR
               {
                  //INITIALIZE THE FAULT LIST:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 0; //0|x = x; so we initialize the fault_list to 0.
                  }


                  //OR the fault_lists of all the unodes
                  for(j=0;j<np->fin;j++)
                  {
                     for(k=1;k<=cycles;k++)
                     {
                        np->fault_list[k] = np->fault_list[k] | np->unodes[j]->fault_list[k]; 
                     }
                  }

                  //INVERT THE FINAL FAULT LIST TO GET NOR:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 1 - np->fault_list[k];
                  }

                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }


                  printf("\n\nFinal fault_list of NOR Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
               }


               if(np->type == 5) //NOT
               {
                  //INVERT the fault_lists of all the unodes
                  for(j=1;j<=cycles;j++)
                  {
                     np->fault_list[j] = 1 - np->unodes[0]->fault_list[j];
                     
                  }
                  
                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }

                  printf("\n\nFinal fault_list of NOT Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
                  
               }



               if(np->type == 6) //NAND
               {
                  //INITIALIZE THE FAULT LIST:
                  printf("\nSetting fault list for node: %d\n",np->num);
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 1; //1&x = x; so we initialize the fault_list to 1.
                  }
                  
                  //printf("list after initializing:\n");
                  for(k=1;k<=cycles;k++)
                  {
                     //printf("%d ",np->fault_list[k]);
                  }
                  //printf("\n");

                  //AND the fault_lists of all the unodes
                  for(j=0;j<np->fin;j++)
                  {
                     for(k=1;k<=cycles;k++)
                     {
                        np->fault_list[k] = np->fault_list[k] & np->unodes[j]->fault_list[k]; 
                     }
                  }

                  //printf("List after ANDING:\n");
                  for(k=1;k<=cycles;k++)
                  {
                     //printf("%d ",np->fault_list[k]);
                  }
                  //printf("\n");

                  //INVERT THE FINAL FAULT LIST TO GET NAND:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 1 - np->fault_list[k];
                  }

                  //printf("Listing after NANDING:\n");
                  for(k=1;k<=cycles;k++)
                  {
                     //printf("%d ",np->fault_list[k]);
                  }

                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) 
                     {
                        np->fault_list[j] = map[j].fault_type;
                        //printf("\nnp->num: %d\n",np->num);
                        //printf("\nUpdating node list for %d\n",j);
                        //printf("\nmap[%d].node_num: %d\n",j,map[j].node_num);
                        //printf("map[j].fault_type: %d\n",map[j].fault_type);
                     }
                  }

                  printf("\nFinal fault_list of NAND Node %d:\n",np->num);
                  for(j=0;j<=cycles;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
                  
               }





               if(np->type == 7) //AND
               {
                  //INITIALIZE THE FAULT LIST:
                  for(k=1;k<=cycles;k++)
                  {
                     np->fault_list[k] = 1; //1&x = x; so we initialize the fault_list to 1.
                  }

                  //AND the fault_lists of all the unodes
                  for(j=0;j<np->fin;j++)
                  {
                     for(k=1;k<=cycles;k++)
                     {
                        np->fault_list[k] = np->fault_list[k] & np->unodes[j]->fault_list[k]; 
                     }
                  }

                  //UPDATE THE FAULT LIST FOR CURRENT NODE'S FAULTS:
                  for(j=1;j<=cycles;j++)
                  {
                     if(np->num == map[j].node_num) np->fault_list[j] = map[j].fault_type;
                  }

                  printf("\n\nFinal fault_list of AND Node %d:\n",np->num);
                  for(j=0;j<W;j++)
                  {
                     printf("%d ",np->fault_list[j]);
                  }
               }

            }
         }
         current_level+=1;
      }

      printf("\n\nFAULT LIST UPDATED FOR ALL THE NODES\n");

      //STORE THE DETECTABLE FAULTS TO THE OUTPUT FILE:
      //using file append will be helpful.
      
      f2 = fopen(output_file,"a");
      printf("Writing all detectable faults to the output file:\n");
      for(j=0;j<Npo;j++)
      {
         current_fault_free_val = Poutput[j]->fault_list[0];
         printf("\n\noutput node: %d\n",Poutput[j]->num);
         printf("current_fault_free_val: %d\n",Poutput[j]->fault_list[0]);
         for(k=1;k<cycles;k++)
         {
            if(current_fault_free_val ^ Poutput[j]->fault_list[k])
            {
               printf("Writing for output j = %d\n",j);
                  if(map[k].found==0)
                  {
                  map[k].found=1;
                  printf("k:%d\n",k);
                  printf("Storing %d@%d\n",map[k].node_num,map[k].fault_type);
                  //printf("map[%d].node_num: %d\n",k,map[k].node_num);
                  //printf("map[%d].fault_type: %d\n",k,map[k].fault_type);
                  //printf("storing: %d@%d\n",map[k].node_num,map[k].fault_type);
                  fprintf(f2,"%d@%d\n",map[k].node_num,map[k].fault_type);
                  }
               //exit(-1);
            }
         }
      }
      fclose(f2); //Close output file.
        //NOTE: FINAL OUTPUT FILE HAS DUPLICATES. REMOVE THOSE DUPLICATES.
        //printf("One iteration completed\n");
        //exit(-1);
   } 
      //fclose(f); // Close fault list.
    fclose(f1); // Close Input vector list.
    printf("PFS COMPLETED SUCCESSFULLY\n");
}

   




//rtg:
rtg(cp)
char *cp;
{
   printf("Entered the rtg function\n\n");
   
}
/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
  The routine receive an integer gate type and return the gate type in
  character string.
-----------------------------------------------------------------------*/
char *gname(tp)
int tp;
{
   switch(tp) {
      case 0: return("PI");
      case 1: return("BRANCH");
      case 2: return("XOR");
      case 3: return("OR");
      case 4: return("NOR");
      case 5: return("NOT");
      case 6: return("NAND");
      case 7: return("AND");
   }
}
/*========================= End of program ============================*/