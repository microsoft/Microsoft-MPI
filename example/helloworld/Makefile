ld=link
cc=cl

cflags=/I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include"
ldflags=/libpath:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64"

libs=msmpi.lib

output=affinity.exe
objs=affinity.obj

all: $(objs)
	$(ld) $(libs) $(ldflags) -out:$(output) $(objs)

.cpp.obj:
	$(cc) $(cflags) /c $*.cpp
clean:
	@del /Q $(objs)
	@del /Q $(output)
