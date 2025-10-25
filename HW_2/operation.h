// operations.h
#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stdio.h>
#include "machine_state.h"



void BINOP_execute(FILE *f, MachineState &ms, int op);
void BEGIN_execute(FILE *f, MachineState &ms);
void CONST_execute(FILE *f, MachineState &ms);
void STORE_G_execute(FILE *f, MachineState &ms);
void LD_G_execute(FILE *f, MachineState &ms);
void DROP_execute(FILE *f, MachineState &ms);
void CALL_execute(FILE *f, MachineState &ms);

#endif