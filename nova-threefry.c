/*
 * Implement the Threefry PRNG for Singular Computing's S1 system
 * By Scott Pakin <pakin@lanl.gov>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <scAcceleratorAPI.h>
#include <scNova.h>


int emulated;                     // 1 for emulated machine, 0 for real S1 hardware
extern int scTotalCyclesTaken;    // if emulated, total cycles taken to run last kernel
extern MachineState *scEmulatedMachineState;   // if emulated, pointer to emulated machine's state
extern LLKernel *llKernel;

// Size info of the real or emulated machine
int chipRows;
int chipCols;
int apeRows;
int apeCols;

// Bits controling how much tracing to do, per scAcceleratorAPI.h
int traceFlags;

// Assign each APE a unique row ID, column ID, and overall ID.
void emitApeIDAssignment()
{
  // Tell each APE its row number.
  DeclareApeVarInit(myRow, Int, IntConst(0));
  DeclareCUVar(rowNum, Int);
  CUFor(rowNum, IntConst(1), IntConst(apeRows), IntConst(1));
  eApeC(apeGet, myRow, _, getNorth);
  Set(myRow, Add(myRow, IntConst(1)));
  CUForEnd();
  Set(myRow, Sub(myRow, IntConst(1)));  // Use zero-based numbering.

  // Tell each APE its column number.
  DeclareApeVarInit(myCol, Int, IntConst(0));
  DeclareCUVar(colNum, Int);
  CUFor(colNum, IntConst(1), IntConst(apeCols), IntConst(1));
  eApeC(apeGet, myCol, _, getWest);
  Set(myCol, Add(myCol, IntConst(1)));
  CUForEnd();
  Set(myCol, Sub(myCol, IntConst(1)));  // Use zero-based numbering.

  // Assign each APE a globally unique ID.
  DeclareApeVarInit(myID, Int,
		    Add(Mul(myRow, IntConst(apeCols)), myCol));
  TraceOneRegisterAllApes(myID);
}

// Emit all code to the kernel.
void emitAll()
{
  // Assign IDs to APEs.
  emitApeIDAssignment();

  // Halt the kernel.
  eCUC(cuHalt, _, _, _);
}

int main (int argc, char *argv[]) {
  //      {real | emulated}
  //      <trace>  see trace flags

  // process the command line arguments
  int argError = 0;
  int nextArg = 1;

  if (argc<= nextArg) argError = 1;
  else if (strcmp(argv[nextArg],"real")==0 ) {
    emulated = 0;
  } else if (strcmp(argv[nextArg],"emulated")==0 ) {
    emulated = 1;
  } else {
    printf("Machine argument not 'real' or 'emulated'.\n");
    argError = 1;
  }
  nextArg += 1;

  if (argc<= nextArg) argError = 1;
  else {
    traceFlags = atoi(argv[nextArg]);
    nextArg += 1;
  }

  if (argc > nextArg) {
    printf("Too many command line arguments.\n");
    argError = 1;
  }

  if (argError) {
    printf("  Command line arguments are:\n");
    printf("       <machine>        'real' or 'emulated'\n");
    printf("       <trace>          Translate | Emit | API | States | Instructions\n");
    exit(1);
  }

  // Initialize Singular arithmetic on CPU
  initSingularArithmetic ();

  // Create a machine
  chipRows = 1;
  chipCols = 1;
  apeRows = 48;
  apeCols = 44;
  scInitializeMachine ((emulated ? scEmulated : scRealMachine),
                       chipRows, chipCols, apeRows, apeCols,
                       traceFlags, 0 /* DDR */, 0 /* randomize */, 0 /* torus */);

  // Exit if S1 is still running.
  // (scInitializeMachine is supposed to completely reset the machine,
  // so this should not be able to happen, but current CU has a bug.)
  if (scReadCURunning() != 0) {
    printf("S1 is RUNNING AFTER RESET.  Terminating execution.\n");
    exit(1);
  }

  // Initialize the kernel-creating code.
  scNovaInit();
  scEmitLLKernelCreate();

  // Define a kernel.
  emitApeIDAssignment();

  // Emit the low-level translation of the high-level kernel instructions.
  scKernelTranslate();

  // Load, free, and start the low-level kernel.
  scLLKernelLoad (llKernel, 0);
  scLLKernelFree(llKernel);
  scLLKernelExecute(0);

  // Wait for the kernel to halt.
  scLLKernelWaitSignal();
  
  // Terminate the machine.
  scTerminateMachine();
}
