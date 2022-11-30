CFLAGS = -c -g -ansi -pedantic -Wall -std=gnu99 `pkg-config fuse --cflags --libs`
LDFLAGS = `pkg-config fuse --cflags --libs`

OBJDIR=obj_files
EXEDIR=exec_files

# Uncomment on of the following three lines to compile
SOURCES= disk_emu.c sfs_api.c sfs_api.h
# SOURCES= disk_emu.c sfs_mock_api.c sfs_test2.c sfs_mock_api.h
# SOURCES= disk_emu.c sfs_api.c sfs_test0.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c sfs_test1.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c sfs_test2.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c fuse_wrap_old.c sfs_api.h
# SOURCES= disk_emu.c sfs_api.c fuse_wrap_new.c sfs_api.h

OBJECTS=$(SOURCES:%.c=$(OBJDIR)/%.o)
EXECUTABLE=sfs

all: $(SOURCES) $(HEADERS) $(EXEDIR)/$(EXECUTABLE)

$(EXEDIR)/$(EXECUTABLE): $(OBJECTS)
	mkdir -p $(@D)
	gcc $(OBJECTS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: %.c
	mkdir -p $(@D)
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o *~ $(EXEDIR)/$(EXECUTABLE)
