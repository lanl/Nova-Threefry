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
int apeRowsLog2;  // Rounded up

// Bits controling how much tracing to do, per scAcceleratorAPI.h
int traceFlags;

// Each of the following represent four 32-bit numbers stored as eight Ints.
Declare(counter_3fry);  /* Input: Loop counter */
Declare(key_3fry);      /* Input: Key (e.g., APE ID) */
Declare(random_3fry);   /* Output: Random numbers */
Declare(scratch_3fry);  /* Internal: Scratch space */

// Define the list of Threefry 32x4 rotation constants.
const int rot_32x4[] = {
  10, 26, 11, 21, 13, 27, 23,  5,  6, 20, 17, 11, 25, 10, 18, 20
};

// Emit code to add two 32-bit numbers.
void Add32Bits(scExpr sum_hi, scExpr sum_lo,
	       scExpr a_hi, scExpr a_lo,
	       scExpr b_hi, scExpr b_lo)
{
  // Add the low words.
  DeclareApeVar(sum_reg, Int)
  DeclareApeVarInit(a_reg, Int, a_lo)
  DeclareApeVarInit(b_reg, Int, b_lo)
  eApeC(apeAdd, sum_reg, a_reg, b_reg);
  Set(sum_lo, sum_reg);

  // Add the high words.
  Set(a_reg, a_hi);
  Set(b_reg, b_hi);
  eApeC(apeAddL, sum_reg, a_reg, b_reg);
  Set(sum_hi, sum_reg);
}

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
                    Add(Asl(myRow, IntConst(apeRowsLog2)), myCol));

  // Temporary
#ifdef XYZZY
  eControl(controlOpReserveApeReg, apeR0);  // Low
  eControl(controlOpReserveApeReg, apeR1);  // High
  DeclareApeVarInit(zero, Int, IntConst(0));
  DeclareApeVarInit(big, Int, IntConst(0xFFFF));
  eApeC(apeAdd, apeR0, big, IntConst(0x0001));
  eApeC(apeAddL, apeR1, zero, zero);
  TraceOneRegisterOneApe(apeR0, 0, 0);
  TraceOneRegisterOneApe(apeR1, 0, 0);
  eControl(controlOpReleaseApeReg, apeR1);
  eControl(controlOpReleaseApeReg, apeR0);
#else
  DeclareApeMemVector(dummy, Int, 8);
  Set(IndexVector(dummy, IntConst(2)), IntConst(0x0001));
  Set(IndexVector(dummy, IntConst(3)), IntConst(0xFFFF));
  Set(IndexVector(dummy, IntConst(4)), IntConst(0x0002));
  Set(IndexVector(dummy, IntConst(5)), IntConst(0xEEEE));
  Add32Bits(IndexVector(dummy, IntConst(0)), IndexVector(dummy, IntConst(1)),
	    IndexVector(dummy, IntConst(2)), IndexVector(dummy, IntConst(3)),
	    IndexVector(dummy, IntConst(4)), IndexVector(dummy, IntConst(5)));
  TraceOneRegisterOneApe(IndexVector(dummy, IntConst(0)), 0, 0);
  TraceOneRegisterOneApe(IndexVector(dummy, IntConst(1)), 0, 0);
#endif
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

  // Create a machine.
  chipRows = 1;
  chipCols = 1;
  apeRows = 48;
  apeCols = 44;
  apeRowsLog2 = 6;
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
