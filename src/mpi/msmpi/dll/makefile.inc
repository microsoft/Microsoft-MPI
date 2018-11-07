
$(OBJ_PATH)\$(O)\typeinfo\binplaced.txt: $(OBJ_PATH)\$(O)\typeinfo\$(TARGETPDB).pdb

$(OBJ_PATH)\$(O)\msmpidbg.def: $(MPI_SRC_ROOT)\msmpi\dll\msmpi.def $(MPI_SRC_ROOT)\msmpi\dll\dbgsupport.def
   copy $(MPI_SRC_ROOT)\msmpi\dll\msmpi.def $(OBJ_PATH)\$(O)\msmpidbg.def
   type $(MPI_SRC_ROOT)\msmpi\dll\dbgsupport.def >> $(OBJ_PATH)\$(O)\msmpidbg.def
   
clean:
	erase $(OBJ_PATH)\$(O)\msmpidbg.def